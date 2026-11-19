#include "AssetServerSubsystem.h"

#include "AssetSha256.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogAssetServer, Log, All);

void UAssetServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FString Url;
	if (FParse::Value(FCommandLine::Get(), TEXT("AssetServerUrl="), Url) && !Url.IsEmpty())
	{
		BaseUrl = Url;
	}
	BaseUrl.RemoveFromEnd(TEXT("/"));
	UE_LOG(LogAssetServer, Log, TEXT("Asset server: %s"), *BaseUrl);
}

FString UAssetServerSubsystem::GetCacheDir()
{
	return FPaths::ProjectSavedDir() / TEXT("AssetCache");
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> UAssetServerSubsystem::MakeRequest(const FString& Verb, const FString& Path) const
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetVerb(Verb);
	Req->SetURL(BaseUrl + Path);
	Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Req->SetActivityTimeout(60.f); // fail if no bytes move for 60 s (large files may take longer overall)
	return Req;
}

FAssetServerResult UAssetServerSubsystem::MakeResult(FHttpResponsePtr Resp, bool bConnected)
{
	FAssetServerResult Result;
	if (!bConnected || !Resp.IsValid() || Resp->GetResponseCode() <= 0)
	{
		Result.Error = TEXT("cannot reach the asset server");
		return Result;
	}
	Result.HttpCode = Resp->GetResponseCode();
	Result.bOk = EHttpResponseCodes::IsOk(Result.HttpCode);
	if (!Result.bOk)
	{
		// FastAPI errors are JSON: {"detail": "..."}
		FString Detail = Resp->GetContentAsString();
		TSharedPtr<FJsonObject> Obj;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Detail), Obj) && Obj.IsValid())
		{
			Obj->TryGetStringField(TEXT("detail"), Detail);
		}
		Result.Error = FString::Printf(TEXT("HTTP %d: %s"), Result.HttpCode, *Detail.Left(300));
	}
	return Result;
}

bool UAssetServerSubsystem::ParseItem(const TSharedPtr<FJsonObject>& Obj, FAssetServerItem& Out)
{
	if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("id"), Out.Id))
	{
		return false;
	}
	Obj->TryGetStringField(TEXT("name"), Out.Name);
	Obj->TryGetStringField(TEXT("ext"), Out.Ext);
	Obj->TryGetNumberField(TEXT("size"), Out.Size);
	Obj->TryGetStringField(TEXT("sha256"), Out.Sha256);
	Obj->TryGetStringField(TEXT("stored_sha256"), Out.StoredSha256); // null unless verify=true
	Obj->TryGetBoolField(TEXT("intact"), Out.bIntact);
	Obj->TryGetStringField(TEXT("created_at"), Out.CreatedAt);
	Obj->TryGetStringField(TEXT("updated_at"), Out.UpdatedAt);
	Obj->TryGetStringField(TEXT("download_url"), Out.DownloadUrl);
	return true;
}

// ---------------------------------------------------------------- C++ API
void UAssetServerSubsystem::UploadFile(const FString& LocalFilePath, const FString& UploadName, const FString& Sha256, const FString& ExistingId, FAssetItemCallback Callback)
{
	const FString Query = TEXT("?name=") + FGenericPlatformHttp::UrlEncode(UploadName);
	auto Req = ExistingId.IsEmpty()
		? MakeRequest(TEXT("POST"), TEXT("/assets/raw") + Query)
		: MakeRequest(TEXT("PUT"), TEXT("/assets/") + FGenericPlatformHttp::UrlEncode(ExistingId) + TEXT("/raw") + Query);
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
	if (!Sha256.IsEmpty())
	{
		Req->SetHeader(TEXT("X-Content-SHA256"), Sha256);
	}

	// Body = the file itself, streamed from disk (no LoadFileToArray, no base64)
	if (!Req->SetContentAsStreamedFile(LocalFilePath))
	{
		FAssetServerResult Result;
		Result.HttpCode = -1;
		Result.Error = TEXT("cannot open file: ") + LocalFilePath;
		Callback(Result, FAssetServerItem());
		return;
	}

	TWeakObjectPtr<UAssetServerSubsystem> WeakThis(this);
	const int64 Total = IFileManager::Get().FileSize(*LocalFilePath);
	Req->OnRequestProgress64().BindLambda([WeakThis, Total](FHttpRequestPtr, uint64 Sent, uint64)
	{
		if (WeakThis.IsValid()) { WeakThis->OnProgress.Broadcast(TEXT("upload"), (int64)Sent, Total); }
	});
	Req->OnProcessRequestComplete().BindLambda([Callback](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
	{
		FAssetServerResult Result = MakeResult(Resp, bConnected);
		FAssetServerItem Item;
		TSharedPtr<FJsonObject> Obj;
		if (Result.bOk && !(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Resp->GetContentAsString()), Obj) && ParseItem(Obj, Item)))
		{
			Result.bOk = false;
			Result.Error = TEXT("bad JSON response");
		}
		Callback(Result, Item);
	});
	Req->ProcessRequest();
}

void UAssetServerSubsystem::GetAssetInfo(const FString& AssetId, bool bVerify, FAssetItemCallback Callback)
{
	auto Req = MakeRequest(TEXT("GET"), TEXT("/assets/") + FGenericPlatformHttp::UrlEncode(AssetId) + (bVerify ? TEXT("?verify=true") : TEXT("")));
	Req->OnProcessRequestComplete().BindLambda([Callback](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
	{
		FAssetServerResult Result = MakeResult(Resp, bConnected);
		FAssetServerItem Item;
		TSharedPtr<FJsonObject> Obj;
		if (Result.bOk && !(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Resp->GetContentAsString()), Obj) && ParseItem(Obj, Item)))
		{
			Result.bOk = false;
			Result.Error = TEXT("bad JSON response");
		}
		Callback(Result, Item);
	});
	Req->ProcessRequest();
}

void UAssetServerSubsystem::DownloadToFile(const FString& AssetId, const FString& DestPath, FAssetResultCallback Callback)
{
	// The body is binary: read it with GetContent() (TArray<uint8>), never GetContentAsString().
	auto Req = MakeRequest(TEXT("GET"), TEXT("/assets/") + FGenericPlatformHttp::UrlEncode(AssetId) + TEXT("/download"));
	Req->SetHeader(TEXT("Accept"), TEXT("application/octet-stream"));

	TWeakObjectPtr<UAssetServerSubsystem> WeakThis(this);
	Req->OnRequestProgress64().BindLambda([WeakThis](FHttpRequestPtr, uint64, uint64 Received)
	{
		if (WeakThis.IsValid()) { WeakThis->OnProgress.Broadcast(TEXT("download"), (int64)Received, 0); }
	});
	Req->OnProcessRequestComplete().BindLambda([Callback, DestPath](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
	{
		FAssetServerResult Result = MakeResult(Resp, bConnected);
		if (Result.bOk)
		{
			// write to .part then rename, so a crash never leaves a half-written file under the final name
			const FString TmpPath = DestPath + TEXT(".part");
			IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
			PF.CreateDirectoryTree(*FPaths::GetPath(DestPath));
			if (!FFileHelper::SaveArrayToFile(Resp->GetContent(), *TmpPath) || !(PF.DeleteFile(*DestPath), PF.MoveFile(*DestPath, *TmpPath)))
			{
				PF.DeleteFile(*TmpPath);
				Result.bOk = false;
				Result.HttpCode = -1;
				Result.Error = TEXT("cannot write ") + DestPath;
			}
		}
		Callback(Result);
	});
	Req->ProcessRequest();
}

void UAssetServerSubsystem::DeleteAsset(const FString& AssetId, FAssetResultCallback Callback)
{
	auto Req = MakeRequest(TEXT("DELETE"), TEXT("/assets/") + FGenericPlatformHttp::UrlEncode(AssetId));
	Req->OnProcessRequestComplete().BindLambda([Callback](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
	{
		Callback(MakeResult(Resp, bConnected));
	});
	Req->ProcessRequest();
}

void UAssetServerSubsystem::ListAssets(const FString& NameFilter, TFunction<void(const FAssetServerResult&, const TArray<FAssetServerItem>&)> Callback)
{
	FString Path = TEXT("/assets?limit=1000");
	if (!NameFilter.IsEmpty())
	{
		Path += TEXT("&q=") + FGenericPlatformHttp::UrlEncode(NameFilter);
	}
	auto Req = MakeRequest(TEXT("GET"), Path);
	Req->OnProcessRequestComplete().BindLambda([Callback](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
	{
		FAssetServerResult Result = MakeResult(Resp, bConnected);
		TArray<FAssetServerItem> Items;
		TArray<TSharedPtr<FJsonValue>> Arr;
		if (Result.bOk)
		{
			if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Resp->GetContentAsString()), Arr))
			{
				for (const TSharedPtr<FJsonValue>& V : Arr)
				{
					FAssetServerItem Item;
					if (ParseItem(V->AsObject(), Item)) { Items.Add(MoveTemp(Item)); }
				}
			}
			else
			{
				Result.bOk = false;
				Result.Error = TEXT("bad JSON response");
			}
		}
		Callback(Result, Items);
	});
	Req->ProcessRequest();
}

// ---------------------------------------------------------------- Blueprint wrappers
void UAssetServerSubsystem::BP_UploadFile(const FString& LocalFilePath)
{
	TWeakObjectPtr<UAssetServerSubsystem> WeakThis(this);
	Async(EAsyncExecution::ThreadPool, [WeakThis, LocalFilePath]()
	{
		FString Sha;
		const bool bHashed = FAssetSha256::HashFile(LocalFilePath, Sha);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, LocalFilePath, Sha, bHashed]()
		{
			if (!WeakThis.IsValid()) { return; }
			if (!bHashed)
			{
				WeakThis->OnUploaded.Broadcast(false, FAssetServerItem(), TEXT("cannot read ") + LocalFilePath);
				return;
			}
			WeakThis->UploadFile(LocalFilePath, FPaths::GetCleanFilename(LocalFilePath), Sha, FString(),
				[WeakThis](const FAssetServerResult& Result, const FAssetServerItem& Item)
				{
					if (WeakThis.IsValid()) { WeakThis->OnUploaded.Broadcast(Result.bOk, Item, Result.Error); }
				});
		});
	});
}

void UAssetServerSubsystem::BP_ListAssets(const FString& NameFilter)
{
	TWeakObjectPtr<UAssetServerSubsystem> WeakThis(this);
	ListAssets(NameFilter, [WeakThis](const FAssetServerResult& Result, const TArray<FAssetServerItem>& Items)
	{
		if (WeakThis.IsValid()) { WeakThis->OnListed.Broadcast(Result.bOk, Items, Result.Error); }
	});
}

void UAssetServerSubsystem::BP_DownloadAsset(const FString& AssetId)
{
	TWeakObjectPtr<UAssetServerSubsystem> WeakThis(this);
	GetAssetInfo(AssetId, false, [WeakThis, AssetId](const FAssetServerResult& Result, const FAssetServerItem& Item)
	{
		if (!WeakThis.IsValid()) { return; }
		if (!Result.bOk)
		{
			WeakThis->OnDownloaded.Broadcast(false, FString(), Result.Error);
			return;
		}
		const FString CachePath = GetCacheDir() / (Item.Sha256 + Item.Ext);
		FString CachedSha;
		if (FAssetSha256::HashFile(CachePath, CachedSha) && CachedSha == Item.Sha256)
		{
			WeakThis->OnDownloaded.Broadcast(true, CachePath, FString()); // cache hit, no download
			return;
		}
		WeakThis->DownloadToFile(AssetId, CachePath, [WeakThis, CachePath, Item](const FAssetServerResult& DlResult)
		{
			if (!WeakThis.IsValid()) { return; }
			FString Sha;
			if (DlResult.bOk && (!FAssetSha256::HashFile(CachePath, Sha) || Sha != Item.Sha256))
			{
				IFileManager::Get().Delete(*CachePath);
				WeakThis->OnDownloaded.Broadcast(false, FString(), FString::Printf(TEXT("hash mismatch after download (%s != %s)"), *Sha, *Item.Sha256));
				return;
			}
			WeakThis->OnDownloaded.Broadcast(DlResult.bOk, DlResult.bOk ? CachePath : FString(), DlResult.Error);
		});
	});
}

void UAssetServerSubsystem::BP_DeleteAsset(const FString& AssetId)
{
	TWeakObjectPtr<UAssetServerSubsystem> WeakThis(this);
	DeleteAsset(AssetId, [WeakThis, AssetId](const FAssetServerResult& Result)
	{
		if (WeakThis.IsValid()) { WeakThis->OnDeleted.Broadcast(Result.bOk, AssetId, Result.Error); }
	});
}

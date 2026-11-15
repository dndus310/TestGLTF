#include "AssetServerSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

namespace
{
	bool IsOk(FHttpResponsePtr Resp, bool bConnected)
	{
		return bConnected && Resp.IsValid() && EHttpResponseCodes::IsOk(Resp->GetResponseCode());
	}
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

FString UAssetServerSubsystem::ErrorText(FHttpResponsePtr Resp, bool bConnected)
{
	if (!bConnected || !Resp.IsValid())
	{
		return TEXT("connection failed (server down, wrong BaseUrl, firewall?)");
	}
	// FastAPI errors are JSON: {"detail": "..."}
	TSharedPtr<FJsonObject> Obj;
	FString Detail = Resp->GetContentAsString();
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Detail), Obj) && Obj.IsValid())
	{
		Obj->TryGetStringField(TEXT("detail"), Detail);
	}
	return FString::Printf(TEXT("HTTP %d: %s"), Resp->GetResponseCode(), *Detail);
}

bool UAssetServerSubsystem::ParseItem(const TSharedPtr<FJsonObject>& Obj, FAssetServerItem& Out)
{
	if (!Obj.IsValid() || !Obj->TryGetStringField(TEXT("id"), Out.Id))
	{
		return false;
	}
	Obj->TryGetStringField(TEXT("name"), Out.Name);
	Obj->TryGetNumberField(TEXT("size"), Out.Size);
	Obj->TryGetStringField(TEXT("sha256"), Out.Sha256);
	Obj->TryGetStringField(TEXT("created_at"), Out.CreatedAt);
	Obj->TryGetStringField(TEXT("download_url"), Out.DownloadUrl);
	return true;
}

// ------------------------------------------------------------------ Upload
void UAssetServerSubsystem::UploadFile(const FString& LocalFilePath)
{
	const FString Name = FPaths::GetCleanFilename(LocalFilePath);
	auto Req = MakeRequest(TEXT("POST"), TEXT("/assets/raw?name=") + FGenericPlatformHttp::UrlEncode(Name));
	Req->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));

	// Body = the file itself, streamed from disk (no LoadFileToArray, no base64)
	if (!Req->SetContentAsStreamedFile(LocalFilePath))
	{
		OnUploaded.Broadcast(false, FAssetServerItem(), TEXT("cannot open file: ") + LocalFilePath);
		return;
	}

	TWeakObjectPtr<UAssetServerSubsystem> WeakThis(this);
	const int64 Total = IFileManager::Get().FileSize(*LocalFilePath);
	Req->OnRequestProgress64().BindLambda([WeakThis, Total](FHttpRequestPtr, uint64 Sent, uint64)
	{
		if (WeakThis.IsValid()) { WeakThis->OnProgress.Broadcast(TEXT("upload"), (int64)Sent, Total); }
	});
	Req->OnProcessRequestComplete().BindLambda([WeakThis](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
	{
		if (!WeakThis.IsValid()) { return; }
		FAssetServerItem Item;
		if (!IsOk(Resp, bConnected))
		{
			WeakThis->OnUploaded.Broadcast(false, Item, ErrorText(Resp, bConnected));
			return;
		}
		// Response is JSON text -> FJsonObject
		TSharedPtr<FJsonObject> Obj;
		const bool bParsed = FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Resp->GetContentAsString()), Obj)
			&& ParseItem(Obj, Item);
		WeakThis->OnUploaded.Broadcast(bParsed, Item, bParsed ? FString() : TEXT("bad JSON response"));
	});
	Req->ProcessRequest();
}

// ------------------------------------------------------------------ List
void UAssetServerSubsystem::ListAssets(const FString& NameFilter)
{
	FString Path = TEXT("/assets");
	if (!NameFilter.IsEmpty())
	{
		Path += TEXT("?q=") + FGenericPlatformHttp::UrlEncode(NameFilter);
	}
	auto Req = MakeRequest(TEXT("GET"), Path);

	TWeakObjectPtr<UAssetServerSubsystem> WeakThis(this);
	Req->OnProcessRequestComplete().BindLambda([WeakThis](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
	{
		if (!WeakThis.IsValid()) { return; }
		TArray<FAssetServerItem> Items;
		if (!IsOk(Resp, bConnected))
		{
			WeakThis->OnListed.Broadcast(false, Items, ErrorText(Resp, bConnected));
			return;
		}
		TArray<TSharedPtr<FJsonValue>> Arr;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Resp->GetContentAsString()), Arr))
		{
			WeakThis->OnListed.Broadcast(false, Items, TEXT("bad JSON response"));
			return;
		}
		for (const TSharedPtr<FJsonValue>& V : Arr)
		{
			FAssetServerItem Item;
			if (ParseItem(V->AsObject(), Item)) { Items.Add(MoveTemp(Item)); }
		}
		WeakThis->OnListed.Broadcast(true, Items, FString());
	});
	Req->ProcessRequest();
}

// ------------------------------------------------------------------ Download
void UAssetServerSubsystem::DownloadAsset(const FString& AssetId)
{
	// Step 1: metadata as JSON (to get sha256 for the cache key)
	auto Req = MakeRequest(TEXT("GET"), TEXT("/assets/") + FGenericPlatformHttp::UrlEncode(AssetId));

	TWeakObjectPtr<UAssetServerSubsystem> WeakThis(this);
	Req->OnProcessRequestComplete().BindLambda([WeakThis](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
	{
		if (!WeakThis.IsValid()) { return; }
		if (!IsOk(Resp, bConnected))
		{
			WeakThis->OnDownloaded.Broadcast(false, FString(), ErrorText(Resp, bConnected));
			return;
		}
		TSharedPtr<FJsonObject> Obj;
		FAssetServerItem Item;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Resp->GetContentAsString()), Obj) || !ParseItem(Obj, Item))
		{
			WeakThis->OnDownloaded.Broadcast(false, FString(), TEXT("bad JSON response"));
			return;
		}

		const FString CachePath = GetCacheDir() / (Item.Sha256 + FPaths::GetExtension(Item.Name, /*bIncludeDot*/ true));
		if (IFileManager::Get().FileSize(*CachePath) == Item.Size)
		{
			WeakThis->OnDownloaded.Broadcast(true, CachePath, FString()); // cache hit, no download
			return;
		}
		WeakThis->StartBinaryDownload(Item);
	});
	Req->ProcessRequest();
}

void UAssetServerSubsystem::StartBinaryDownload(const FAssetServerItem& Item)
{
	// Step 2: the file as raw bytes. NOTE: use GetContent() (TArray<uint8>), never GetContentAsString()
	// - converting binary to FString corrupts it.
	auto Req = MakeRequest(TEXT("GET"), Item.DownloadUrl);
	Req->SetHeader(TEXT("Accept"), TEXT("application/octet-stream"));

	TWeakObjectPtr<UAssetServerSubsystem> WeakThis(this);
	Req->OnRequestProgress64().BindLambda([WeakThis, Total = Item.Size](FHttpRequestPtr, uint64, uint64 Received)
	{
		if (WeakThis.IsValid()) { WeakThis->OnProgress.Broadcast(TEXT("download"), (int64)Received, Total); }
	});
	Req->OnProcessRequestComplete().BindLambda([WeakThis, Item](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
	{
		if (!WeakThis.IsValid()) { return; }
		if (!IsOk(Resp, bConnected))
		{
			WeakThis->OnDownloaded.Broadcast(false, FString(), ErrorText(Resp, bConnected));
			return;
		}
		const TArray<uint8>& Bytes = Resp->GetContent();
		if (Bytes.Num() != Item.Size)
		{
			WeakThis->OnDownloaded.Broadcast(false, FString(),
				FString::Printf(TEXT("size mismatch: got %d, expected %lld"), Bytes.Num(), Item.Size));
			return;
		}
		const FString CachePath = GetCacheDir() / (Item.Sha256 + FPaths::GetExtension(Item.Name, true));
		const FString TmpPath = CachePath + TEXT(".part");
		IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
		PF.CreateDirectoryTree(*GetCacheDir());
		// write to .part then rename, so a crash never leaves a half-written file under the final name
		if (!FFileHelper::SaveArrayToFile(Bytes, *TmpPath) || !(PF.DeleteFile(*CachePath), PF.MoveFile(*CachePath, *TmpPath)))
		{
			WeakThis->OnDownloaded.Broadcast(false, FString(), TEXT("cannot write ") + CachePath);
			return;
		}
		WeakThis->OnDownloaded.Broadcast(true, CachePath, FString());
	});
	Req->ProcessRequest();
}

// ------------------------------------------------------------------ Delete
void UAssetServerSubsystem::DeleteAsset(const FString& AssetId)
{
	auto Req = MakeRequest(TEXT("DELETE"), TEXT("/assets/") + FGenericPlatformHttp::UrlEncode(AssetId));

	TWeakObjectPtr<UAssetServerSubsystem> WeakThis(this);
	Req->OnProcessRequestComplete().BindLambda([WeakThis, AssetId](FHttpRequestPtr, FHttpResponsePtr Resp, bool bConnected)
	{
		if (!WeakThis.IsValid()) { return; }
		const bool bOk = IsOk(Resp, bConnected);
		WeakThis->OnDeleted.Broadcast(bOk, AssetId, bOk ? FString() : ErrorText(Resp, bConnected));
	});
	Req->ProcessRequest();
}

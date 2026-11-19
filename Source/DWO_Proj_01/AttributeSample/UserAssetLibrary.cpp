// Attribute Sample - user asset manifest + local store + server sync (see UserAssetLibrary.h for the rules)

#include "UserAssetLibrary.h"

#include "AssetServerSubsystem.h"
#include "AssetSha256.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogUserAssets, Log, All);

namespace UserAssetJson
{
	FString NowIso() { return FDateTime::UtcNow().ToIso8601(); }
	FString Short(const FString& Sha) { return Sha.Left(8); }

	FString StateToString(EUserAssetSyncState State)
	{
		switch (State)
		{
		case EUserAssetSyncState::Synced:        return TEXT("Synced");
		case EUserAssetSyncState::PendingDelete: return TEXT("PendingDelete");
		case EUserAssetSyncState::Error:         return TEXT("Error");
		default:                                 return TEXT("PendingUpload");
		}
	}

	EUserAssetSyncState StateFromString(const FString& Value)
	{
		if (Value == TEXT("Synced"))        { return EUserAssetSyncState::Synced; }
		if (Value == TEXT("PendingDelete")) { return EUserAssetSyncState::PendingDelete; }
		if (Value == TEXT("Error"))         { return EUserAssetSyncState::Error; }
		return EUserAssetSyncState::PendingUpload;
	}

	FString LayerToString(EAttributeAssetType Layer) { return Layer == EAttributeAssetType::Load ? TEXT("Load") : TEXT("Building"); }
	EAttributeAssetType LayerFromString(const FString& Value) { return Value == TEXT("Load") ? EAttributeAssetType::Load : EAttributeAssetType::Building; }
}

using namespace UserAssetJson;

// ---------------------------------------------------------------- setup / manifest
void UUserAssetLibrary::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Server = Collection.InitializeDependency<UAssetServerSubsystem>();
	LoadManifest();
	// Pending uploads / deletes from the last session are retried by SyncAll (UAttributeSampleSubsystem calls it on start)
}

FString UUserAssetLibrary::GetLibraryDir()
{
	return FPaths::ProjectSavedDir() / TEXT("UserAssets");
}

FString UUserAssetLibrary::GetManifestPath() const
{
	return GetLibraryDir() / TEXT("manifest.json");
}

FString UUserAssetLibrary::GetLocalPath(const FUserAssetEntry& Entry) const
{
	return GetLibraryDir() / TEXT("Files") / (Entry.LocalId + Entry.Ext);
}

FString UUserAssetLibrary::GetTempPath(const FString& LocalId, const FString& Suffix, const FString& Ext) const
{
	return GetLibraryDir() / TEXT("Temp") / FString::Printf(TEXT("%s.%s%s"), *LocalId, *Suffix, *Ext);
}

const FUserAssetEntry* UUserAssetLibrary::FindEntry(const FString& LocalId) const
{
	return Entries.FindByPredicate([&LocalId](const FUserAssetEntry& E) { return E.LocalId == LocalId; });
}

FUserAssetEntry* UUserAssetLibrary::Find(const FString& LocalId)
{
	return Entries.FindByPredicate([&LocalId](const FUserAssetEntry& E) { return E.LocalId == LocalId; });
}

void UUserAssetLibrary::LoadManifest()
{
	Entries.Reset();
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *GetManifestPath()))
	{
		return; // first run
	}

	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid() || !Root->TryGetArrayField(TEXT("assets"), Assets))
	{
		// Keep the unreadable file for inspection instead of overwriting it with an empty list
		const FString Backup = GetManifestPath() + TEXT(".corrupt-") + FDateTime::UtcNow().ToString(TEXT("%Y%m%d%H%M%S"));
		IFileManager::Get().Move(*Backup, *GetManifestPath());
		UE_LOG(LogUserAssets, Error, TEXT("manifest.json is not valid JSON; moved to %s"), *Backup);
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *Assets)
	{
		const TSharedPtr<FJsonObject> O = Value->AsObject();
		FUserAssetEntry E;
		if (!O.IsValid() || !O->TryGetStringField(TEXT("localId"), E.LocalId))
		{
			continue;
		}
		FString Text2;
		O->TryGetStringField(TEXT("name"), E.Name);
		O->TryGetStringField(TEXT("ext"), E.Ext);
		if (O->TryGetStringField(TEXT("layer"), Text2)) { E.Layer = LayerFromString(Text2); }
		O->TryGetStringField(TEXT("sha256"), E.Sha256);
		O->TryGetNumberField(TEXT("size"), E.Size);
		O->TryGetStringField(TEXT("serverId"), E.ServerId);
		if (O->TryGetStringField(TEXT("state"), Text2)) { E.State = StateFromString(Text2); }
		O->TryGetStringField(TEXT("lastError"), E.LastError);
		O->TryGetStringField(TEXT("updatedAt"), E.UpdatedAt);
		O->TryGetStringField(TEXT("lastSyncedAt"), E.LastSyncedAt);
		Entries.Add(MoveTemp(E));
	}
	UE_LOG(LogUserAssets, Log, TEXT("Loaded %d user asset(s) from %s"), Entries.Num(), *GetManifestPath());
}

bool UUserAssetLibrary::SaveManifest()
{
	TArray<TSharedPtr<FJsonValue>> Assets;
	for (const FUserAssetEntry& E : Entries)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("localId"), E.LocalId);
		O->SetStringField(TEXT("name"), E.Name);
		O->SetStringField(TEXT("ext"), E.Ext);
		O->SetStringField(TEXT("layer"), LayerToString(E.Layer));
		O->SetStringField(TEXT("sha256"), E.Sha256);
		O->SetNumberField(TEXT("size"), double(E.Size));
		O->SetStringField(TEXT("serverId"), E.ServerId);
		O->SetStringField(TEXT("state"), StateToString(E.State));
		O->SetStringField(TEXT("lastError"), E.LastError);
		O->SetStringField(TEXT("updatedAt"), E.UpdatedAt);
		O->SetStringField(TEXT("lastSyncedAt"), E.LastSyncedAt);
		Assets.Add(MakeShared<FJsonValueObject>(O));
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("version"), 1);
	Root->SetStringField(TEXT("server"), Server ? Server->BaseUrl : FString());
	Root->SetArrayField(TEXT("assets"), Assets);

	FString Text;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));

	// write-then-rename so a crash never leaves a truncated manifest
	const FString Path = GetManifestPath();
	const FString Tmp = Path + TEXT(".tmp");
	IFileManager::Get().MakeDirectory(*GetLibraryDir(), true);
	if (!FFileHelper::SaveStringToFile(Text, *Tmp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) || !IFileManager::Get().Move(*Path, *Tmp, true))
	{
		UE_LOG(LogUserAssets, Error, TEXT("Cannot write %s"), *Path);
		return false;
	}
	return true;
}

void UUserAssetLibrary::SetState(const FString& LocalId, EUserAssetSyncState State, const FString& Error)
{
	if (FUserAssetEntry* E = Find(LocalId))
	{
		E->State = State;
		E->LastError = Error;
		if (State == EUserAssetSyncState::Synced)
		{
			E->LastSyncedAt = NowIso();
		}
		SaveManifest();
	}
}

void UUserAssetLibrary::Status(const FString& Message, bool bIsError)
{
	if (bIsError)
	{
		UE_LOG(LogUserAssets, Warning, TEXT("%s"), *Message);
	}
	else
	{
		UE_LOG(LogUserAssets, Log, TEXT("%s"), *Message);
	}
	OnSyncStatus.Broadcast(Message, bIsError);
}

void UUserAssetLibrary::HashFileAsync(const FString& Path, TFunction<void(bool, const FString&, int64)> Callback)
{
	TWeakObjectPtr<UUserAssetLibrary> WeakThis(this);
	Async(EAsyncExecution::ThreadPool, [WeakThis, Path, Callback = MoveTemp(Callback)]() mutable
	{
		FString Sha;
		int64 Size = 0;
		const bool bOk = FAssetSha256::HashFile(Path, Sha, &Size);
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bOk, Sha, Size, Callback = MoveTemp(Callback)]()
		{
			if (WeakThis.IsValid())
			{
				Callback(bOk, Sha, Size);
			}
		});
	});
}

// ---------------------------------------------------------------- public changes
bool UUserAssetLibrary::AddFile(const FString& SourcePath, EAttributeAssetType Layer, FUserAssetEntry& OutEntry, FString& OutError)
{
	FUserAssetEntry E;
	E.LocalId = FGuid::NewGuid().ToString(EGuidFormats::DigitsLower);
	E.Name = FPaths::GetBaseFilename(SourcePath);
	E.Ext = TEXT(".") + FPaths::GetExtension(SourcePath).ToLower();
	E.Layer = Layer;
	E.Size = IFileManager::Get().FileSize(*SourcePath);
	E.State = EUserAssetSyncState::PendingUpload;
	E.UpdatedAt = NowIso();

	const FString Dest = GetLocalPath(E);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Dest), true);
	if (IFileManager::Get().Copy(*Dest, *SourcePath) != COPY_OK)
	{
		OutError = TEXT("cannot copy the file into ") + FPaths::GetPath(Dest);
		return false;
	}

	Entries.Add(E);
	if (!SaveManifest())
	{
		OutError = TEXT("cannot write ") + GetManifestPath();
		// keep the entry in memory; it is written with the next change
	}
	OutEntry = E;
	UE_LOG(LogUserAssets, Log, TEXT("Added '%s' (%s) -> %s"), *E.Name, *E.LocalId, *Dest);
	QueueSync(E.LocalId);
	return true;
}

bool UUserAssetLibrary::RemoveAsset(const FString& LocalId)
{
	FUserAssetEntry* E = Find(LocalId);
	if (!E)
	{
		return false;
	}

	IFileManager::Get().Delete(*GetLocalPath(*E), false, true, true);
	const FString Name = E->Name;
	if (E->ServerId.IsEmpty())
	{
		Entries.RemoveAll([&LocalId](const FUserAssetEntry& X) { return X.LocalId == LocalId; });
		SaveManifest();
		Status(FString::Printf(TEXT("Deleted '%s' (it was not on the server)."), *Name));
		return true;
	}

	E->State = EUserAssetSyncState::PendingDelete;
	E->LastError.Reset();
	E->UpdatedAt = NowIso();
	SaveManifest();
	QueueSync(LocalId);
	return true;
}

void UUserAssetLibrary::SyncAll()
{
	for (const FUserAssetEntry& E : Entries)
	{
		QueueSync(E.LocalId);
	}
}

// ---------------------------------------------------------------- queue
void UUserAssetLibrary::QueueSync(const FString& LocalId)
{
	SyncQueue.AddUnique(LocalId);
	if (!bSyncRunning)
	{
		bSyncRunning = true;
		BatchSynced = BatchErrors = 0;
		ProcessNext();
	}
}

void UUserAssetLibrary::ProcessNext()
{
	if (SyncQueue.IsEmpty())
	{
		bSyncRunning = false;
		if (BatchErrors > 0)
		{
			Status(FString::Printf(TEXT("Server sync finished: %d ok, %d error(s). See Saved/UserAssets/manifest.json."), BatchSynced, BatchErrors), true);
		}
		else if (BatchSynced > 0)
		{
			Status(FString::Printf(TEXT("Server sync finished: %d operation(s) ok."), BatchSynced));
		}
		return;
	}

	const FString LocalId = SyncQueue[0];
	SyncQueue.RemoveAt(0);
	TWeakObjectPtr<UUserAssetLibrary> WeakThis(this);
	SyncEntry(LocalId, [WeakThis]() { if (WeakThis.IsValid()) { WeakThis->ProcessNext(); } });
}

void UUserAssetLibrary::StopOffline(const FAssetServerResult& Result)
{
	SyncQueue.Reset();
	bSyncRunning = false;
	const int32 Pending = Entries.FilterByPredicate([](const FUserAssetEntry& E)
	{
		return E.State == EUserAssetSyncState::PendingUpload || E.State == EUserAssetSyncState::PendingDelete;
	}).Num();
	Status(FString::Printf(TEXT("Asset server offline (%s). Saved locally; %d change(s) will be sent on the next sync."), *Result.Error, Pending), true);
}

// ---------------------------------------------------------------- steps
void UUserAssetLibrary::SyncEntry(const FString& LocalId, FStepDone Done)
{
	FUserAssetEntry* E = Find(LocalId);
	if (!E || !Server)
	{
		Done();
		return;
	}

	if (E->State == EUserAssetSyncState::PendingDelete)
	{
		StepDelete(LocalId, Done);
		return;
	}

	const FString LocalPath = GetLocalPath(*E);
	if (!IFileManager::Get().FileExists(*LocalPath))
	{
		if (E->ServerId.IsEmpty())
		{
			SetState(LocalId, EUserAssetSyncState::Error, TEXT("local file missing and never uploaded"));
			++BatchErrors;
			Done();
			return;
		}
		StepRestoreLocal(LocalId, E->Sha256, true, Done);
		return;
	}

	TWeakObjectPtr<UUserAssetLibrary> WeakThis(this);
	HashFileAsync(LocalPath, [WeakThis, LocalId, Done](bool bOk, const FString& LocalSha, int64 Size)
	{
		UUserAssetLibrary* Self = WeakThis.Get();
		FUserAssetEntry* E = Self ? Self->Find(LocalId) : nullptr;
		if (!E)
		{
			Done();
			return;
		}
		if (!bOk)
		{
			Self->SetState(LocalId, EUserAssetSyncState::Error, TEXT("cannot read the local file"));
			++Self->BatchErrors;
			Done();
			return;
		}

		if (E->Sha256.IsEmpty())
		{
			// First check after import: this is the hash we record
			E->Sha256 = LocalSha;
			E->Size = Size;
			Self->SaveManifest();
		}

		if (LocalSha == E->Sha256)
		{
			Self->StepCompareWithServer(LocalId, LocalSha, Done);
			return;
		}

		// The local file no longer matches the manifest
		UE_LOG(LogUserAssets, Warning, TEXT("'%s': local file hash %s != recorded %s"), *E->Name, *Short(LocalSha), *Short(E->Sha256));
		auto AdoptLocal = [WeakThis, LocalId, LocalSha, Size, Done]()
		{
			UUserAssetLibrary* S = WeakThis.Get();
			FUserAssetEntry* X = S ? S->Find(LocalId) : nullptr;
			if (!X) { Done(); return; }
			S->Status(FString::Printf(TEXT("'%s' changed locally (%s -> %s); the local file becomes the new version."), *X->Name, *Short(X->Sha256), *Short(LocalSha)));
			X->Sha256 = LocalSha;
			X->Size = Size;
			X->UpdatedAt = NowIso();
			S->SaveManifest();
			S->StepCompareWithServer(LocalId, LocalSha, Done);
		};

		if (E->ServerId.IsEmpty())
		{
			AdoptLocal();
			return;
		}

		// Does the server still hold the recorded version? Then the local copy is damaged: repair it from the server.
		const FString Recorded = E->Sha256;
		Self->Server->GetAssetInfo(E->ServerId, true, [WeakThis, LocalId, Recorded, Done, AdoptLocal](const FAssetServerResult& Result, const FAssetServerItem& Item)
		{
			UUserAssetLibrary* S = WeakThis.Get();
			if (!S) { return; }
			if (Result.IsConnectionError()) { S->StopOffline(Result); return; }
			if (Result.bOk && Item.bIntact && Item.StoredSha256 == Recorded)
			{
				S->StepRestoreLocal(LocalId, Recorded, false, Done);
			}
			else
			{
				AdoptLocal();
			}
		});
	});
}

void UUserAssetLibrary::StepDelete(const FString& LocalId, FStepDone Done)
{
	FUserAssetEntry* E = Find(LocalId);
	if (!E)
	{
		Done();
		return;
	}
	if (E->ServerId.IsEmpty())
	{
		Entries.RemoveAll([&LocalId](const FUserAssetEntry& X) { return X.LocalId == LocalId; });
		SaveManifest();
		Done();
		return;
	}

	const FString Name = E->Name;
	TWeakObjectPtr<UUserAssetLibrary> WeakThis(this);
	Server->DeleteAsset(E->ServerId, [WeakThis, LocalId, Name, Done](const FAssetServerResult& Result)
	{
		UUserAssetLibrary* S = WeakThis.Get();
		if (!S) { return; }
		if (Result.IsConnectionError()) { S->StopOffline(Result); return; }

		if (Result.bOk || Result.IsNotFound())
		{
			S->Entries.RemoveAll([&LocalId](const FUserAssetEntry& X) { return X.LocalId == LocalId; });
			S->SaveManifest();
			S->Status(FString::Printf(TEXT("Deleted '%s' on the server."), *Name));
			++S->BatchSynced;
		}
		else
		{
			S->SetState(LocalId, EUserAssetSyncState::PendingDelete, Result.Error); // stays pending, retried next sync
			++S->BatchErrors;
		}
		Done();
	});
}

void UUserAssetLibrary::StepRestoreLocal(const FString& LocalId, const FString& RequiredSha, bool bWasMissing, FStepDone Done)
{
	FUserAssetEntry* E = Find(LocalId);
	if (!E)
	{
		Done();
		return;
	}

	const FString ServerId = E->ServerId;
	TWeakObjectPtr<UUserAssetLibrary> WeakThis(this);
	Server->GetAssetInfo(ServerId, true, [WeakThis, LocalId, ServerId, RequiredSha, bWasMissing, Done](const FAssetServerResult& Result, const FAssetServerItem& Item)
	{
		UUserAssetLibrary* S = WeakThis.Get();
		if (!S) { return; }
		if (Result.IsConnectionError()) { S->StopOffline(Result); return; }

		auto Fail = [S, LocalId, Done](const FString& Error)
		{
			S->SetState(LocalId, EUserAssetSyncState::Error, Error);
			++S->BatchErrors;
			S->Status(Error, true);
			Done();
		};

		if (Result.IsNotFound()) { Fail(TEXT("local file is missing and the server has no copy")); return; }
		if (!Result.bOk)         { Fail(Result.Error); return; }
		if (!Item.bIntact)       { Fail(FString::Printf(TEXT("server copy of '%s' is damaged (stored %s != recorded %s)"), *Item.Name, *Short(Item.StoredSha256), *Short(Item.Sha256))); return; }
		if (!RequiredSha.IsEmpty() && Item.StoredSha256 != RequiredSha && !bWasMissing)
		{
			Fail(TEXT("server copy does not match the recorded version"));
			return;
		}

		const FString Temp = S->GetTempPath(LocalId, TEXT("download"), Item.Ext);
		S->Server->DownloadToFile(ServerId, Temp, [WeakThis, LocalId, Item, Temp, bWasMissing, Done, Fail](const FAssetServerResult& DlResult)
		{
			UUserAssetLibrary* S2 = WeakThis.Get();
			if (!S2) { return; }
			if (DlResult.IsConnectionError()) { S2->StopOffline(DlResult); return; }
			if (!DlResult.bOk) { Fail(DlResult.Error); return; }

			S2->HashFileAsync(Temp, [WeakThis, LocalId, Item, Temp, bWasMissing, Done, Fail](bool bOk, const FString& DownloadedSha, int64 Size)
			{
				UUserAssetLibrary* S3 = WeakThis.Get();
				FUserAssetEntry* X = S3 ? S3->Find(LocalId) : nullptr;
				if (!X) { IFileManager::Get().Delete(*Temp); Done(); return; }

				if (!bOk || DownloadedSha != Item.StoredSha256)
				{
					IFileManager::Get().Delete(*Temp);
					Fail(FString::Printf(TEXT("downloaded copy of '%s' hashes to %s, server says %s"), *X->Name, *Short(DownloadedSha), *Short(Item.StoredSha256)));
					return;
				}

				// Server extension wins (the file may have been replaced with another format)
				IFileManager::Get().Delete(*S3->GetLocalPath(*X), false, true, true);
				X->Ext = Item.Ext.IsEmpty() ? X->Ext : Item.Ext;
				const FString LocalPath = S3->GetLocalPath(*X);
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(LocalPath), true);
				if (!IFileManager::Get().Move(*LocalPath, *Temp, true))
				{
					Fail(TEXT("cannot write ") + LocalPath);
					return;
				}

				X->Sha256 = DownloadedSha;
				X->Size = Size;
				X->State = EUserAssetSyncState::Synced;
				X->LastError.Reset();
				X->LastSyncedAt = NowIso();
				S3->SaveManifest();
				++S3->BatchSynced;
				S3->Status(bWasMissing
					? FString::Printf(TEXT("Restored '%s' from the server (sha256 %s)."), *X->Name, *Short(DownloadedSha))
					: FString::Printf(TEXT("Repaired local '%s' from the server copy (sha256 %s)."), *X->Name, *Short(DownloadedSha)));
				S3->OnAssetRestored.Broadcast(*X);
				Done();
			});
		});
	});
}

void UUserAssetLibrary::StepCompareWithServer(const FString& LocalId, const FString& LocalSha, FStepDone Done)
{
	FUserAssetEntry* E = Find(LocalId);
	if (!E)
	{
		Done();
		return;
	}
	if (E->ServerId.IsEmpty())
	{
		StepUpload(LocalId, LocalSha, Done);
		return;
	}

	const FString ServerId = E->ServerId;
	TWeakObjectPtr<UUserAssetLibrary> WeakThis(this);
	Server->GetAssetInfo(ServerId, true, [WeakThis, LocalId, LocalSha, ServerId, Done](const FAssetServerResult& Result, const FAssetServerItem& Item)
	{
		UUserAssetLibrary* S = WeakThis.Get();
		FUserAssetEntry* X = S ? S->Find(LocalId) : nullptr;
		if (!S) { return; }
		if (Result.IsConnectionError()) { S->StopOffline(Result); return; }
		if (!X) { Done(); return; }

		if (Result.IsNotFound())
		{
			S->Status(FString::Printf(TEXT("'%s' is missing on the server; uploading the local file."), *X->Name));
			X->ServerId.Reset();
			S->SaveManifest();
			S->StepUpload(LocalId, LocalSha, Done);
			return;
		}
		if (!Result.bOk)
		{
			S->SetState(LocalId, EUserAssetSyncState::Error, Result.Error);
			++S->BatchErrors;
			Done();
			return;
		}

		if (Item.StoredSha256 == LocalSha)
		{
			S->SetState(LocalId, EUserAssetSyncState::Synced);
			++S->BatchSynced;
			Done();
			return;
		}

		// Hashes differ: download the server copy and compare it with the local file, then put the local
		// version back on the server (local is the source of truth for the user's own assets).
		const FString Temp = S->GetTempPath(LocalId, TEXT("server"), Item.Ext);
		S->Server->DownloadToFile(ServerId, Temp, [WeakThis, LocalId, LocalSha, Item, Temp, Done](const FAssetServerResult& DlResult)
		{
			UUserAssetLibrary* S2 = WeakThis.Get();
			if (!S2) { return; }
			if (DlResult.IsConnectionError()) { S2->StopOffline(DlResult); return; }

			auto Reupload = [WeakThis, LocalId, LocalSha, Item, Temp, Done](const FString& ServerCopySha)
			{
				UUserAssetLibrary* S3 = WeakThis.Get();
				FUserAssetEntry* X = S3 ? S3->Find(LocalId) : nullptr;
				IFileManager::Get().Delete(*Temp);
				if (!X) { Done(); return; }
				S3->Status(FString::Printf(TEXT("'%s' differs on the server (server %s, downloaded %s, local %s); re-uploading the local file."),
					*X->Name, *Short(Item.StoredSha256), *Short(ServerCopySha), *Short(LocalSha)));
				S3->StepUpload(LocalId, LocalSha, Done);
			};

			if (!DlResult.bOk)
			{
				Reupload(TEXT("download failed"));
				return;
			}
			S2->HashFileAsync(Temp, [Reupload](bool bOk, const FString& ServerCopySha, int64)
			{
				Reupload(bOk ? ServerCopySha : FString(TEXT("unreadable")));
			});
		});
	});
}

void UUserAssetLibrary::StepUpload(const FString& LocalId, const FString& LocalSha, FStepDone Done)
{
	FUserAssetEntry* E = Find(LocalId);
	if (!E)
	{
		Done();
		return;
	}

	const FString ReplaceId = E->ServerId;
	Status(FString::Printf(TEXT("Uploading '%s%s'..."), *E->Name, *E->Ext));
	TWeakObjectPtr<UUserAssetLibrary> WeakThis(this);
	Server->UploadFile(GetLocalPath(*E), E->Name + E->Ext, LocalSha, ReplaceId,
		[WeakThis, LocalId, LocalSha, ReplaceId, Done](const FAssetServerResult& Result, const FAssetServerItem& Item)
	{
		UUserAssetLibrary* S = WeakThis.Get();
		FUserAssetEntry* X = S ? S->Find(LocalId) : nullptr;
		if (!S) { return; }
		if (Result.IsConnectionError()) { S->StopOffline(Result); return; }
		if (!X) { Done(); return; }

		if (Result.IsNotFound() && !ReplaceId.IsEmpty())
		{
			// The asset we wanted to replace was deleted on the server meanwhile: upload as new
			X->ServerId.Reset();
			S->SaveManifest();
			S->StepUpload(LocalId, LocalSha, Done);
			return;
		}
		if (!Result.bOk || Item.Sha256 != LocalSha)
		{
			const FString Error = Result.bOk ? FString::Printf(TEXT("server recorded %s, expected %s"), *Short(Item.Sha256), *Short(LocalSha)) : Result.Error;
			S->SetState(LocalId, EUserAssetSyncState::Error, Error);
			S->Status(FString::Printf(TEXT("Upload of '%s' failed: %s"), *X->Name, *Error), true);
			++S->BatchErrors;
			Done();
			return;
		}

		X->ServerId = Item.Id;
		S->SaveManifest();
		S->StepVerifyAfterUpload(LocalId, LocalSha, Done);
	});
}

void UUserAssetLibrary::StepVerifyAfterUpload(const FString& LocalId, const FString& LocalSha, FStepDone Done)
{
	FUserAssetEntry* E = Find(LocalId);
	if (!E)
	{
		Done();
		return;
	}

	TWeakObjectPtr<UUserAssetLibrary> WeakThis(this);
	Server->GetAssetInfo(E->ServerId, true, [WeakThis, LocalId, LocalSha, Done](const FAssetServerResult& Result, const FAssetServerItem& Item)
	{
		UUserAssetLibrary* S = WeakThis.Get();
		FUserAssetEntry* X = S ? S->Find(LocalId) : nullptr;
		if (!S) { return; }
		if (Result.IsConnectionError()) { S->StopOffline(Result); return; }
		if (!X) { Done(); return; }

		if (Result.bOk && Item.StoredSha256 == LocalSha)
		{
			S->SetState(LocalId, EUserAssetSyncState::Synced);
			S->Status(FString::Printf(TEXT("'%s' uploaded and verified on the server (sha256 %s)."), *X->Name, *Short(LocalSha)));
			++S->BatchSynced;
		}
		else
		{
			const FString Error = Result.bOk ? FString::Printf(TEXT("verification failed: server file %s, local %s"), *Short(Item.StoredSha256), *Short(LocalSha)) : Result.Error;
			S->SetState(LocalId, EUserAssetSyncState::Error, Error);
			S->Status(FString::Printf(TEXT("'%s': %s"), *X->Name, *Error), true);
			++S->BatchErrors;
		}
		Done();
	});
}

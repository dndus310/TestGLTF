// Attribute Sample - the user's imported mesh files: local JSON manifest + local file store + server sync
//
// Local is the source of truth for the user's own assets. Every change (import / delete) is written to
// Saved/UserAssets/manifest.json first and then reflected on the asset server. Each entry is synced by
// comparing SHA-256 hashes:
//
//   R = hash recorded in the manifest, L = hash of the local file now, S = hash of the server's stored
//   file (the server re-hashes it: GET /assets/{id}?verify=true)
//
//   local file missing       -> download the server copy, check it hashes to S, restore it locally
//   L != R (local changed)   -> server copy hashes to R? download it and repair the local file
//                               otherwise accept the local file as the new truth (R = L)
//   not on the server        -> upload with X-Content-SHA256 = L (the server rejects a mismatch)
//   S == L                   -> in sync
//   S != L                   -> download the server copy and compare it with the local file (logged),
//                               then re-upload the local file to replace it and verify S == L again
//   pending delete           -> DELETE on the server, then drop the entry
//
// If the server cannot be reached, entries stay pending and are retried on the next SyncAll / change.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AttributeSampleTypes.h"
#include "UserAssetLibrary.generated.h"

class UAssetServerSubsystem;
struct FAssetServerResult;
struct FAssetServerItem;

UENUM(BlueprintType)
enum class EUserAssetSyncState : uint8
{
	PendingUpload,
	Synced,
	PendingDelete,
	Error
};

USTRUCT(BlueprintType)
struct FUserAssetEntry
{
	GENERATED_BODY()

	/** Local identity (GUID). Also the file name in the local store. */
	UPROPERTY(BlueprintReadOnly, Category = "UserAsset") FString LocalId;
	UPROPERTY(BlueprintReadOnly, Category = "UserAsset") FString Name;
	/** Lower-case with dot, e.g. ".glb" */
	UPROPERTY(BlueprintReadOnly, Category = "UserAsset") FString Ext;
	UPROPERTY(BlueprintReadOnly, Category = "UserAsset") EAttributeAssetType Layer = EAttributeAssetType::Building;
	/** Hash of the local file when it was last verified (R) */
	UPROPERTY(BlueprintReadOnly, Category = "UserAsset") FString Sha256;
	UPROPERTY(BlueprintReadOnly, Category = "UserAsset") int64 Size = 0;
	/** Asset id on the server; empty until the first upload succeeds */
	UPROPERTY(BlueprintReadOnly, Category = "UserAsset") FString ServerId;
	UPROPERTY(BlueprintReadOnly, Category = "UserAsset") EUserAssetSyncState State = EUserAssetSyncState::PendingUpload;
	UPROPERTY(BlueprintReadOnly, Category = "UserAsset") FString LastError;
	UPROPERTY(BlueprintReadOnly, Category = "UserAsset") FString UpdatedAt;
	UPROPERTY(BlueprintReadOnly, Category = "UserAsset") FString LastSyncedAt;
};

/** A local file was (re)written from the server copy: the catalog should (re)load it. */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnUserAssetRestored, const FUserAssetEntry& /*Entry*/);
/** Human readable sync progress / result */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUserAssetSyncStatus, const FString& /*Message*/, bool /*bIsError*/);

UCLASS()
class DWO_PROJ_01_API UUserAssetLibrary : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const TArray<FUserAssetEntry>& GetEntries() const { return Entries; }
	const FUserAssetEntry* FindEntry(const FString& LocalId) const;

	/** Saved/UserAssets/Files/<LocalId><Ext> */
	FString GetLocalPath(const FUserAssetEntry& Entry) const;

	/**
	 * Copies SourcePath into the local store, records it in the manifest (PendingUpload) and starts the sync.
	 * Returns false only if the local copy / manifest write fails.
	 */
	bool AddFile(const FString& SourcePath, EAttributeAssetType Layer, FUserAssetEntry& OutEntry, FString& OutError);

	/** Deletes the local copy, marks the entry PendingDelete and starts the sync (server DELETE). */
	bool RemoveAsset(const FString& LocalId);

	/** Queues every entry for a hash check against the server. */
	UFUNCTION(BlueprintCallable, Category = "UserAsset")
	void SyncAll();

	UFUNCTION(BlueprintPure, Category = "UserAsset")
	bool IsSyncing() const { return bSyncRunning; }

	UFUNCTION(BlueprintPure, Category = "UserAsset")
	static FString GetLibraryDir();

	FOnUserAssetRestored OnAssetRestored;
	FOnUserAssetSyncStatus OnSyncStatus;

private:
	using FStepDone = TFunction<void()>;

	// manifest
	void LoadManifest();
	bool SaveManifest();
	FString GetManifestPath() const;
	FUserAssetEntry* Find(const FString& LocalId);
	void SetState(const FString& LocalId, EUserAssetSyncState State, const FString& Error = FString());
	void Status(const FString& Message, bool bIsError = false);

	// queue
	void QueueSync(const FString& LocalId);
	void ProcessNext();
	/** Stops the queue: the server is down, everything stays pending for the next attempt */
	void StopOffline(const FAssetServerResult& Result);

	// steps (each ends by calling Done, or StopOffline on a connection error)
	void SyncEntry(const FString& LocalId, FStepDone Done);
	void StepDelete(const FString& LocalId, FStepDone Done);
	void StepRestoreLocal(const FString& LocalId, const FString& RequiredSha, bool bWasMissing, FStepDone Done);
	void StepCompareWithServer(const FString& LocalId, const FString& LocalSha, FStepDone Done);
	void StepUpload(const FString& LocalId, const FString& LocalSha, FStepDone Done);
	void StepVerifyAfterUpload(const FString& LocalId, const FString& LocalSha, FStepDone Done);

	/** Hash a file on a worker thread, call back on the game thread with (bOk, Sha, Size) */
	void HashFileAsync(const FString& Path, TFunction<void(bool, const FString&, int64)> Callback);
	FString GetTempPath(const FString& LocalId, const FString& Suffix, const FString& Ext) const;

	UPROPERTY(Transient)
	TObjectPtr<UAssetServerSubsystem> Server;

	UPROPERTY()
	TArray<FUserAssetEntry> Entries;

	TArray<FString> SyncQueue;
	bool bSyncRunning = false;
	int32 BatchSynced = 0;
	int32 BatchErrors = 0;
};

// Asset server client for UE 5.7 (HttpModule)
// - Control/metadata traffic : JSON  (FJsonObject / FJsonSerializer)
// - File bytes               : raw binary (no base64), streamed from disk on upload
// - Integrity                : uploads carry X-Content-SHA256 (the server rejects a mismatch with 422),
//                              GetAssetInfo(bVerify) makes the server re-hash its stored file
//
// The C++ functions take callbacks so callers (UUserAssetLibrary) can chain steps; the Blueprint
// functions are thin wrappers that broadcast the delegates below.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "AssetServerSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FAssetServerItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AssetServer") FString Id;
	UPROPERTY(BlueprintReadOnly, Category = "AssetServer") FString Name;
	UPROPERTY(BlueprintReadOnly, Category = "AssetServer") FString Ext;
	UPROPERTY(BlueprintReadOnly, Category = "AssetServer") int64 Size = 0;
	/** Hash the server recorded when the file was uploaded */
	UPROPERTY(BlueprintReadOnly, Category = "AssetServer") FString Sha256;
	/** Hash of the file as it is stored now (only filled by GetAssetInfo with bVerify) */
	UPROPERTY(BlueprintReadOnly, Category = "AssetServer") FString StoredSha256;
	/** StoredSha256 == Sha256 (only meaningful with bVerify) */
	UPROPERTY(BlueprintReadOnly, Category = "AssetServer") bool bIntact = false;
	UPROPERTY(BlueprintReadOnly, Category = "AssetServer") FString CreatedAt;
	UPROPERTY(BlueprintReadOnly, Category = "AssetServer") FString UpdatedAt;
	UPROPERTY(BlueprintReadOnly, Category = "AssetServer") FString DownloadUrl;
};

/** Result of one request. HttpCode is 0 when the server could not be reached. */
struct FAssetServerResult
{
	bool bOk = false;
	int32 HttpCode = 0;
	FString Error;

	bool IsConnectionError() const { return HttpCode == 0; }
	bool IsNotFound() const { return HttpCode == 404; }
};

using FAssetItemCallback = TFunction<void(const FAssetServerResult& Result, const FAssetServerItem& Item)>;
using FAssetResultCallback = TFunction<void(const FAssetServerResult& Result)>;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAssetUploaded, bool, bSuccess, const FAssetServerItem&, Item, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAssetListed, bool, bSuccess, const TArray<FAssetServerItem>&, Items, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAssetDownloaded, bool, bSuccess, const FString&, LocalPath, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAssetDeleted, bool, bSuccess, const FString&, AssetId, const FString&, Error);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAssetProgress, const FString&, Operation, int64, BytesDone, int64, BytesTotal);

UCLASS()
class DWO_PROJ_01_API UAssetServerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** e.g. http://192.168.0.10:8000 (no trailing slash). Command line: -AssetServerUrl=http://host:port */
	UPROPERTY(BlueprintReadWrite, Category = "AssetServer")
	FString BaseUrl = TEXT("http://localhost:8000");

	// ---------------------------------------------------------------- C++ API (callbacks)

	/**
	 * Uploads LocalFilePath as raw bytes. ExistingId empty -> POST /assets/raw (new id),
	 * otherwise PUT /assets/{id}/raw (replace, keeps the id). Sha256 goes in X-Content-SHA256.
	 */
	void UploadFile(const FString& LocalFilePath, const FString& UploadName, const FString& Sha256, const FString& ExistingId, FAssetItemCallback Callback);

	/** GET /assets/{id}[?verify=true] */
	void GetAssetInfo(const FString& AssetId, bool bVerify, FAssetItemCallback Callback);

	/** GET /assets/{id}/download -> DestPath (written as DestPath.part, then renamed). No hash check here. */
	void DownloadToFile(const FString& AssetId, const FString& DestPath, FAssetResultCallback Callback);

	/** DELETE /assets/{id} */
	void DeleteAsset(const FString& AssetId, FAssetResultCallback Callback);

	/** GET /assets[?q=] */
	void ListAssets(const FString& NameFilter, TFunction<void(const FAssetServerResult&, const TArray<FAssetServerItem>&)> Callback);

	// ---------------------------------------------------------------- Blueprint API (delegates)

	/** Hashes the file, then uploads it as a new asset. */
	UFUNCTION(BlueprintCallable, Category = "AssetServer", meta = (DisplayName = "Upload File"))
	void BP_UploadFile(const FString& LocalFilePath);

	UFUNCTION(BlueprintCallable, Category = "AssetServer", meta = (DisplayName = "List Assets"))
	void BP_ListAssets(const FString& NameFilter);

	/** Downloads into Saved/AssetCache/<sha256><ext>; a cached file with the right hash is reused. */
	UFUNCTION(BlueprintCallable, Category = "AssetServer", meta = (DisplayName = "Download Asset"))
	void BP_DownloadAsset(const FString& AssetId);

	UFUNCTION(BlueprintCallable, Category = "AssetServer", meta = (DisplayName = "Delete Asset"))
	void BP_DeleteAsset(const FString& AssetId);

	UPROPERTY(BlueprintAssignable, Category = "AssetServer") FOnAssetUploaded OnUploaded;
	UPROPERTY(BlueprintAssignable, Category = "AssetServer") FOnAssetListed OnListed;
	UPROPERTY(BlueprintAssignable, Category = "AssetServer") FOnAssetDownloaded OnDownloaded;
	UPROPERTY(BlueprintAssignable, Category = "AssetServer") FOnAssetDeleted OnDeleted;
	UPROPERTY(BlueprintAssignable, Category = "AssetServer") FOnAssetProgress OnProgress;

	/** Saved/AssetCache */
	UFUNCTION(BlueprintPure, Category = "AssetServer")
	static FString GetCacheDir();

private:
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> MakeRequest(const FString& Verb, const FString& Path) const;
	static bool ParseItem(const TSharedPtr<class FJsonObject>& Obj, FAssetServerItem& Out);
	static FAssetServerResult MakeResult(FHttpResponsePtr Resp, bool bConnected);
};

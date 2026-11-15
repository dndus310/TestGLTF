// Asset server client for UE 5.7 (HttpModule)
// - Control/metadata traffic : JSON  (FJsonObject / FJsonSerializer)
// - File bytes               : raw binary (no base64), so large GLBs stay fast and small
//
// Drop into Source/DWO_Proj_01/AttributeSample/ and add "HTTP", "Json" to the module's
// PrivateDependencyModuleNames in DWO_Proj_01.Build.cs.

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
	UPROPERTY(BlueprintReadOnly, Category = "AssetServer") int64 Size = 0;
	UPROPERTY(BlueprintReadOnly, Category = "AssetServer") FString Sha256;
	UPROPERTY(BlueprintReadOnly, Category = "AssetServer") FString CreatedAt;
	UPROPERTY(BlueprintReadOnly, Category = "AssetServer") FString DownloadUrl;
};

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
	/** e.g. http://192.168.0.10:8000 (no trailing slash) */
	UPROPERTY(BlueprintReadWrite, Category = "AssetServer")
	FString BaseUrl = TEXT("http://localhost:8000");

	/** POST /assets/raw?name=<file> with the file streamed from disk as the body. Response JSON -> Item. */
	UFUNCTION(BlueprintCallable, Category = "AssetServer")
	void UploadFile(const FString& LocalFilePath);

	/** GET /assets[?q=] -> JSON array */
	UFUNCTION(BlueprintCallable, Category = "AssetServer")
	void ListAssets(const FString& NameFilter);

	/**
	 * GET /assets/{id} (JSON: sha256) -> if Saved/AssetCache/<sha256>.glb exists, done (cache hit);
	 * otherwise GET /assets/{id}/download (binary) -> save -> verify size.
	 */
	UFUNCTION(BlueprintCallable, Category = "AssetServer")
	void DownloadAsset(const FString& AssetId);

	/** DELETE /assets/{id} */
	UFUNCTION(BlueprintCallable, Category = "AssetServer")
	void DeleteAsset(const FString& AssetId);

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
	static FString ErrorText(FHttpResponsePtr Resp, bool bConnected);
	void StartBinaryDownload(const FAssetServerItem& Item);
};

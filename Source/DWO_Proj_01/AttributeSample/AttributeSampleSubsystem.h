// Attribute Sample - owns the asset catalog, the attribute list and spawning

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AttributeSampleTypes.h"
#include "AttributeSampleSubsystem.generated.h"

class ACoreAttribute;
class AAttributeThumbnailStudio;
class UDataTable;
class UTexture;

DECLARE_MULTICAST_DELEGATE(FOnAttributeSampleListChanged);

/**
 * Two lists:
 *  - Catalog       : every available asset (DataTable rows + user imports) -> RightPanel TileView
 *  - AttributeList : assets registered for spawning                        -> LeftPanel list
 */
UCLASS()
class DWO_PROJ_01_API UAttributeSampleSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	/** Fills both lists from DT_AttributeAssetData. */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	void InitializeFromDataTable(UDataTable* DataTable);

	UFUNCTION(BlueprintPure, Category = "Attribute")
	TArray<UAttributeAssetItem*> GetCatalogItems(EAttributeAssetType Type) const;

	UFUNCTION(BlueprintPure, Category = "Attribute")
	TArray<UAttributeAssetItem*> GetAttributeListItems(EAttributeAssetType Type) const;

	/** LeftPanel "AddAttribute": registers a catalog item in the attribute list. */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool AddAttribute(UAttributeAssetItem* Item);

	/** LeftPanel item click: spawns Building/LoadAttribute at a random point and applies the item mesh. */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	ACoreAttribute* SpawnAttribute(UAttributeAssetItem* Item);

	/** Loads a user file through glTFRuntime and adds it to the catalog (memory only, nothing is saved). */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	UAttributeAssetItem* ImportUserAsset(const FString& FilePath, EAttributeAssetType Type, FString& OutError);

	/**
	 * RightPanel "Import" / drop: ImportUserAsset, then registers the file in UUserAssetLibrary
	 * (copied to Saved/UserAssets, recorded in manifest.json, uploaded to the asset server with a hash check).
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	UAttributeAssetItem* ImportAndRegisterUserAsset(const FString& FilePath, EAttributeAssetType Type, FString& OutError);

	/** Removes a user import from the catalog / attribute list, deletes the local copy and the server copy. */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool DeleteUserAsset(UAttributeAssetItem* Item, FString& OutError);

	UFUNCTION(BlueprintCallable, Category = "Attribute")
	UTexture* GetOrCreateThumbnail(UAttributeAssetItem* Item);

	UFUNCTION(BlueprintCallable, Category = "Attribute")
	void ClearSpawnedAttributes();

	/** Random spawn area on the floor (XY, cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	FVector2D SpawnHalfExtent = FVector2D(1000.f, 1000.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	TSubclassOf<ACoreAttribute> BuildingAttributeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	TSubclassOf<ACoreAttribute> LoadAttributeClass;

	FOnAttributeSampleListChanged OnCatalogChanged;
	FOnAttributeSampleListChanged OnAttributeListChanged;

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	FVector FindSpawnLocation() const;

	class UUserAssetLibrary* GetUserAssetLibrary() const;
	/** Adds every saved user asset to the catalog and starts the server sync. */
	void LoadUserAssetsFromLibrary();
	UAttributeAssetItem* AddLibraryEntryToCatalog(const struct FUserAssetEntry& Entry);
	/** The library rewrote a local file from the server copy: (re)load it into the catalog. */
	void HandleUserAssetRestored(const struct FUserAssetEntry& Entry);

	FDelegateHandle AssetRestoredHandle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAttributeAssetItem>> Catalog;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAttributeAssetItem>> AttributeList;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACoreAttribute>> SpawnedAttributes;

	UPROPERTY(Transient)
	TObjectPtr<AAttributeThumbnailStudio> ThumbnailStudio;

	int32 UserImportCounter = 0;
};

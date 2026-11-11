// Attribute Sample - shared data types

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UObject/Object.h"
#include "AttributeSampleTypes.generated.h"

class UStaticMesh;
class UTexture;

/** Layer an attribute asset belongs to. Drives the Building / Load tabs and which actor class is spawned. */
UENUM(BlueprintType)
enum class EAttributeAssetType : uint8
{
	Building,
	Load
};

/** One row of DT_AttributeAssetData. */
USTRUCT(BlueprintType)
struct FAttributeAssetData : public FTableRowBase
{
	GENERATED_BODY()

	/** Display name shown in the lists. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	FString Name;

	/** Building or Load layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	EAttributeAssetType AssetType = EAttributeAssetType::Building;

	/** Mesh applied to the spawned attribute. Loaded on demand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	TSoftObjectPtr<UStaticMesh> SoftObjectStaticMeshAsset;

	/**
	 * Optional thumbnail. Either a texture object path (/Game/...)
	 * or an image file on disk (png/jpg). Empty = rendered automatically from the mesh.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute")
	FString ThumbnailPath;
};

/**
 * Runtime list item used by both panels (VerticalBox list and TileView).
 * Wraps a DataTable row, or a mesh the user imported from disk.
 */
UCLASS(BlueprintType)
class DWO_PROJ_01_API UAttributeAssetItem : public UObject
{
	GENERATED_BODY()

public:
	/** DataTable row name, or a generated name for user imports. */
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	FName RowName;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	FAttributeAssetData Data;

	/** Mesh built at runtime by glTFRuntime / glTFRuntimeFBX (user imports only). */
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	TObjectPtr<UStaticMesh> RuntimeMesh;

	/** Original file on disk (user imports only). */
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	FString SourceFilePath;

	/** Cached thumbnail (texture asset, image file, or render target). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Attribute")
	TObjectPtr<UTexture> Thumbnail;

	UFUNCTION(BlueprintPure, Category = "Attribute")
	bool IsUserImported() const { return !SourceFilePath.IsEmpty(); }

	/** Returns the runtime mesh, or synchronously loads the soft reference. */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	UStaticMesh* ResolveMesh();

	/** Tries to load ThumbnailPath as a texture asset or an image file. */
	UTexture* LoadThumbnailFromPath() const;
};

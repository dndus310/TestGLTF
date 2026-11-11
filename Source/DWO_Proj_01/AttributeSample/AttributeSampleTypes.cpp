// Attribute Sample - shared data types

#include "AttributeSampleTypes.h"

#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"

UStaticMesh* UAttributeAssetItem::ResolveMesh()
{
	if (RuntimeMesh)
	{
		return RuntimeMesh;
	}

	if (!Data.SoftObjectStaticMeshAsset.IsNull())
	{
		// Sample keeps this synchronous for readability. Use FStreamableManager::RequestAsyncLoad for large assets.
		return Data.SoftObjectStaticMeshAsset.LoadSynchronous();
	}

	return nullptr;
}

UTexture* UAttributeAssetItem::LoadThumbnailFromPath() const
{
	const FString& Path = Data.ThumbnailPath;
	if (Path.IsEmpty())
	{
		return nullptr;
	}

	// Asset path (/Game/..., /Engine/...)
	if (Path.StartsWith(TEXT("/")))
	{
		return Cast<UTexture>(FSoftObjectPath(Path).TryLoad());
	}

	// Image file on disk, absolute or relative to the project directory
	FString FilePath = Path;
	if (FPaths::IsRelative(FilePath))
	{
		FilePath = FPaths::Combine(FPaths::ProjectDir(), FilePath);
	}

	if (FPaths::FileExists(FilePath))
	{
		return FImageUtils::ImportFileAsTexture2D(FilePath);
	}

	return nullptr;
}

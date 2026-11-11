// Attribute Sample - user file import (file dialog + glTFRuntime / glTFRuntimeFBX)

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AttributeRuntimeMeshLoader.generated.h"

class UStaticMesh;

UCLASS()
class DWO_PROJ_01_API UAttributeRuntimeMeshLoader : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Opens the native Windows file dialog (works in Shipping). Returns false when cancelled or unsupported. */
	UFUNCTION(BlueprintCallable, Category = "Attribute|Import")
	static bool OpenMeshFileDialog(FString& OutFilePath);

	/**
	 * Loads a .gltf / .glb / .fbx file and merges all mesh nodes into one UStaticMesh.
	 * Must run on the game thread.
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute|Import")
	static UStaticMesh* LoadStaticMeshFromFile(UObject* Outer, const FString& FilePath, FString& OutError);

	UFUNCTION(BlueprintPure, Category = "Attribute|Import")
	static bool IsSupportedMeshFile(const FString& FilePath);

	/** User files are untrusted input: reject anything above these limits. */
	static constexpr int64 MaxFileSizeBytes = 256ll * 1024 * 1024;
	static constexpr int32 MaxTriangles = 3000000;
};

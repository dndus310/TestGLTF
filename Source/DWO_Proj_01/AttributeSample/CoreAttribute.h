// Attribute Sample - base attribute actor

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AttributeSampleTypes.h"
#include "CoreAttribute.generated.h"

class USplineComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Base attribute placed in the level.
 *  - StaticMeshComponent : shows the asset mesh (DefaultMesh until one is applied)
 *  - SplineComponent     : closed footprint outline generated from the mesh bounds
 */
UCLASS()
class DWO_PROJ_01_API ACoreAttribute : public AActor
{
	GENERATED_BODY()

public:
	ACoreAttribute();

	/** Applies a mesh at runtime (asset or glTFRuntime mesh), rests it on the ground and rebuilds the spline. Null = DefaultMesh. */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool ApplyRuntimeSetMeshAsset(UStaticMesh* NewMesh);

	/** Builds a closed, linear spline around the XY footprint of the current mesh bounds. */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	void MakeSplinePointsByShapeBounding();

	UFUNCTION(BlueprintPure, Category = "Attribute")
	EAttributeAssetType GetAttributeType() const { return AttributeType; }

	UFUNCTION(BlueprintPure, Category = "Attribute")
	USplineComponent* GetSplineComponent() const { return SplineComponent; }

	UFUNCTION(BlueprintPure, Category = "Attribute")
	UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }

	/** Name of the source asset (DataTable row name or imported file). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadWrite, Category = "Attribute")
	FString SourceAssetName;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Rebuilds the visible outline (one thin mesh per spline segment) from the spline points. */
	void RebuildOutlineVisual();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attribute")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attribute")
	TObjectPtr<USplineComponent> SplineComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Attribute")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;

	/** Mesh shown before any asset is applied. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attribute")
	TObjectPtr<UStaticMesh> DefaultMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attribute")
	EAttributeAssetType AttributeType = EAttributeAssetType::Building;

	/** Outline look */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute|Outline")
	bool bShowOutline = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute|Outline")
	FLinearColor OutlineColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute|Outline", meta = (ClampMin = "0.5"))
	float OutlineThickness = 6.f;

	/** Extra margin around the mesh footprint (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute|Outline")
	float OutlinePadding = 15.f;

	/** Height above the mesh bottom (cm), avoids z-fighting with the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute|Outline")
	float OutlineHeightOffset = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "Attribute|Outline")
	TObjectPtr<UStaticMesh> OutlineSegmentMesh;

	/** Expected to expose a "Color" vector parameter (BasicShapeMaterial does). */
	UPROPERTY(EditDefaultsOnly, Category = "Attribute|Outline")
	TObjectPtr<UMaterialInterface> OutlineMaterial;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> OutlineSegments;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OutlineMID;
};

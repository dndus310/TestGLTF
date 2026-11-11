// Attribute Sample - shared base for Left/Right panels: frame + Layer (Building / Load) tabs

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AttributeSampleTypes.h"
#include "AttributeLayerPanelWidget.generated.h"

class UAttributeSampleSubsystem;
class UBorder;
class UButton;
class UHorizontalBox;

/**
 * Widgets are built in C++ when the widget tree is empty.
 * A Widget Blueprint child can instead lay out its own designer tree using the same widget names
 * (Layer, BuildingTab, LoadTab, ...) - they are picked up through BindWidgetOptional.
 */
UCLASS(Abstract)
class DWO_PROJ_01_API UAttributeLayerPanelWidget : public UUserWidget
{
	GENERATED_BODY()

	/** Editor tool that writes BuildDefaultLayout() into the Widget Blueprint designer tree. */
	friend class FAttributeWidgetDesignerExporter;

public:
	UFUNCTION(BlueprintCallable, Category = "Attribute|UI")
	void SetLayer(EAttributeAssetType NewLayer);

	UFUNCTION(BlueprintPure, Category = "Attribute|UI")
	EAttributeAssetType GetLayer() const { return CurrentLayer; }

protected:
	virtual void NativeOnInitialized() override;

	/** Build the default layout into the (empty) widget tree. */
	virtual void BuildDefaultLayout() {}

	/** Called after the layout exists: bind events here. */
	virtual void OnPanelInitialized() {}

	virtual void OnLayerChanged() {}

	/** Root canvas + panel border docked to the left or right edge. */
	UBorder* BuildPanelFrame(bool bRightSide, float Width);

	/** Creates Layer (HorizontalBox) with BuildingTab / LoadTab buttons. */
	UHorizontalBox* BuildLayerTabs();

	void RefreshTabVisuals();

	UAttributeSampleSubsystem* GetSampleSubsystem() const;

	UFUNCTION()
	void HandleBuildingTabClicked();

	UFUNCTION()
	void HandleLoadTabClicked();

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> Layer;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UButton> BuildingTab;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UButton> LoadTab;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI")
	EAttributeAssetType CurrentLayer = EAttributeAssetType::Building;
};

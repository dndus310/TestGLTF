// Attribute Sample - WBP_RightPanel: Layer tabs + WBP_AttributeAssetList

#pragma once

#include "CoreMinimal.h"
#include "AttributeLayerPanelWidget.h"
#include "AttributeRightPanelWidget.generated.h"

class UAttributeAssetListWidget;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAttributeApplied, UAttributeAssetItem* /*Item*/);

UCLASS()
class DWO_PROJ_01_API UAttributeRightPanelWidget : public UAttributeLayerPanelWidget
{
	GENERATED_BODY()

public:
	FSimpleMulticastDelegate OnCloseRequested;

	/** Apply succeeded: the item was added to the attribute list */
	FOnAttributeApplied OnAttributeApplied;

	UFUNCTION(BlueprintCallable, Category = "Attribute|UI")
	void RefreshAssets();

protected:
	virtual void BuildDefaultLayout() override;
	virtual void OnPanelInitialized() override;
	virtual void OnLayerChanged() override;
	virtual void NativeDestruct() override;

	void HandleApply();
	void HandleImportUserAsset();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(EditAnywhere, Category = "Attribute|UI")
	TSoftClassPtr<UAttributeAssetListWidget> AttributeAssetListClass = TSoftClassPtr<UAttributeAssetListWidget>(FSoftObjectPath(TEXT("/Game/AttributeSample/UI/WBP_AttributeAssetList.WBP_AttributeAssetList_C")));

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UAttributeAssetListWidget> AttributeAssetList;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

private:
	FDelegateHandle CatalogChangedHandle;
};

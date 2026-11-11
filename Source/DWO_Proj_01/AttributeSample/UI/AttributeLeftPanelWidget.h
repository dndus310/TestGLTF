// Attribute Sample - WBP_LeftPanel: Layer tabs + WBP_AttributeList

#pragma once

#include "CoreMinimal.h"
#include "AttributeLayerPanelWidget.h"
#include "AttributeLeftPanelWidget.generated.h"

class UAttributeListWidget;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnAddAttributeRequested, EAttributeAssetType /*Layer*/);

UCLASS()
class DWO_PROJ_01_API UAttributeLeftPanelWidget : public UAttributeLayerPanelWidget
{
	GENERATED_BODY()

public:
	/** AddAttribute pressed -> owner opens the RightPanel */
	FOnAddAttributeRequested OnAddAttributeRequested;

	UFUNCTION(BlueprintCallable, Category = "Attribute|UI")
	void RefreshList();

protected:
	virtual void BuildDefaultLayout() override;
	virtual void OnPanelInitialized() override;
	virtual void OnLayerChanged() override;
	virtual void NativeDestruct() override;

	void HandleItemClicked(UAttributeAssetItem* Item);
	void HandleAddAttribute();

	UPROPERTY(EditAnywhere, Category = "Attribute|UI")
	TSoftClassPtr<UAttributeListWidget> AttributeListClass = TSoftClassPtr<UAttributeListWidget>(FSoftObjectPath(TEXT("/Game/AttributeSample/UI/WBP_AttributeList.WBP_AttributeList_C")));

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UAttributeListWidget> AttributeList;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	FDelegateHandle ListChangedHandle;
};

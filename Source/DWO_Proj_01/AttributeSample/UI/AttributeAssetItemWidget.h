// Attribute Sample - WBP_AttributeAssetItem: TileView entry (thumbnail + name)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "AttributeAssetItemWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UAttributeAssetItem;

UCLASS()
class DWO_PROJ_01_API UAttributeAssetItemWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

	/** Editor tool that writes BuildDefaultLayout() into the Widget Blueprint designer tree. */
	friend class FAttributeWidgetDesignerExporter;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;

	void BuildDefaultLayout();

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Background;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Thumbnail;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TagText;

	UPROPERTY(Transient)
	TObjectPtr<UAttributeAssetItem> Item;
};

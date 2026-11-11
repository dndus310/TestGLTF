// Attribute Sample - WBP_AttributeAssetList: TileView of assets + Import / Apply buttons

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AttributeSampleTypes.h"
#include "AttributeAssetListWidget.generated.h"

class UButton;
class UTextBlock;
class UTileView;

UCLASS()
class DWO_PROJ_01_API UAttributeAssetListWidget : public UUserWidget
{
	GENERATED_BODY()

	/** Editor tool that writes BuildDefaultLayout() into the Widget Blueprint designer tree. */
	friend class FAttributeWidgetDesignerExporter;

public:
	UFUNCTION(BlueprintCallable, Category = "Attribute|UI")
	void SetItems(const TArray<UAttributeAssetItem*>& Items);

	UFUNCTION(BlueprintPure, Category = "Attribute|UI")
	UAttributeAssetItem* GetSelectedItem() const;

	UFUNCTION(BlueprintCallable, Category = "Attribute|UI")
	void SelectItem(UAttributeAssetItem* Item);

	UFUNCTION(BlueprintCallable, Category = "Attribute|UI")
	void SetStatus(const FString& Message, bool bIsError = false);

	FSimpleMulticastDelegate OnApplyClicked;
	FSimpleMulticastDelegate OnImportClicked;

	/** TileView size = Columns x VisibleRows entries */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute|UI", meta = (ClampMin = "1"))
	int32 Columns = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute|UI", meta = (ClampMin = "1"))
	int32 VisibleRows = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute|UI")
	float EntryWidth = 124.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attribute|UI")
	float EntryHeight = 150.f;

protected:
	virtual void NativeOnInitialized() override;

	void BuildDefaultLayout();
	void ConfigureTileView();

	UFUNCTION()
	void HandleApplyClicked();

	UFUNCTION()
	void HandleImportClicked();

	UPROPERTY(EditAnywhere, Category = "Attribute|UI")
	TSoftClassPtr<UUserWidget> EntryWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/AttributeSample/UI/WBP_AttributeAssetItem.WBP_AttributeAssetItem_C")));

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTileView> List;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Apply;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ImportUserAsset;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;
};

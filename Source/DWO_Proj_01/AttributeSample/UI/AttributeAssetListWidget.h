// Attribute Sample - WBP_AttributeAssetList: TileView of assets + Import / Apply buttons

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AttributeSampleTypes.h"
#include "AttributeAssetListWidget.generated.h"

class UButton;
class UFileDropZone;
class UTextBlock;
class UTileView;

/** Files dropped from Explorer onto the Import button: (allowed mesh files, everything else) */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnImportFilesDropped, const TArray<FString>& /*AcceptedFiles*/, const TArray<FString>& /*RejectedFiles*/);

UCLASS()
class DWO_PROJ_01_API UAttributeAssetListWidget : public UUserWidget
{
	GENERATED_BODY()

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
	FOnImportFilesDropped OnImportFilesDropped;

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

	/** Wraps the Import button in a File Drop Zone unless the designer already placed one around it. */
	void EnsureImportDropZone();

	UFUNCTION()
	void HandleImportFilesDropped(const TArray<FString>& AcceptedFiles, const TArray<FString>& RejectedFiles);

	UPROPERTY(EditAnywhere, Category = "Attribute|UI")
	TSoftClassPtr<UUserWidget> EntryWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(TEXT("/Game/AttributeSample/UI/WBP_AttributeAssetItem.WBP_AttributeAssetItem_C")));

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTileView> List;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Apply;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UButton> ImportUserAsset;

	/** Optional: place a File Drop Zone named ImportDropZone around the Import button in the designer.
	 *  If missing, one is created around ImportUserAsset at runtime. */
	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UFileDropZone> ImportDropZone;

	UPROPERTY(BlueprintReadOnly, Category = "Attribute|UI", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;
};

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
	void HandleImportFilesDropped(const TArray<FString>& AcceptedFiles, const TArray<FString>& RejectedFiles);
	void HandleDeleteUserAsset();
	/** UUserAssetLibrary server sync progress -> status line (not while a local import batch is running) */
	void HandleSyncStatus(const FString& Message, bool bIsError);

	/** Queues files for import into the current layer. Imports run one per frame so the status can repaint. */
	void QueueImport(const TArray<FString>& Files, int32 SkippedCount);
	void ImportNextQueued();
	void FinishImport();

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
	FDelegateHandle SyncStatusHandle;

	// Batch import state
	TArray<FString> ImportQueue;
	TArray<FString> ImportErrors;
	TWeakObjectPtr<UAttributeAssetItem> LastImported;
	EAttributeAssetType ImportLayer = EAttributeAssetType::Building;
	int32 ImportTotal = 0;
	int32 ImportSucceeded = 0;
	int32 ImportSkipped = 0;
	bool bImportRunning = false;
};

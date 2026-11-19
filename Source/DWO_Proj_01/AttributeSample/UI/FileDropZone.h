// Attribute Sample - accepts files dragged from Windows Explorer onto its content (e.g. the Import button)
//
// UMG's UUserWidget::NativeOnDrop only sees UMG drag operations (FUMGDragDropOp). Files dropped from the OS
// arrive in Slate as FExternalDragOperation, so this widget handles them at the Slate level and forwards
// the file list through OnFilesDropped. Windows desktop only (the OS registers the game window as an
// OLE drop target). A game running elevated (as administrator) cannot receive drops from a non-elevated
// Explorer.

#pragma once

#include "CoreMinimal.h"
#include "Components/ContentWidget.h"
#include "FileDropZone.generated.h"

class SFileDropTarget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFilesDropped, const TArray<FString>&, AcceptedFiles, const TArray<FString>&, RejectedFiles);

UCLASS(meta = (DisplayName = "File Drop Zone"))
class DWO_PROJ_01_API UFileDropZone : public UContentWidget
{
	GENERATED_BODY()

public:
	/** Lower-case extensions without the dot, e.g. "glb". Empty = any file. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Drop")
	TArray<FString> AllowedExtensions;

	/** Outline while dragging files that include at least one allowed file */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Drop")
	FLinearColor AcceptColor = FLinearColor(0.15f, 0.55f, 1.f, 1.f);

	/** Outline while dragging only files that are not allowed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Drop")
	FLinearColor RejectColor = FLinearColor(0.9f, 0.25f, 0.2f, 1.f);

	/** Outline thickness around the content (always reserved so the layout does not jump) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "File Drop")
	FMargin OutlinePadding = FMargin(2.f);

	/** Fired on drop. Files are split by AllowedExtensions; folders count as rejected. */
	UPROPERTY(BlueprintAssignable, Category = "File Drop")
	FOnFilesDropped OnFilesDropped;

	UFUNCTION(BlueprintPure, Category = "File Drop")
	bool IsFileAllowed(const FString& FilePath) const;

	void SplitFiles(const TArray<FString>& Files, TArray<FString>& OutAccepted, TArray<FString>& OutRejected) const;

	/** Called by the Slate drop target */
	void NotifyFilesDropped(const TArray<FString>& Files);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void OnSlotAdded(UPanelSlot* InSlot) override;
	virtual void OnSlotRemoved(UPanelSlot* InSlot) override;

private:
	TSharedPtr<SFileDropTarget> MyDropTarget;
};

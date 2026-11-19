// Attribute Sample - File Drop Zone

#include "FileDropZone.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/PanelSlot.h"
#include "HAL/FileManager.h"
#include "Input/DragAndDrop.h"
#include "Misc/Paths.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SCompoundWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogFileDropZone, Log, All);

/** Slate side: outline + OS file drag handling around one child */
class SFileDropTarget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFileDropTarget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UFileDropZone>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Owner = InArgs._Owner;
		OutlineBrush = FSlateRoundedBoxBrush(FLinearColor::White, 4.f);

		ChildSlot
		[
			SAssignNew(Border, SBorder)
			.BorderImage(&OutlineBrush)
			.BorderBackgroundColor(this, &SFileDropTarget::GetOutlineColor)
			.Padding(this, &SFileDropTarget::GetOutlinePadding)
		];
	}

	void SetContent(const TSharedRef<SWidget>& Content)
	{
		Border->SetContent(Content);
	}

	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		UpdateState(DragDropEvent);
		const TSharedPtr<FDragDropOperation> Op = DragDropEvent.GetOperation();
		UE_LOG(LogFileDropZone, Verbose, TEXT("DragEnter: op=%s external=%d state=%d"), Op.IsValid() ? TEXT("valid") : TEXT("none"),
			Op.IsValid() && Op->IsOfType<FExternalDragOperation>() ? 1 : 0, (int32)State);
	}

	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override
	{
		State = EState::None;
	}

	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		UpdateState(DragDropEvent);
		return State == EState::None ? FReply::Unhandled() : FReply::Handled();
	}

	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override
	{
		State = EState::None;
		const TSharedPtr<FExternalDragOperation> Op = DragDropEvent.GetOperationAs<FExternalDragOperation>();
		if (!Op.IsValid() || !Op->HasFiles())
		{
			UE_LOG(LogFileDropZone, Verbose, TEXT("Drop ignored: not an external file drag"));
			return FReply::Unhandled();
		}
		UE_LOG(LogFileDropZone, Verbose, TEXT("Drop: %d file(s)"), Op->GetFiles().Num());
		if (UFileDropZone* Zone = Owner.Get())
		{
			Zone->NotifyFilesDropped(Op->GetFiles());
		}
		return FReply::Handled();
	}

private:
	enum class EState : uint8 { None, Accept, Reject };

	void UpdateState(const FDragDropEvent& DragDropEvent)
	{
		const TSharedPtr<FExternalDragOperation> Op = DragDropEvent.GetOperationAs<FExternalDragOperation>();
		if (!Op.IsValid() || !Op->HasFiles())
		{
			State = EState::None;
			return;
		}
		const UFileDropZone* Zone = Owner.Get();
		const bool bAnyAllowed = Zone && Op->GetFiles().ContainsByPredicate([Zone](const FString& File) { return Zone->IsFileAllowed(File); });
		State = bAnyAllowed ? EState::Accept : EState::Reject;
	}

	FSlateColor GetOutlineColor() const
	{
		const UFileDropZone* Zone = Owner.Get();
		if (!Zone || State == EState::None)
		{
			return FLinearColor::Transparent;
		}
		return State == EState::Accept ? Zone->AcceptColor : Zone->RejectColor;
	}

	FMargin GetOutlinePadding() const
	{
		const UFileDropZone* Zone = Owner.Get();
		return Zone ? Zone->OutlinePadding : FMargin(0.f);
	}

	TWeakObjectPtr<UFileDropZone> Owner;
	TSharedPtr<SBorder> Border;
	FSlateBrush OutlineBrush;
	EState State = EState::None;
};

bool UFileDropZone::IsFileAllowed(const FString& FilePath) const
{
	if (FilePath.IsEmpty() || IFileManager::Get().DirectoryExists(*FilePath))
	{
		return false; // folders are not imported
	}
	if (AllowedExtensions.IsEmpty())
	{
		return true;
	}
	const FString Extension = FPaths::GetExtension(FilePath).ToLower();
	return AllowedExtensions.ContainsByPredicate([&Extension](const FString& Allowed) { return Allowed.Equals(Extension, ESearchCase::IgnoreCase); });
}

void UFileDropZone::SplitFiles(const TArray<FString>& Files, TArray<FString>& OutAccepted, TArray<FString>& OutRejected) const
{
	for (const FString& File : Files)
	{
		(IsFileAllowed(File) ? OutAccepted : OutRejected).Add(File);
	}
}

void UFileDropZone::NotifyFilesDropped(const TArray<FString>& Files)
{
	TArray<FString> Accepted, Rejected;
	SplitFiles(Files, Accepted, Rejected);
	UE_LOG(LogFileDropZone, Log, TEXT("%s: %d accepted, %d rejected, %d listener(s)"), *GetName(), Accepted.Num(), Rejected.Num(), OnFilesDropped.IsBound() ? 1 : 0);
	OnFilesDropped.Broadcast(Accepted, Rejected);
}

TSharedRef<SWidget> UFileDropZone::RebuildWidget()
{
	MyDropTarget = SNew(SFileDropTarget).Owner(this);
	if (UWidget* Content = GetContent())
	{
		MyDropTarget->SetContent(Content->TakeWidget());
	}
	UE_LOG(LogFileDropZone, Verbose, TEXT("%s: Slate built (content=%s)"), *GetPathName(), *GetNameSafe(GetContent()));
	return MyDropTarget.ToSharedRef();
}

void UFileDropZone::OnSlotAdded(UPanelSlot* InSlot)
{
	if (MyDropTarget.IsValid() && InSlot && InSlot->Content)
	{
		MyDropTarget->SetContent(InSlot->Content->TakeWidget());
	}
}

void UFileDropZone::OnSlotRemoved(UPanelSlot* InSlot)
{
	if (MyDropTarget.IsValid())
	{
		MyDropTarget->SetContent(SNullWidget::NullWidget);
	}
}

void UFileDropZone::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	MyDropTarget.Reset();
}

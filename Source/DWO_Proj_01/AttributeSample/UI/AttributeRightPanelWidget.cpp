// Attribute Sample - WBP_RightPanel

#include "AttributeRightPanelWidget.h"

#include "AttributeAssetListWidget.h"
#include "AttributeRuntimeMeshLoader.h"
#include "AttributeSampleSubsystem.h"
#include "AttributeUIHelpers.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

void UAttributeRightPanelWidget::BuildDefaultLayout()
{
	UBorder* Frame = BuildPanelFrame(true, 440.f);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
	Frame->SetContent(Content);

	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Header"));
	UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(AttributeUI::MakeText(WidgetTree, TEXT("ASSET LIBRARY"), 16, AttributeUI::TextColor, true));
	TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	CloseButton = AttributeUI::MakeButton(WidgetTree, TEXT("X"), TEXT("CloseButton"), AttributeUI::ButtonColor);
	Header->AddChildToHorizontalBox(CloseButton);
	Content->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));

	Content->AddChildToVerticalBox(AttributeUI::MakeText(WidgetTree, TEXT("Pick an asset and Apply to add it to the attribute list"), 9, AttributeUI::SubTextColor))
		->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

	Content->AddChildToVerticalBox(BuildLayerTabs())->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

	const TSubclassOf<UAttributeAssetListWidget> ListClass = AttributeUI::ResolveClass(AttributeAssetListClass, UAttributeAssetListWidget::StaticClass());
	AttributeAssetList = WidgetTree->ConstructWidget<UAttributeAssetListWidget>(ListClass, TEXT("AttributeAssetList"));
	Content->AddChildToVerticalBox(AttributeAssetList);
}

void UAttributeRightPanelWidget::OnPanelInitialized()
{
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &UAttributeRightPanelWidget::HandleCloseClicked);
	}

	if (AttributeAssetList)
	{
		AttributeAssetList->OnApplyClicked.AddUObject(this, &UAttributeRightPanelWidget::HandleApply);
		AttributeAssetList->OnImportClicked.AddUObject(this, &UAttributeRightPanelWidget::HandleImportUserAsset);
		AttributeAssetList->OnImportFilesDropped.AddUObject(this, &UAttributeRightPanelWidget::HandleImportFilesDropped);
	}

	if (UAttributeSampleSubsystem* Subsystem = GetSampleSubsystem())
	{
		CatalogChangedHandle = Subsystem->OnCatalogChanged.AddUObject(this, &UAttributeRightPanelWidget::RefreshAssets);
	}
}

void UAttributeRightPanelWidget::NativeDestruct()
{
	if (UAttributeSampleSubsystem* Subsystem = GetSampleSubsystem())
	{
		Subsystem->OnCatalogChanged.Remove(CatalogChangedHandle);
	}
	Super::NativeDestruct();
}

void UAttributeRightPanelWidget::OnLayerChanged()
{
	RefreshAssets();
}

void UAttributeRightPanelWidget::RefreshAssets()
{
	UAttributeSampleSubsystem* Subsystem = GetSampleSubsystem();
	if (AttributeAssetList && Subsystem)
	{
		AttributeAssetList->SetItems(Subsystem->GetCatalogItems(CurrentLayer));
	}
}

void UAttributeRightPanelWidget::HandleApply()
{
	UAttributeSampleSubsystem* Subsystem = GetSampleSubsystem();
	UAttributeAssetItem* Selected = AttributeAssetList ? AttributeAssetList->GetSelectedItem() : nullptr;
	if (!Subsystem || !AttributeAssetList)
	{
		return;
	}

	if (!Selected)
	{
		AttributeAssetList->SetStatus(TEXT("Select an asset first."), true);
		return;
	}

	if (!Subsystem->AddAttribute(Selected))
	{
		AttributeAssetList->SetStatus(FString::Printf(TEXT("'%s' is already in the attribute list."), *Selected->Data.Name), true);
		return;
	}

	AttributeAssetList->SetStatus(FString::Printf(TEXT("Added '%s'."), *Selected->Data.Name));
	OnAttributeApplied.Broadcast(Selected);
	OnCloseRequested.Broadcast();
}

void UAttributeRightPanelWidget::HandleImportUserAsset()
{
	UAttributeSampleSubsystem* Subsystem = GetSampleSubsystem();
	if (!Subsystem || !AttributeAssetList)
	{
		return;
	}

	TArray<FString> FilePaths;
	if (!UAttributeRuntimeMeshLoader::OpenMeshFilesDialog(FilePaths))
	{
		return; // cancelled
	}
	QueueImport(FilePaths, 0);
}

void UAttributeRightPanelWidget::HandleImportFilesDropped(const TArray<FString>& AcceptedFiles, const TArray<FString>& RejectedFiles)
{
	if (AcceptedFiles.IsEmpty())
	{
		if (AttributeAssetList)
		{
			AttributeAssetList->SetStatus(FString::Printf(TEXT("No supported files dropped (%d skipped). Use .gltf, .glb or .fbx."), RejectedFiles.Num()), true);
		}
		return;
	}
	QueueImport(AcceptedFiles, RejectedFiles.Num());
}

void UAttributeRightPanelWidget::QueueImport(const TArray<FString>& Files, int32 SkippedCount)
{
	UWorld* World = GetWorld();
	if (!World || !AttributeAssetList || Files.IsEmpty())
	{
		return;
	}

	if (!bImportRunning)
	{
		// New batch (a drop during a running batch just extends it)
		bImportRunning = true;
		ImportLayer = CurrentLayer;
		ImportTotal = ImportSucceeded = ImportSkipped = 0;
		ImportErrors.Reset();
		LastImported.Reset();
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAttributeRightPanelWidget::ImportNextQueued));
	}

	ImportQueue.Append(Files);
	ImportTotal += Files.Num();
	ImportSkipped += SkippedCount;
	AttributeAssetList->SetStatus(FString::Printf(TEXT("Importing %d/%d: %s ..."),
		ImportTotal - ImportQueue.Num() + 1, ImportTotal, *FPaths::GetCleanFilename(ImportQueue[0])));
}

void UAttributeRightPanelWidget::ImportNextQueued()
{
	UAttributeSampleSubsystem* Subsystem = GetSampleSubsystem();
	if (!Subsystem || !AttributeAssetList || ImportQueue.IsEmpty())
	{
		FinishImport();
		return;
	}

	const FString FilePath = ImportQueue[0];
	ImportQueue.RemoveAt(0);

	FString Error;
	if (UAttributeAssetItem* Imported = Subsystem->ImportUserAsset(FilePath, ImportLayer, Error))
	{
		++ImportSucceeded;
		LastImported = Imported;
	}
	else
	{
		ImportErrors.Add(FString::Printf(TEXT("%s (%s)"), *FPaths::GetCleanFilename(FilePath), *Error));
	}

	if (ImportQueue.IsEmpty())
	{
		FinishImport();
		return;
	}

	// Show the next file, let this frame paint, then import it
	AttributeAssetList->SetStatus(FString::Printf(TEXT("Importing %d/%d: %s ..."),
		ImportTotal - ImportQueue.Num() + 1, ImportTotal, *FPaths::GetCleanFilename(ImportQueue[0])));
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &UAttributeRightPanelWidget::ImportNextQueued));
	}
	else
	{
		FinishImport();
	}
}

void UAttributeRightPanelWidget::FinishImport()
{
	bImportRunning = false;
	ImportQueue.Reset();
	if (!AttributeAssetList)
	{
		return;
	}

	// The catalog broadcast already refreshed the tiles; select the last import if its layer is still shown
	UAttributeAssetItem* Last = LastImported.Get();
	if (Last && ImportLayer == CurrentLayer)
	{
		AttributeAssetList->SelectItem(Last);
	}

	FString Message;
	if (ImportTotal == 1 && ImportSucceeded == 1 && Last)
	{
		Message = FString::Printf(TEXT("Imported '%s'. Press Apply to add it."), *Last->Data.Name);
	}
	else if (ImportTotal == 1 && ImportErrors.Num() == 1)
	{
		Message = TEXT("Import failed: ") + ImportErrors[0];
	}
	else
	{
		Message = FString::Printf(TEXT("Imported %d of %d file(s)."), ImportSucceeded, ImportTotal);
		if (ImportErrors.Num() > 0)
		{
			Message += TEXT(" Failed: ") + FString::Join(ImportErrors, TEXT(", "));
		}
		if (ImportSucceeded > 0)
		{
			Message += TEXT(" Select one and press Apply.");
		}
	}
	if (ImportSkipped > 0)
	{
		Message += FString::Printf(TEXT(" Skipped %d unsupported file(s)."), ImportSkipped);
	}
	AttributeAssetList->SetStatus(Message, ImportErrors.Num() > 0);
}

void UAttributeRightPanelWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}

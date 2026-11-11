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

	FString FilePath;
	if (!UAttributeRuntimeMeshLoader::OpenMeshFileDialog(FilePath))
	{
		return; // cancelled
	}

	AttributeAssetList->SetStatus(TEXT("Loading..."));

	FString Error;
	UAttributeAssetItem* Imported = Subsystem->ImportUserAsset(FilePath, CurrentLayer, Error);
	if (!Imported)
	{
		AttributeAssetList->SetStatus(FString::Printf(TEXT("Import failed: %s"), *Error), true);
		return;
	}

	// The catalog broadcast already refreshed the tiles
	AttributeAssetList->SelectItem(Imported);
	AttributeAssetList->SetStatus(FString::Printf(TEXT("Imported '%s'. Press Apply to add it."), *Imported->Data.Name));
}

void UAttributeRightPanelWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}

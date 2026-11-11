// Attribute Sample - WBP_AttributeAssetList

#include "AttributeAssetListWidget.h"

#include "AttributeAssetItemWidget.h"
#include "AttributeUIHelpers.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TileView.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UAttributeAssetListWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (AttributeUI::PrepareEmptyTree(WidgetTree))
	{
		BuildDefaultLayout();
	}

	ConfigureTileView();

	if (Apply)
	{
		Apply->OnClicked.AddUniqueDynamic(this, &UAttributeAssetListWidget::HandleApplyClicked);
	}
	if (ImportUserAsset)
	{
		ImportUserAsset->OnClicked.AddUniqueDynamic(this, &UAttributeAssetListWidget::HandleImportClicked);
	}
}

void UAttributeAssetListWidget::BuildDefaultLayout()
{
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// size = Columns x Rows entries (+ room for the scroll bar)
	USizeBox* ListSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ListSize"));
	ListSize->SetWidthOverride(Columns * EntryWidth + 16.f);
	ListSize->SetHeightOverride(VisibleRows * EntryHeight);

	UBorder* ListBackground = AttributeUI::MakeBorder(WidgetTree, AttributeUI::SectionColor, FMargin(4.f));
	List = WidgetTree->ConstructWidget<UTileView>(UTileView::StaticClass(), TEXT("List"));
	ListSize->AddChild(List);
	ListBackground->SetContent(ListSize);
	Root->AddChildToVerticalBox(ListBackground);

	StatusText = AttributeUI::MakeText(WidgetTree, TEXT("Select an asset, then Apply."), 9, AttributeUI::SubTextColor, false, TEXT("StatusText"));
	StatusText->SetAutoWrapText(true);
	Root->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.f, 8.f));

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Buttons"));
	ImportUserAsset = AttributeUI::MakeButton(WidgetTree, TEXT("Import User Asset..."), TEXT("ImportUserAsset"), AttributeUI::ButtonColor);
	Apply = AttributeUI::MakeButton(WidgetTree, TEXT("Apply"), TEXT("Apply"), AttributeUI::PrimaryColor);

	UHorizontalBoxSlot* ImportSlot = Buttons->AddChildToHorizontalBox(ImportUserAsset);
	ImportSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	ImportSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	Buttons->AddChildToHorizontalBox(Apply)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	Root->AddChildToVerticalBox(Buttons);
}

void UAttributeAssetListWidget::ConfigureTileView()
{
	if (!List)
	{
		return;
	}

	List->SetEntryWidth(EntryWidth);
	List->SetEntryHeight(EntryHeight);
	List->SetSelectionMode(ESelectionMode::Single);

	// EntryWidgetClass is protected on UListViewBase; set it through reflection when built from C++
	if (!List->GetEntryWidgetClass())
	{
		const TSubclassOf<UUserWidget> EntryClass = AttributeUI::ResolveClass(EntryWidgetClass, UAttributeAssetItemWidget::StaticClass());
		if (FClassProperty* EntryProperty = FindFProperty<FClassProperty>(UListViewBase::StaticClass(), TEXT("EntryWidgetClass")))
		{
			EntryProperty->SetObjectPropertyValue_InContainer(List, EntryClass.Get());
		}
	}
}

void UAttributeAssetListWidget::SetItems(const TArray<UAttributeAssetItem*>& Items)
{
	if (List)
	{
		List->SetListItems(Items);
	}
}

UAttributeAssetItem* UAttributeAssetListWidget::GetSelectedItem() const
{
	return List ? List->GetSelectedItem<UAttributeAssetItem>() : nullptr;
}

void UAttributeAssetListWidget::SelectItem(UAttributeAssetItem* Item)
{
	if (List && Item)
	{
		List->SetSelectedItem(Item);
		List->RequestScrollItemIntoView(Item);
	}
}

void UAttributeAssetListWidget::SetStatus(const FString& Message, bool bIsError)
{
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Message));
		StatusText->SetColorAndOpacity(FSlateColor(bIsError ? AttributeUI::ErrorColor : AttributeUI::SubTextColor));
	}
}

void UAttributeAssetListWidget::HandleApplyClicked()
{
	OnApplyClicked.Broadcast();
}

void UAttributeAssetListWidget::HandleImportClicked()
{
	OnImportClicked.Broadcast();
}

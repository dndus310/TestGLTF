// Attribute Sample - WBP_AttributeList

#include "AttributeListWidget.h"

#include "AttributeUIHelpers.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UAttributeListEntryProxy::HandleClicked()
{
	if (Owner.IsValid())
	{
		Owner->HandleEntryClicked(Item);
	}
}

void UAttributeListWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (AttributeUI::PrepareEmptyTree(WidgetTree))
	{
		BuildDefaultLayout();
	}

	if (AddAttribute)
	{
		AddAttribute->OnClicked.AddUniqueDynamic(this, &UAttributeListWidget::HandleAddAttributeClicked);
	}
}

void UAttributeListWidget::BuildDefaultLayout()
{
	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	UBorder* ListBackground = AttributeUI::MakeBorder(WidgetTree, AttributeUI::SectionColor, FMargin(6.f));
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("Scroll"));
	List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("List"));
	Scroll->AddChild(List);
	ListBackground->SetContent(Scroll);

	UVerticalBoxSlot* ListSlot = Root->AddChildToVerticalBox(ListBackground);
	ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	AddAttribute = AttributeUI::MakeButton(WidgetTree, TEXT("+  Add Attribute"), TEXT("AddAttribute"), AttributeUI::PrimaryColor, nullptr, 12);
	UVerticalBoxSlot* ButtonSlot = Root->AddChildToVerticalBox(AddAttribute);
	ButtonSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
}

void UAttributeListWidget::SetItems(const TArray<UAttributeAssetItem*>& Items)
{
	EntryProxies.Reset();
	if (!List)
	{
		return;
	}
	List->ClearChildren();

	if (Items.Num() == 0)
	{
		UTextBlock* Empty = AttributeUI::MakeText(WidgetTree, TEXT("No attributes. Use \"Add Attribute\"."), 10, AttributeUI::SubTextColor);
		List->AddChildToVerticalBox(Empty)->SetPadding(FMargin(6.f));
		return;
	}

	for (UAttributeAssetItem* Item : Items)
	{
		if (!Item)
		{
			continue;
		}

		UButton* EntryButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
		EntryButton->SetBackgroundColor(AttributeUI::ButtonColor);
		EntryButton->SetToolTipText(FText::FromString(TEXT("Click to spawn in the viewport")));

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		// Layer color bar
		USizeBox* BarSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		BarSize->SetWidthOverride(4.f);
		BarSize->SetHeightOverride(18.f);
		BarSize->AddChild(AttributeUI::MakeBorder(WidgetTree, AttributeUI::GetLayerColor(Item->Data.AssetType), FMargin(0.f)));
		UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(BarSize);
		BarSlot->SetVerticalAlignment(VAlign_Center);
		BarSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

		UTextBlock* NameText = AttributeUI::MakeText(WidgetTree, Item->Data.Name, 11, AttributeUI::TextColor);
		UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(NameText);
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);

		UTextBlock* TagText = AttributeUI::MakeText(WidgetTree, Item->IsUserImported() ? TEXT("USER") : TEXT("DT"), 8, AttributeUI::SubTextColor, true);
		Row->AddChildToHorizontalBox(TagText)->SetVerticalAlignment(VAlign_Center);

		if (UButtonSlot* RowSlot = Cast<UButtonSlot>(EntryButton->AddChild(Row)))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetPadding(FMargin(8.f, 4.f));
		}

		UAttributeListEntryProxy* Proxy = NewObject<UAttributeListEntryProxy>(this);
		Proxy->Item = Item;
		Proxy->Owner = this;
		EntryButton->OnClicked.AddDynamic(Proxy, &UAttributeListEntryProxy::HandleClicked);
		EntryProxies.Add(Proxy);

		List->AddChildToVerticalBox(EntryButton)->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}
}

void UAttributeListWidget::HandleEntryClicked(UAttributeAssetItem* Item)
{
	OnItemClicked.Broadcast(Item);
}

void UAttributeListWidget::HandleAddAttributeClicked()
{
	OnAddAttributeClicked.Broadcast();
}

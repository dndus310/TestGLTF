// Attribute Sample - WBP_LeftPanel

#include "AttributeLeftPanelWidget.h"

#include "AttributeListWidget.h"
#include "AttributeSampleSubsystem.h"
#include "AttributeUIHelpers.h"
#include "CoreAttribute.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UAttributeLeftPanelWidget::BuildDefaultLayout()
{
	UBorder* Frame = BuildPanelFrame(false, 300.f);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
	Frame->SetContent(Content);

	Content->AddChildToVerticalBox(AttributeUI::MakeText(WidgetTree, TEXT("ATTRIBUTES"), 16, AttributeUI::TextColor, true))
		->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
	Content->AddChildToVerticalBox(AttributeUI::MakeText(WidgetTree, TEXT("Click an item to spawn it"), 9, AttributeUI::SubTextColor))
		->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

	Content->AddChildToVerticalBox(BuildLayerTabs())->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

	const TSubclassOf<UAttributeListWidget> ListClass = AttributeUI::ResolveClass(AttributeListClass, UAttributeListWidget::StaticClass());
	AttributeList = WidgetTree->ConstructWidget<UAttributeListWidget>(ListClass, TEXT("AttributeList"));
	UVerticalBoxSlot* ListSlot = Content->AddChildToVerticalBox(AttributeList);
	ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	StatusText = AttributeUI::MakeText(WidgetTree, TEXT(""), 9, AttributeUI::SubTextColor, false, TEXT("StatusText"));
	StatusText->SetAutoWrapText(true);
	Content->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
}

void UAttributeLeftPanelWidget::OnPanelInitialized()
{
	if (AttributeList)
	{
		AttributeList->OnItemClicked.AddUObject(this, &UAttributeLeftPanelWidget::HandleItemClicked);
		AttributeList->OnAddAttributeClicked.AddUObject(this, &UAttributeLeftPanelWidget::HandleAddAttribute);
	}

	if (UAttributeSampleSubsystem* Subsystem = GetSampleSubsystem())
	{
		ListChangedHandle = Subsystem->OnAttributeListChanged.AddUObject(this, &UAttributeLeftPanelWidget::RefreshList);
	}
}

void UAttributeLeftPanelWidget::NativeDestruct()
{
	if (UAttributeSampleSubsystem* Subsystem = GetSampleSubsystem())
	{
		Subsystem->OnAttributeListChanged.Remove(ListChangedHandle);
	}
	Super::NativeDestruct();
}

void UAttributeLeftPanelWidget::OnLayerChanged()
{
	RefreshList();
}

void UAttributeLeftPanelWidget::RefreshList()
{
	UAttributeSampleSubsystem* Subsystem = GetSampleSubsystem();
	if (AttributeList && Subsystem)
	{
		AttributeList->SetItems(Subsystem->GetAttributeListItems(CurrentLayer));
	}
}

void UAttributeLeftPanelWidget::HandleItemClicked(UAttributeAssetItem* Item)
{
	UAttributeSampleSubsystem* Subsystem = GetSampleSubsystem();
	if (!Subsystem || !Item)
	{
		return;
	}

	const ACoreAttribute* Spawned = Subsystem->SpawnAttribute(Item);
	if (StatusText)
	{
		StatusText->SetText(FText::FromString(Spawned
			? FString::Printf(TEXT("Spawned %s (%s)"), *Item->Data.Name, *AttributeUI::GetLayerLabel(Item->Data.AssetType))
			: FString::Printf(TEXT("Failed to spawn %s"), *Item->Data.Name)));
		StatusText->SetColorAndOpacity(FSlateColor(Spawned ? AttributeUI::SubTextColor : AttributeUI::ErrorColor));
	}
}

void UAttributeLeftPanelWidget::HandleAddAttribute()
{
	OnAddAttributeRequested.Broadcast(CurrentLayer);
}

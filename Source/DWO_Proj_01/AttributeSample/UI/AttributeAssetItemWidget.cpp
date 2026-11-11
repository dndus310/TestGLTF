// Attribute Sample - WBP_AttributeAssetItem

#include "AttributeAssetItemWidget.h"

#include "AttributeSampleSubsystem.h"
#include "AttributeUIHelpers.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture.h"
#include "Engine/World.h"

void UAttributeAssetItemWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (AttributeUI::PrepareEmptyTree(WidgetTree))
	{
		BuildDefaultLayout();
	}
}

void UAttributeAssetItemWidget::BuildDefaultLayout()
{
	Background = AttributeUI::MakeBorder(WidgetTree, AttributeUI::SectionColor, FMargin(6.f), TEXT("Background"));
	WidgetTree->RootWidget = Background;

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Content"));
	Background->SetContent(Content);

	USizeBox* ThumbnailBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ThumbnailBox"));
	ThumbnailBox->SetWidthOverride(96.f);
	ThumbnailBox->SetHeightOverride(96.f);
	Thumbnail = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Thumbnail"));
	ThumbnailBox->AddChild(Thumbnail);
	UVerticalBoxSlot* ThumbnailSlot = Content->AddChildToVerticalBox(ThumbnailBox);
	ThumbnailSlot->SetHorizontalAlignment(HAlign_Center);

	NameText = AttributeUI::MakeText(WidgetTree, TEXT(""), 10, AttributeUI::TextColor, true, TEXT("NameText"));
	NameText->SetJustification(ETextJustify::Center);
	NameText->SetClipping(EWidgetClipping::ClipToBounds);
	Content->AddChildToVerticalBox(NameText)->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

	TagText = AttributeUI::MakeText(WidgetTree, TEXT(""), 8, AttributeUI::SubTextColor, false, TEXT("TagText"));
	TagText->SetJustification(ETextJustify::Center);
	Content->AddChildToVerticalBox(TagText);
}

void UAttributeAssetItemWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	Item = Cast<UAttributeAssetItem>(ListItemObject);
	if (!Item)
	{
		return;
	}

	if (NameText)
	{
		NameText->SetText(FText::FromString(Item->Data.Name));
	}

	if (TagText)
	{
		const FString Tag = AttributeUI::GetLayerLabel(Item->Data.AssetType) + (Item->IsUserImported() ? TEXT(" | USER") : TEXT(""));
		TagText->SetText(FText::FromString(Tag));
		TagText->SetColorAndOpacity(FSlateColor(AttributeUI::GetLayerColor(Item->Data.AssetType)));
	}

	if (Thumbnail)
	{
		UTexture* Texture = nullptr;
		if (UAttributeSampleSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UAttributeSampleSubsystem>() : nullptr)
		{
			Texture = Subsystem->GetOrCreateThumbnail(Item);
		}

		if (Texture)
		{
			Thumbnail->SetBrushResourceObject(Texture);
			Thumbnail->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			Thumbnail->SetBrushResourceObject(nullptr);
			Thumbnail->SetColorAndOpacity(AttributeUI::ButtonColor);
		}
		Thumbnail->SetDesiredSizeOverride(FVector2D(96.f, 96.f));
	}

	NativeOnItemSelectionChanged(IsListItemSelected());
}

void UAttributeAssetItemWidget::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);

	if (Background)
	{
		Background->SetBrushColor(bIsSelected ? AttributeUI::PrimaryColor : AttributeUI::SectionColor);
	}

	// Layer-colored tag would vanish on the blue selection background
	if (TagText && Item)
	{
		TagText->SetColorAndOpacity(FSlateColor(bIsSelected ? AttributeUI::TextColor : AttributeUI::GetLayerColor(Item->Data.AssetType)));
	}
}

// Attribute Sample - small helpers to build UMG widget trees from C++

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPtr.h"
#include "AttributeSampleTypes.h"

namespace AttributeUI
{
	inline const FLinearColor PanelColor(0.012f, 0.014f, 0.018f, 0.90f);
	inline const FLinearColor SectionColor(0.035f, 0.040f, 0.050f, 1.0f);
	inline const FLinearColor ButtonColor(0.18f, 0.20f, 0.24f, 1.0f);
	inline const FLinearColor PrimaryColor(0.10f, 0.45f, 0.95f, 1.0f);
	inline const FLinearColor TextColor(0.92f, 0.93f, 0.95f, 1.0f);
	inline const FLinearColor SubTextColor(0.55f, 0.58f, 0.63f, 1.0f);
	inline const FLinearColor ErrorColor(1.0f, 0.35f, 0.30f, 1.0f);
	inline const FLinearColor BuildingColor(0.10f, 0.55f, 1.00f, 1.0f);
	inline const FLinearColor LoadColor(1.00f, 0.45f, 0.05f, 1.0f);

	inline FLinearColor GetLayerColor(EAttributeAssetType Type)
	{
		return Type == EAttributeAssetType::Load ? LoadColor : BuildingColor;
	}

	inline FString GetLayerLabel(EAttributeAssetType Type)
	{
		return Type == EAttributeAssetType::Load ? TEXT("LOAD") : TEXT("BUILDING");
	}

	/** Uses the Widget Blueprint when its package exists, otherwise the native fallback (no load warnings). */
	template <typename T>
	TSubclassOf<T> ResolveClass(const TSoftClassPtr<T>& SoftClass, UClass* Fallback)
	{
		if (!SoftClass.IsNull())
		{
			const FString PackageName = SoftClass.ToSoftObjectPath().GetLongPackageName();
			if (FPackageName::DoesPackageExist(PackageName))
			{
				if (UClass* Loaded = SoftClass.LoadSynchronous())
				{
					return Loaded;
				}
			}
		}
		return Fallback;
	}

	/**
	 * True when the tree has no designer content (no root, or only an empty root panel that the
	 * Widget Blueprint factory adds by default). Clears that empty root so C++ can build the layout.
	 */
	inline bool PrepareEmptyTree(UWidgetTree* Tree)
	{
		if (!Tree)
		{
			return false;
		}
		if (!Tree->RootWidget)
		{
			return true;
		}
		const UPanelWidget* RootPanel = Cast<UPanelWidget>(Tree->RootWidget);
		if (RootPanel && RootPanel->GetChildrenCount() == 0)
		{
			Tree->RootWidget = nullptr;
			return true;
		}
		return false;
	}

	inline UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, int32 Size, const FLinearColor& Color, bool bBold = false, FName Name = NAME_None)
	{
		UTextBlock* TextBlock = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		TextBlock->SetText(FText::FromString(Text));
		FSlateFontInfo Font = TextBlock->GetFont();
		Font.Size = Size;
		Font.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
		TextBlock->SetFont(Font);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		return TextBlock;
	}

	inline UButton* MakeButton(UWidgetTree* Tree, const FString& Label, FName Name, const FLinearColor& Color, UTextBlock** OutLabel = nullptr, int32 FontSize = 11)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Button->SetBackgroundColor(Color);

		UTextBlock* LabelText = MakeText(Tree, Label, FontSize, TextColor, true);
		LabelText->SetJustification(ETextJustify::Center);
		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Button->AddChild(LabelText)))
		{
			ButtonSlot->SetPadding(FMargin(10.f, 5.f));
		}

		if (OutLabel)
		{
			*OutLabel = LabelText;
		}
		return Button;
	}

	inline UBorder* MakeBorder(UWidgetTree* Tree, const FLinearColor& Color, const FMargin& Padding, FName Name = NAME_None)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Border->SetBrushColor(Color);
		Border->SetPadding(Padding);
		return Border;
	}
}

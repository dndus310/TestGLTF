// Attribute Sample - shared base for Left/Right panels

#include "AttributeLayerPanelWidget.h"

#include "AttributeSampleSubsystem.h"
#include "AttributeUIHelpers.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Engine/World.h"

void UAttributeLayerPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (AttributeUI::PrepareEmptyTree(WidgetTree))
	{
		BuildDefaultLayout();
	}

	if (BuildingTab)
	{
		BuildingTab->OnClicked.AddUniqueDynamic(this, &UAttributeLayerPanelWidget::HandleBuildingTabClicked);
	}
	if (LoadTab)
	{
		LoadTab->OnClicked.AddUniqueDynamic(this, &UAttributeLayerPanelWidget::HandleLoadTabClicked);
	}

	OnPanelInitialized();
	RefreshTabVisuals();
	OnLayerChanged();
}

void UAttributeLayerPanelWidget::SetLayer(EAttributeAssetType NewLayer)
{
	if (CurrentLayer == NewLayer)
	{
		return;
	}

	CurrentLayer = NewLayer;
	RefreshTabVisuals();
	OnLayerChanged();
}

UBorder* UAttributeLayerPanelWidget::BuildPanelFrame(bool bRightSide, float Width)
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	UBorder* Frame = AttributeUI::MakeBorder(WidgetTree, AttributeUI::PanelColor, FMargin(14.f), TEXT("PanelFrame"));
	UCanvasPanelSlot* FrameSlot = Root->AddChildToCanvas(Frame);

	const float Margin = 16.f;
	if (bRightSide)
	{
		FrameSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
		FrameSlot->SetAlignment(FVector2D(1.f, 0.f));
		FrameSlot->SetOffsets(FMargin(-Margin, Margin, Width, Margin));
	}
	else
	{
		FrameSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
		FrameSlot->SetAlignment(FVector2D(0.f, 0.f));
		FrameSlot->SetOffsets(FMargin(Margin, Margin, Width, Margin));
	}

	return Frame;
}

UHorizontalBox* UAttributeLayerPanelWidget::BuildLayerTabs()
{
	Layer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Layer"));
	BuildingTab = AttributeUI::MakeButton(WidgetTree, TEXT("Building"), TEXT("BuildingTab"), AttributeUI::ButtonColor);
	LoadTab = AttributeUI::MakeButton(WidgetTree, TEXT("Load"), TEXT("LoadTab"), AttributeUI::ButtonColor);

	for (UButton* Tab : { BuildingTab.Get(), LoadTab.Get() })
	{
		UHorizontalBoxSlot* TabSlot = Layer->AddChildToHorizontalBox(Tab);
		TabSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TabSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
	}

	return Layer;
}

void UAttributeLayerPanelWidget::RefreshTabVisuals()
{
	const FLinearColor Dim = AttributeUI::ButtonColor;
	if (BuildingTab)
	{
		BuildingTab->SetBackgroundColor(CurrentLayer == EAttributeAssetType::Building ? AttributeUI::BuildingColor : Dim);
	}
	if (LoadTab)
	{
		LoadTab->SetBackgroundColor(CurrentLayer == EAttributeAssetType::Load ? AttributeUI::LoadColor : Dim);
	}
}

UAttributeSampleSubsystem* UAttributeLayerPanelWidget::GetSampleSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UAttributeSampleSubsystem>() : nullptr;
}

void UAttributeLayerPanelWidget::HandleBuildingTabClicked()
{
	SetLayer(EAttributeAssetType::Building);
}

void UAttributeLayerPanelWidget::HandleLoadTabClicked()
{
	SetLayer(EAttributeAssetType::Load);
}

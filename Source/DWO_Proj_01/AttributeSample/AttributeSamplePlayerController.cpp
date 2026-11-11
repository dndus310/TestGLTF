// Attribute Sample - creates the panels and drives the camera

#include "AttributeSamplePlayerController.h"

#include "AttributeSampleCameraPawn.h"
#include "AttributeSampleSubsystem.h"
#include "UI/AttributeLeftPanelWidget.h"
#include "UI/AttributeRightPanelWidget.h"
#include "UI/AttributeUIHelpers.h"

#include "Components/InputComponent.h"
#include "Engine/DataTable.h"
#include "InputCoreTypes.h"

AAttributeSamplePlayerController::AAttributeSamplePlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = true;

	AttributeDataTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/AttributeSample/Data/DT_AttributeAssetData.DT_AttributeAssetData")));
	LeftPanelClass = TSoftClassPtr<UAttributeLeftPanelWidget>(FSoftObjectPath(TEXT("/Game/AttributeSample/UI/WBP_LeftPanel.WBP_LeftPanel_C")));
	RightPanelClass = TSoftClassPtr<UAttributeRightPanelWidget>(FSoftObjectPath(TEXT("/Game/AttributeSample/UI/WBP_RightPanel.WBP_RightPanel_C")));
}

void AAttributeSamplePlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (UAttributeSampleSubsystem* Subsystem = GetWorld()->GetSubsystem<UAttributeSampleSubsystem>())
	{
		Subsystem->InitializeFromDataTable(AttributeDataTable.LoadSynchronous());
	}

	LeftPanel = CreateWidget<UAttributeLeftPanelWidget>(this, AttributeUI::ResolveClass(LeftPanelClass, UAttributeLeftPanelWidget::StaticClass()));
	RightPanel = CreateWidget<UAttributeRightPanelWidget>(this, AttributeUI::ResolveClass(RightPanelClass, UAttributeRightPanelWidget::StaticClass()));

	if (LeftPanel)
	{
		LeftPanel->AddToViewport(10);
		LeftPanel->OnAddAttributeRequested.AddUObject(this, &AAttributeSamplePlayerController::ShowRightPanel);
	}

	if (RightPanel)
	{
		RightPanel->AddToViewport(20);
		RightPanel->SetVisibility(ESlateVisibility::Collapsed);
		RightPanel->OnCloseRequested.AddUObject(this, &AAttributeSamplePlayerController::HideRightPanel);
		RightPanel->OnAttributeApplied.AddWeakLambda(this, [this](UAttributeAssetItem* Item)
		{
			// Show the layer the new attribute was added to
			if (LeftPanel && Item)
			{
				LeftPanel->SetLayer(Item->Data.AssetType);
			}
		});
	}

	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void AAttributeSamplePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AAttributeSamplePlayerController::OnOrbitPressed);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AAttributeSamplePlayerController::OnOrbitReleased);
	InputComponent->BindAxisKey(EKeys::MouseX, this, &AAttributeSamplePlayerController::OnMouseX);
	InputComponent->BindAxisKey(EKeys::MouseY, this, &AAttributeSamplePlayerController::OnMouseY);
	InputComponent->BindAxisKey(EKeys::MouseWheelAxis, this, &AAttributeSamplePlayerController::OnMouseWheel);
}

void AAttributeSamplePlayerController::ShowRightPanel(EAttributeAssetType Layer)
{
	if (RightPanel)
	{
		RightPanel->SetLayer(Layer);
		RightPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void AAttributeSamplePlayerController::HideRightPanel()
{
	if (RightPanel)
	{
		RightPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void AAttributeSamplePlayerController::AttributeSpawnAll()
{
	if (UAttributeSampleSubsystem* Subsystem = GetWorld()->GetSubsystem<UAttributeSampleSubsystem>())
	{
		for (const EAttributeAssetType Type : { EAttributeAssetType::Building, EAttributeAssetType::Load })
		{
			for (UAttributeAssetItem* Item : Subsystem->GetAttributeListItems(Type))
			{
				Subsystem->SpawnAttribute(Item);
			}
		}
	}
}

void AAttributeSamplePlayerController::AttributeShowLibrary(int32 Layer)
{
	ShowRightPanel(Layer == 1 ? EAttributeAssetType::Load : EAttributeAssetType::Building);
}

void AAttributeSamplePlayerController::AttributeImportFile(const FString& FilePath, int32 Layer)
{
	UAttributeSampleSubsystem* Subsystem = GetWorld()->GetSubsystem<UAttributeSampleSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	FString Error;
	const EAttributeAssetType Type = Layer == 1 ? EAttributeAssetType::Load : EAttributeAssetType::Building;
	if (UAttributeAssetItem* Item = Subsystem->ImportUserAsset(FilePath.TrimQuotes(), Type, Error))
	{
		Subsystem->AddAttribute(Item);
		Subsystem->SpawnAttribute(Item);
	}
	else
	{
		ClientMessage(FString::Printf(TEXT("Import failed: %s"), *Error));
	}
}

void AAttributeSamplePlayerController::OnMouseX(float Value)
{
	if (bOrbiting && Value != 0.f)
	{
		if (AAttributeSampleCameraPawn* CameraPawn = GetPawn<AAttributeSampleCameraPawn>())
		{
			CameraPawn->AddOrbitInput(Value * 2.f, 0.f);
		}
	}
}

void AAttributeSamplePlayerController::OnMouseY(float Value)
{
	if (bOrbiting && Value != 0.f)
	{
		if (AAttributeSampleCameraPawn* CameraPawn = GetPawn<AAttributeSampleCameraPawn>())
		{
			CameraPawn->AddOrbitInput(0.f, Value * 2.f);
		}
	}
}

void AAttributeSamplePlayerController::OnMouseWheel(float Value)
{
	if (Value != 0.f)
	{
		if (AAttributeSampleCameraPawn* CameraPawn = GetPawn<AAttributeSampleCameraPawn>())
		{
			CameraPawn->AddZoomInput(Value);
		}
	}
}

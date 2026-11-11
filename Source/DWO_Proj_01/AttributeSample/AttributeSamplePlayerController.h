// Attribute Sample - creates the panels and drives the camera

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AttributeSampleTypes.h"
#include "AttributeSamplePlayerController.generated.h"

class UAttributeLeftPanelWidget;
class UAttributeRightPanelWidget;
class UDataTable;

UCLASS()
class DWO_PROJ_01_API AAttributeSamplePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAttributeSamplePlayerController();

	UFUNCTION(BlueprintCallable, Category = "Attribute|UI")
	void ShowRightPanel(EAttributeAssetType Layer);

	UFUNCTION(BlueprintCallable, Category = "Attribute|UI")
	void HideRightPanel();

	/** Console: spawns every entry of the attribute list once (quick visual check). */
	UFUNCTION(Exec)
	void AttributeSpawnAll();

	/** Console: opens the RightPanel for the given layer (0 = Building, 1 = Load). */
	UFUNCTION(Exec)
	void AttributeShowLibrary(int32 Layer = 0);

	/** Console: imports a file without the dialog, adds it to the list and spawns it. */
	UFUNCTION(Exec)
	void AttributeImportFile(const FString& FilePath, int32 Layer = 0);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditAnywhere, Category = "Attribute")
	TSoftObjectPtr<UDataTable> AttributeDataTable;

	/** Widget Blueprints are used when they exist, otherwise the native C++ classes. */
	UPROPERTY(EditAnywhere, Category = "Attribute|UI")
	TSoftClassPtr<UAttributeLeftPanelWidget> LeftPanelClass;

	UPROPERTY(EditAnywhere, Category = "Attribute|UI")
	TSoftClassPtr<UAttributeRightPanelWidget> RightPanelClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Attribute|UI")
	TObjectPtr<UAttributeLeftPanelWidget> LeftPanel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Attribute|UI")
	TObjectPtr<UAttributeRightPanelWidget> RightPanel;

private:
	void OnOrbitPressed() { bOrbiting = true; }
	void OnOrbitReleased() { bOrbiting = false; }
	void OnMouseX(float Value);
	void OnMouseY(float Value);
	void OnMouseWheel(float Value);

	bool bOrbiting = false;
};

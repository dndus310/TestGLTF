// Attribute Sample - orbit camera (right mouse drag = orbit, wheel = zoom)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AttributeSampleCameraPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class DWO_PROJ_01_API AAttributeSampleCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AAttributeSampleCameraPawn();

	void AddOrbitInput(float DeltaYaw, float DeltaPitch);
	void AddZoomInput(float Delta);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> Boom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MinArmLength = 500.f;

	UPROPERTY(EditAnywhere, Category = "Camera")
	float MaxArmLength = 8000.f;
};

// Attribute Sample - orbit camera

#include "AttributeSampleCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

AAttributeSampleCameraPawn::AAttributeSampleCameraPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(RootComponent);
	Boom->TargetArmLength = 2600.f;
	Boom->bDoCollisionTest = false;
	Boom->bUsePawnControlRotation = false;
	Boom->SetRelativeRotation(FRotator(-40.f, -30.f, 0.f));

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom);
}

void AAttributeSampleCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	// Always look at the center of the spawn area regardless of the PlayerStart position
	SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
}

void AAttributeSampleCameraPawn::AddOrbitInput(float DeltaYaw, float DeltaPitch)
{
	FRotator Rotation = Boom->GetRelativeRotation();
	Rotation.Yaw += DeltaYaw;
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch + DeltaPitch, -85.f, -5.f);
	Boom->SetRelativeRotation(Rotation);
}

void AAttributeSampleCameraPawn::AddZoomInput(float Delta)
{
	Boom->TargetArmLength = FMath::Clamp(Boom->TargetArmLength * (1.f - Delta * 0.1f), MinArmLength, MaxArmLength);
}

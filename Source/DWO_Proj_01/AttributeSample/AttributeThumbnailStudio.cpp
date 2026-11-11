// Attribute Sample - renders mesh thumbnails at runtime

#include "AttributeThumbnailStudio.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "TimerManager.h"

AAttributeThumbnailStudio::AAttributeThumbnailStudio()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(Root);
	PreviewMesh->SetMobility(EComponentMobility::Movable);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetVisibleInSceneCaptureOnly(true);
	PreviewMesh->SetCastShadow(false);
	// Always render the full-quality LOD, independent of streaming / screen size
	PreviewMesh->SetForcedLodModel(1);

	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Root);
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->FOVAngle = 30.f;
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetEyeAdaptation(false);
	Capture->ShowFlags.SetMotionBlur(false);
	Capture->ShowFlags.SetBloom(false);
	Capture->ShowFlags.SetTemporalAA(false);
	Capture->ShowFlags.SetAntiAliasing(true);

	Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
	Capture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	Capture->PostProcessSettings.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Capture->PostProcessSettings.AutoExposureApplyPhysicalCameraExposure = false;
	Capture->PostProcessBlendWeight = 1.f;
}

UTextureRenderTarget2D* AAttributeThumbnailStudio::RenderThumbnail(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return nullptr;
	}

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->ClearColor = FLinearColor(0.02f, 0.02f, 0.025f, 1.f);
	RenderTarget->InitAutoFormat(Resolution, Resolution);
	RenderTarget->UpdateResourceImmediate(true);

	FPendingCapture Pending;
	Pending.Mesh = Mesh;
	Pending.Target = RenderTarget;
	Queue.Add(Pending);

	if (!QueueTimer.IsValid())
	{
		QueueTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AAttributeThumbnailStudio::ProcessQueue);
	}

	return RenderTarget;
}

void AAttributeThumbnailStudio::StageMesh(UStaticMesh* Mesh)
{
	PreviewMesh->SetStaticMesh(Mesh);
	Capture->ShowOnlyComponents.Reset();
	Capture->ShowOnlyComponent(PreviewMesh);

	// Frame the bounds from a 3/4 view
	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const float Radius = FMath::Max(Bounds.SphereRadius, 1.f);
	const float Distance = Radius / FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle * 0.5f)) * 1.1f;
	const FVector ViewDir = FRotator(-30.f, 45.f, 0.f).Vector();
	const FVector Target = GetActorTransform().TransformPosition(Bounds.Origin);
	Capture->SetWorldLocationAndRotation(Target - ViewDir * Distance, ViewDir.Rotation());

	// Push the new mesh to the render thread now instead of at the end of the frame
	PreviewMesh->MarkRenderStateDirty();
	GetWorld()->SendAllEndOfFrameUpdates();
}

void AAttributeThumbnailStudio::CaptureStaged(UTextureRenderTarget2D* Target)
{
	Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
	Capture->PostProcessSettings.AutoExposureBias = ExposureBias;

	Capture->TextureTarget = Target;
	Capture->CaptureScene();
	Capture->TextureTarget = nullptr;
}

void AAttributeThumbnailStudio::ProcessQueue()
{
	QueueTimer.Invalidate();

	// Drop entries whose mesh or texture went away
	Queue.RemoveAll([](const FPendingCapture& Item) { return !Item.Mesh.IsValid() || !Item.Target.IsValid(); });
	if (Queue.Num() == 0)
	{
		return;
	}

	FPendingCapture& Current = Queue[0];
	if (!bStaged)
	{
		// Frame N: assign the mesh. Frame N+1: capture it (render proxy exists by then).
		StageMesh(Current.Mesh.Get());
		bStaged = true;
	}
	else
	{
		CaptureStaged(Current.Target.Get());
		// Capture twice so streamed-in LODs / textures make it into the final image
		if (--Current.PassesLeft <= 0)
		{
			Queue.RemoveAt(0);
			bStaged = false;
		}
	}

	if (Queue.Num() > 0)
	{
		QueueTimer = GetWorldTimerManager().SetTimerForNextTick(this, &AAttributeThumbnailStudio::ProcessQueue);
	}
}

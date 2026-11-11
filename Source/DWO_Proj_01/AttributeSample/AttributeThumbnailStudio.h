// Attribute Sample - renders mesh thumbnails at runtime (editor thumbnails do not exist in packaged builds)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AttributeThumbnailStudio.generated.h"

class USceneCaptureComponent2D;
class UStaticMesh;
class UStaticMeshComponent;
class UTextureRenderTarget2D;

/**
 * Hidden actor far below the level: places a mesh in front of a scene capture and renders it to a texture.
 * The render target is returned immediately and (re)captured over the next frames, one mesh per frame,
 * because a freshly assigned mesh needs a frame before its render proxy is ready.
 */
UCLASS(NotPlaceable)
class DWO_PROJ_01_API AAttributeThumbnailStudio : public AActor
{
	GENERATED_BODY()

public:
	AAttributeThumbnailStudio();

	UFUNCTION(BlueprintCallable, Category = "Attribute|Thumbnail")
	UTextureRenderTarget2D* RenderThumbnail(UStaticMesh* Mesh);

	UPROPERTY(EditAnywhere, Category = "Attribute|Thumbnail")
	int32 Resolution = 128;

	/** Manual exposure for the capture; raise if thumbnails look dark. */
	UPROPERTY(EditAnywhere, Category = "Attribute|Thumbnail")
	float ExposureBias = 0.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Attribute|Thumbnail")
	TObjectPtr<UStaticMeshComponent> PreviewMesh;

	UPROPERTY(VisibleAnywhere, Category = "Attribute|Thumbnail")
	TObjectPtr<USceneCaptureComponent2D> Capture;

private:
	struct FPendingCapture
	{
		TWeakObjectPtr<UStaticMesh> Mesh;
		TWeakObjectPtr<UTextureRenderTarget2D> Target;
		int32 PassesLeft = 2;
	};

	/** Step 1: place the mesh and flush its render state. */
	void StageMesh(UStaticMesh* Mesh);

	/** Step 2 (next frame): capture the staged mesh. */
	void CaptureStaged(UTextureRenderTarget2D* Target);

	void ProcessQueue();

	TArray<FPendingCapture> Queue;
	bool bStaged = false;
	FTimerHandle QueueTimer;
};

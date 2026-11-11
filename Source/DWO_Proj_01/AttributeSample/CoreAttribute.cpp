// Attribute Sample - base attribute actor

#include "CoreAttribute.h"

#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ACoreAttribute::ACoreAttribute()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	RootComponent = SceneRoot;

	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	SplineComponent->SetupAttachment(SceneRoot);
	SplineComponent->SetClosedLoop(true);

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComponent->SetupAttachment(SceneRoot);
	StaticMeshComponent->SetMobility(EComponentMobility::Movable);
	StaticMeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	DefaultMesh = CubeMesh.Object;
	OutlineSegmentMesh = CubeMesh.Object;
	OutlineMaterial = ShapeMaterial.Object;
}

void ACoreAttribute::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!StaticMeshComponent->GetStaticMesh())
	{
		StaticMeshComponent->SetStaticMesh(DefaultMesh);
	}
}

void ACoreAttribute::BeginPlay()
{
	Super::BeginPlay();

	// Placed in the level without an applied asset -> show the default mesh with its outline
	if (SplineComponent->GetNumberOfSplinePoints() < 3)
	{
		ApplyRuntimeSetMeshAsset(StaticMeshComponent->GetStaticMesh());
	}
}

bool ACoreAttribute::ApplyRuntimeSetMeshAsset(UStaticMesh* NewMesh)
{
	UStaticMesh* MeshToApply = NewMesh ? NewMesh : DefaultMesh.Get();
	if (!MeshToApply)
	{
		return false;
	}

	StaticMeshComponent->SetStaticMesh(MeshToApply);

	// Put the pivot on the bottom center so the mesh rests on the actor location (= ground point)
	const FBox MeshBox = MeshToApply->GetBoundingBox();
	const FVector Center = MeshBox.GetCenter();
	StaticMeshComponent->SetRelativeLocation(FVector(-Center.X, -Center.Y, -MeshBox.Min.Z));

	MakeSplinePointsByShapeBounding();
	return true;
}

void ACoreAttribute::MakeSplinePointsByShapeBounding()
{
	UStaticMesh* Mesh = StaticMeshComponent->GetStaticMesh();
	if (!Mesh)
	{
		return;
	}

	// Mesh bounds in actor space
	const FBox LocalBox = Mesh->GetBoundingBox().TransformBy(StaticMeshComponent->GetRelativeTransform());
	const float Z = LocalBox.Min.Z + OutlineHeightOffset;
	const FVector Min(LocalBox.Min.X - OutlinePadding, LocalBox.Min.Y - OutlinePadding, Z);
	const FVector Max(LocalBox.Max.X + OutlinePadding, LocalBox.Max.Y + OutlinePadding, Z);

	const FVector Corners[4] =
	{
		FVector(Min.X, Min.Y, Z),
		FVector(Max.X, Min.Y, Z),
		FVector(Max.X, Max.Y, Z),
		FVector(Min.X, Max.Y, Z),
	};

	SplineComponent->ClearSplinePoints(false);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Corners); ++Index)
	{
		SplineComponent->AddSplinePoint(Corners[Index], ESplineCoordinateSpace::Local, false);
		SplineComponent->SetSplinePointType(Index, ESplinePointType::Linear, false);
	}
	SplineComponent->SetClosedLoop(true, false);
	SplineComponent->UpdateSpline();

	RebuildOutlineVisual();
}

void ACoreAttribute::RebuildOutlineVisual()
{
	for (UStaticMeshComponent* Segment : OutlineSegments)
	{
		if (Segment)
		{
			Segment->DestroyComponent();
		}
	}
	OutlineSegments.Reset();

	if (!bShowOutline || !OutlineSegmentMesh)
	{
		return;
	}

	if (!OutlineMID && OutlineMaterial)
	{
		OutlineMID = UMaterialInstanceDynamic::Create(OutlineMaterial, this);
		OutlineMID->SetVectorParameterValue(TEXT("Color"), OutlineColor);
	}

	const int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();
	if (NumPoints < 2)
	{
		return;
	}

	// The footprint spline is linear, so each segment is a thin stretched cube
	// (plain static meshes keep any material usable; spline meshes need bUsedWithSplineMeshes).
	const float CrossScale = OutlineThickness / 100.f;
	const int32 NumSegments = SplineComponent->IsClosedLoop() ? NumPoints : NumPoints - 1;

	for (int32 Index = 0; Index < NumSegments; ++Index)
	{
		const FVector Start = SplineComponent->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::Local);
		const FVector End = SplineComponent->GetLocationAtSplinePoint((Index + 1) % NumPoints, ESplineCoordinateSpace::Local);
		const FVector Delta = End - Start;
		const float Length = Delta.Size();
		if (Length <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(this);
		Segment->SetMobility(EComponentMobility::Movable);
		Segment->SetupAttachment(SplineComponent);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetCastShadow(false);
		Segment->SetStaticMesh(OutlineSegmentMesh);
		if (OutlineMID)
		{
			Segment->SetMaterial(0, OutlineMID);
		}
		Segment->SetRelativeTransform(FTransform(
			Delta.Rotation(),
			(Start + End) * 0.5f,
			FVector((Length + OutlineThickness) / 100.f, CrossScale, CrossScale)));
		Segment->RegisterComponent();

		OutlineSegments.Add(Segment);
	}
}

// Attribute Sample - owns the asset catalog, the attribute list and spawning

#include "AttributeSampleSubsystem.h"

#include "AttributeRuntimeMeshLoader.h"
#include "AttributeThumbnailStudio.h"
#include "BuildingAttribute.h"
#include "CoreAttribute.h"
#include "LoadAttribute.h"
#include "UserAssetLibrary.h"

#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogAttributeSample, Log, All);

bool UAttributeSampleSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAttributeSampleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	BuildingAttributeClass = ABuildingAttribute::StaticClass();
	LoadAttributeClass = ALoadAttribute::StaticClass();
}

void UAttributeSampleSubsystem::Deinitialize()
{
	if (UUserAssetLibrary* Library = GetUserAssetLibrary())
	{
		Library->OnAssetRestored.Remove(AssetRestoredHandle);
	}
	Super::Deinitialize();
}

UUserAssetLibrary* UAttributeSampleSubsystem::GetUserAssetLibrary() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UUserAssetLibrary>() : nullptr;
}

void UAttributeSampleSubsystem::InitializeFromDataTable(UDataTable* DataTable)
{
	Catalog.Reset();
	AttributeList.Reset();

	if (!DataTable)
	{
		UE_LOG(LogAttributeSample, Warning, TEXT("DT_AttributeAssetData is missing. Lists stay empty."));
	}
	else
	{
		DataTable->ForeachRow<FAttributeAssetData>(TEXT("AttributeSample"), [this](const FName& RowName, const FAttributeAssetData& Row)
		{
			UAttributeAssetItem* Item = NewObject<UAttributeAssetItem>(this);
			Item->RowName = RowName;
			Item->Data = Row;
			if (Item->Data.Name.IsEmpty())
			{
				Item->Data.Name = RowName.ToString();
			}

			Catalog.Add(Item);
			// The left list is pre-populated with the DataTable entries
			AttributeList.Add(Item);
		});
	}

	LoadUserAssetsFromLibrary();

	OnCatalogChanged.Broadcast();
	OnAttributeListChanged.Broadcast();
}

void UAttributeSampleSubsystem::LoadUserAssetsFromLibrary()
{
	UUserAssetLibrary* Library = GetUserAssetLibrary();
	if (!Library)
	{
		return;
	}

	Library->OnAssetRestored.Remove(AssetRestoredHandle);
	AssetRestoredHandle = Library->OnAssetRestored.AddUObject(this, &UAttributeSampleSubsystem::HandleUserAssetRestored);

	int32 Loaded = 0;
	for (const FUserAssetEntry& Entry : Library->GetEntries())
	{
		// Entries whose local file is missing are restored by the sync and arrive through OnAssetRestored
		if (Entry.State != EUserAssetSyncState::PendingDelete && IFileManager::Get().FileExists(*Library->GetLocalPath(Entry)))
		{
			Loaded += AddLibraryEntryToCatalog(Entry) ? 1 : 0;
		}
	}
	UE_LOG(LogAttributeSample, Log, TEXT("Loaded %d saved user asset(s) into the catalog."), Loaded);

	// Hash-check every saved asset against the server (uploads, repairs, pending deletes)
	Library->SyncAll();
}

UAttributeAssetItem* UAttributeSampleSubsystem::AddLibraryEntryToCatalog(const FUserAssetEntry& Entry)
{
	UUserAssetLibrary* Library = GetUserAssetLibrary();
	if (!Library)
	{
		return nullptr;
	}

	FString Error;
	UAttributeAssetItem* Item = ImportUserAsset(Library->GetLocalPath(Entry), Entry.Layer, Error);
	if (!Item)
	{
		UE_LOG(LogAttributeSample, Warning, TEXT("Saved user asset '%s' could not be loaded: %s"), *Entry.Name, *Error);
		return nullptr;
	}
	Item->Data.Name = Entry.Name;
	Item->UserAssetId = Entry.LocalId;
	return Item;
}

void UAttributeSampleSubsystem::HandleUserAssetRestored(const FUserAssetEntry& Entry)
{
	// Replace an item that showed the old (damaged) file, or add one whose file was missing
	bool bWasInAttributeList = false;
	for (int32 i = Catalog.Num() - 1; i >= 0; --i)
	{
		if (Catalog[i] && Catalog[i]->UserAssetId == Entry.LocalId)
		{
			bWasInAttributeList |= AttributeList.Remove(Catalog[i]) > 0;
			Catalog.RemoveAt(i);
		}
	}
	UAttributeAssetItem* Item = AddLibraryEntryToCatalog(Entry);
	if (Item && bWasInAttributeList)
	{
		AttributeList.Add(Item);
	}
	OnCatalogChanged.Broadcast();
	OnAttributeListChanged.Broadcast();
}

TArray<UAttributeAssetItem*> UAttributeSampleSubsystem::GetCatalogItems(EAttributeAssetType Type) const
{
	TArray<UAttributeAssetItem*> Result;
	for (UAttributeAssetItem* Item : Catalog)
	{
		if (Item && Item->Data.AssetType == Type)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

TArray<UAttributeAssetItem*> UAttributeSampleSubsystem::GetAttributeListItems(EAttributeAssetType Type) const
{
	TArray<UAttributeAssetItem*> Result;
	for (UAttributeAssetItem* Item : AttributeList)
	{
		if (Item && Item->Data.AssetType == Type)
		{
			Result.Add(Item);
		}
	}
	return Result;
}

bool UAttributeSampleSubsystem::AddAttribute(UAttributeAssetItem* Item)
{
	if (!Item || AttributeList.Contains(Item))
	{
		return false;
	}

	AttributeList.Add(Item);
	OnAttributeListChanged.Broadcast();
	return true;
}

FVector UAttributeSampleSubsystem::FindSpawnLocation() const
{
	const FVector2D Random(
		FMath::FRandRange(-SpawnHalfExtent.X, SpawnHalfExtent.X),
		FMath::FRandRange(-SpawnHalfExtent.Y, SpawnHalfExtent.Y));

	FVector Location(Random.X, Random.Y, 0.f);

	// Snap to the floor (static geometry only, so attributes do not stack on each other)
	FHitResult Hit;
	const FVector Start(Random.X, Random.Y, 10000.f);
	const FVector End(Random.X, Random.Y, -10000.f);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		Location.Z = Hit.ImpactPoint.Z;
	}

	return Location;
}

ACoreAttribute* UAttributeSampleSubsystem::SpawnAttribute(UAttributeAssetItem* Item)
{
	if (!Item)
	{
		return nullptr;
	}

	const TSubclassOf<ACoreAttribute> Class = Item->Data.AssetType == EAttributeAssetType::Load ? LoadAttributeClass : BuildingAttributeClass;
	if (!Class)
	{
		return nullptr;
	}

	const FTransform SpawnTransform(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), FindSpawnLocation());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ACoreAttribute* Attribute = GetWorld()->SpawnActor<ACoreAttribute>(Class, SpawnTransform, Params);
	if (!Attribute)
	{
		return nullptr;
	}

	Attribute->SourceAssetName = Item->Data.Name;
	Attribute->ApplyRuntimeSetMeshAsset(Item->ResolveMesh());
	SpawnedAttributes.Add(Attribute);

	UE_LOG(LogAttributeSample, Log, TEXT("Spawned %s '%s' at %s"), *Class->GetName(), *Item->Data.Name, *SpawnTransform.GetLocation().ToString());
	return Attribute;
}

UAttributeAssetItem* UAttributeSampleSubsystem::ImportUserAsset(const FString& FilePath, EAttributeAssetType Type, FString& OutError)
{
	UStaticMesh* Mesh = UAttributeRuntimeMeshLoader::LoadStaticMeshFromFile(this, FilePath, OutError);
	if (!Mesh)
	{
		UE_LOG(LogAttributeSample, Warning, TEXT("Import failed '%s': %s"), *FilePath, *OutError);
		return nullptr;
	}

	UAttributeAssetItem* Item = NewObject<UAttributeAssetItem>(this);
	Item->RowName = *FString::Printf(TEXT("User_%d"), ++UserImportCounter);
	Item->Data.Name = FPaths::GetBaseFilename(FilePath);
	Item->Data.AssetType = Type;
	Item->RuntimeMesh = Mesh;
	Item->SourceFilePath = FilePath;

	Catalog.Add(Item);
	OnCatalogChanged.Broadcast();
	return Item;
}

UAttributeAssetItem* UAttributeSampleSubsystem::ImportAndRegisterUserAsset(const FString& FilePath, EAttributeAssetType Type, FString& OutError)
{
	// Validate by loading first: only files that actually import are saved and uploaded
	UAttributeAssetItem* Item = ImportUserAsset(FilePath, Type, OutError);
	if (!Item)
	{
		return nullptr;
	}

	UUserAssetLibrary* Library = GetUserAssetLibrary();
	FUserAssetEntry Entry;
	FString LibraryError;
	if (Library && Library->AddFile(FilePath, Type, Entry, LibraryError))
	{
		Item->SourceFilePath = Library->GetLocalPath(Entry);
		Item->UserAssetId = Entry.LocalId;
	}
	else
	{
		UE_LOG(LogAttributeSample, Warning, TEXT("'%s' imported but not saved to the user library: %s"), *FilePath, *LibraryError);
	}
	return Item;
}

bool UAttributeSampleSubsystem::DeleteUserAsset(UAttributeAssetItem* Item, FString& OutError)
{
	if (!Item || !Item->IsUserImported())
	{
		OutError = TEXT("Only imported user assets can be deleted.");
		return false;
	}

	Catalog.Remove(Item);
	AttributeList.Remove(Item);
	if (!Item->UserAssetId.IsEmpty())
	{
		if (UUserAssetLibrary* Library = GetUserAssetLibrary())
		{
			Library->RemoveAsset(Item->UserAssetId); // local file + manifest now, server DELETE in the sync
		}
	}
	UE_LOG(LogAttributeSample, Log, TEXT("Deleted user asset '%s'"), *Item->Data.Name);

	OnCatalogChanged.Broadcast();
	OnAttributeListChanged.Broadcast();
	return true;
}

UTexture* UAttributeSampleSubsystem::GetOrCreateThumbnail(UAttributeAssetItem* Item)
{
	if (!Item)
	{
		return nullptr;
	}

	if (Item->Thumbnail)
	{
		return Item->Thumbnail;
	}

	Item->Thumbnail = Item->LoadThumbnailFromPath();
	if (Item->Thumbnail)
	{
		return Item->Thumbnail;
	}

	UStaticMesh* Mesh = Item->ResolveMesh();
	if (!Mesh)
	{
		return nullptr;
	}

	if (!ThumbnailStudio)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ThumbnailStudio = GetWorld()->SpawnActor<AAttributeThumbnailStudio>(FVector(0.f, 0.f, -50000.f), FRotator::ZeroRotator, Params);
	}

	if (ThumbnailStudio)
	{
		Item->Thumbnail = ThumbnailStudio->RenderThumbnail(Mesh);
	}
	return Item->Thumbnail;
}

void UAttributeSampleSubsystem::ClearSpawnedAttributes()
{
	for (ACoreAttribute* Attribute : SpawnedAttributes)
	{
		if (IsValid(Attribute))
		{
			Attribute->Destroy();
		}
	}
	SpawnedAttributes.Reset();
}

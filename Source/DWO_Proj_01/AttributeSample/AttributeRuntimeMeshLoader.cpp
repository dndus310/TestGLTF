// Attribute Sample - user file import (file dialog + glTFRuntime / glTFRuntimeFBX)

#include "AttributeRuntimeMeshLoader.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Widgets/SWindow.h"

#include "glTFRuntimeAsset.h"
#include "glTFRuntimeFunctionLibrary.h"
#include "glTFRuntimeFBXFunctionLibrary.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <shobjidl.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAttributeImport, Log, All);

namespace AttributeImport
{
	/** World transform of an FBX node = own local transform composed with every parent. */
	static FTransform GetFBXParentChainTransform(UglTFRuntimeAsset* Asset, const FglTFRuntimeFBXNode& Node)
	{
		FTransform Result = FTransform::Identity;
		FglTFRuntimeFBXNode Current = Node;
		FglTFRuntimeFBXNode Parent;
		int32 Guard = 0;
		while (UglTFRuntimeFBXFunctionLibrary::GetFBXNodeParent(Asset, Current, Parent) && Guard++ < 256)
		{
			Result = Result * Parent.Transform;
			Current = Parent;
		}
		return Result;
	}

	static void TransformPrimitive(FglTFRuntimePrimitive& Primitive, const FTransform& Transform)
	{
		for (FVector& Position : Primitive.Positions)
		{
			Position = Transform.TransformPosition(Position);
		}
		for (FVector& Normal : Primitive.Normals)
		{
			Normal = Transform.TransformVectorNoScale(Normal).GetSafeNormal();
		}
		for (FVector4& Tangent : Primitive.Tangents)
		{
			const FVector Dir = Transform.TransformVectorNoScale(FVector(Tangent.X, Tangent.Y, Tangent.Z)).GetSafeNormal();
			Tangent = FVector4(Dir.X, Dir.Y, Dir.Z, Tangent.W);
		}
	}

	/** glTFRuntimeFBX exposes FBX per node; merge every mesh node (with its hierarchy transform) into one LOD. */
	static UStaticMesh* LoadFBXAsSingleStaticMesh(UglTFRuntimeAsset* Asset, const FglTFRuntimeStaticMeshConfig& Config, FString& OutError)
	{
		FglTFRuntimeMeshLOD Combined;

		for (const FglTFRuntimeFBXNode& Node : UglTFRuntimeFBXFunctionLibrary::GetFBXNodesMeshes(Asset))
		{
			FglTFRuntimeMeshLOD NodeLOD;
			bool bIsSkeletal = false;
			if (!UglTFRuntimeFBXFunctionLibrary::LoadFBXAsRuntimeLODByNode(Asset, Node, NodeLOD, bIsSkeletal, Config.MaterialsConfig, FglTFRuntimeMaterialsConfig()))
			{
				continue;
			}

			// Static nodes come in geometry space, skinned nodes already include their local transform
			const FTransform ParentChain = GetFBXParentChainTransform(Asset, Node);
			const FTransform NodeToRoot = bIsSkeletal ? ParentChain : Node.Transform * ParentChain;

			for (FglTFRuntimePrimitive& Primitive : NodeLOD.Primitives)
			{
				TransformPrimitive(Primitive, NodeToRoot);
				Primitive.Joints.Empty();
				Primitive.Weights.Empty();
				Combined.Primitives.Add(MoveTemp(Primitive));
			}

			Combined.bHasNormals |= NodeLOD.bHasNormals;
			Combined.bHasTangents |= NodeLOD.bHasTangents;
			Combined.bHasUV |= NodeLOD.bHasUV;
			Combined.bHasVertexColors |= NodeLOD.bHasVertexColors;
		}

		if (Combined.Primitives.Num() == 0)
		{
			OutError = TEXT("FBX has no mesh nodes.");
			return nullptr;
		}

		return Asset->LoadStaticMeshFromRuntimeLODs({ Combined }, Config);
	}
}

bool UAttributeRuntimeMeshLoader::IsSupportedMeshFile(const FString& FilePath)
{
	const FString Extension = FPaths::GetExtension(FilePath).ToLower();
	return Extension == TEXT("gltf") || Extension == TEXT("glb") || Extension == TEXT("fbx");
}

bool UAttributeRuntimeMeshLoader::OpenMeshFileDialog(FString& OutFilePath)
{
#if PLATFORM_WINDOWS
	const HRESULT InitResult = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	const bool bShouldUninitialize = SUCCEEDED(InitResult);

	bool bPicked = false;
	IFileOpenDialog* Dialog = nullptr;
	if (SUCCEEDED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&Dialog))))
	{
		const COMDLG_FILTERSPEC Filters[] =
		{
			{ L"3D Mesh (*.gltf;*.glb;*.fbx)", L"*.gltf;*.glb;*.fbx" },
			{ L"glTF (*.gltf;*.glb)", L"*.gltf;*.glb" },
			{ L"FBX (*.fbx)", L"*.fbx" },
		};
		Dialog->SetFileTypes(UE_ARRAY_COUNT(Filters), Filters);
		Dialog->SetTitle(L"Select a mesh file");

		DWORD Options = 0;
		Dialog->GetOptions(&Options);
		Dialog->SetOptions(Options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);

		HWND ParentWindow = nullptr;
		if (GEngine && GEngine->GameViewport)
		{
			TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
			if (Window.IsValid() && Window->GetNativeWindow().IsValid())
			{
				ParentWindow = static_cast<HWND>(Window->GetNativeWindow()->GetOSWindowHandle());
			}
		}

		if (SUCCEEDED(Dialog->Show(ParentWindow)))
		{
			IShellItem* Item = nullptr;
			if (SUCCEEDED(Dialog->GetResult(&Item)))
			{
				PWSTR Path = nullptr;
				if (SUCCEEDED(Item->GetDisplayName(SIGDN_FILESYSPATH, &Path)))
				{
					OutFilePath = FString(Path);
					::CoTaskMemFree(Path);
					bPicked = true;
				}
				Item->Release();
			}
		}
		Dialog->Release();
	}

	if (bShouldUninitialize)
	{
		::CoUninitialize();
	}
	return bPicked;
#else
	UE_LOG(LogAttributeImport, Warning, TEXT("OpenMeshFileDialog is only implemented on Windows."));
	return false;
#endif
}

UStaticMesh* UAttributeRuntimeMeshLoader::LoadStaticMeshFromFile(UObject* Outer, const FString& FilePath, FString& OutError)
{
	OutError.Reset();

	if (!IsSupportedMeshFile(FilePath))
	{
		OutError = TEXT("Unsupported file type. Use .gltf, .glb or .fbx.");
		return nullptr;
	}

	const int64 FileSize = IFileManager::Get().FileSize(*FilePath);
	if (FileSize <= 0)
	{
		OutError = TEXT("File not found or empty.");
		return nullptr;
	}
	if (FileSize > MaxFileSizeBytes)
	{
		OutError = FString::Printf(TEXT("File is too large (%lld MB, limit %lld MB)."), FileSize / (1024 * 1024), MaxFileSizeBytes / (1024 * 1024));
		return nullptr;
	}

	const bool bIsFBX = FPaths::GetExtension(FilePath).ToLower() == TEXT("fbx");

	FglTFRuntimeConfig LoaderConfig;
	// glTFRuntimeFBX parses the raw file itself; glTF parsing has to be disabled
	LoaderConfig.bAsBlob = bIsFBX;

	UglTFRuntimeAsset* Asset = UglTFRuntimeFunctionLibrary::glTFLoadAssetFromFilename(FilePath, false, LoaderConfig);
	if (!Asset)
	{
		OutError = TEXT("glTFRuntime could not read the file.");
		return nullptr;
	}

	FglTFRuntimeStaticMeshConfig MeshConfig;
	MeshConfig.Outer = Outer;
	MeshConfig.bBuildSimpleCollision = true;

	UStaticMesh* Mesh = bIsFBX
		? AttributeImport::LoadFBXAsSingleStaticMesh(Asset, MeshConfig, OutError)
		: Asset->LoadStaticMeshRecursive(FString(), {}, MeshConfig);

	if (!Mesh)
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("No static mesh could be built from the file.");
		}
		return nullptr;
	}

	const int32 NumTriangles = Mesh->GetNumTriangles(0);
	if (NumTriangles <= 0)
	{
		OutError = TEXT("The file contains no geometry.");
		return nullptr;
	}
	if (NumTriangles > MaxTriangles)
	{
		OutError = FString::Printf(TEXT("Mesh has too many triangles (%d, limit %d)."), NumTriangles, MaxTriangles);
		return nullptr;
	}

	UE_LOG(LogAttributeImport, Log, TEXT("Imported '%s' (%d triangles)."), *FilePath, NumTriangles);
	return Mesh;
}

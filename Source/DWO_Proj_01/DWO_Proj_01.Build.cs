// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DWO_Proj_01 : ModuleRules
{
	public DWO_Proj_01(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		// Attribute Sample: runtime mesh import (glTF / GLB / FBX)
		PrivateDependencyModuleNames.AddRange(new string[] {
			"SlateCore",
			"glTFRuntime",
			"glTFRuntimeFBX"
		});

		// Ship the open source notices next to the executable (not inside the pak)
		RuntimeDependencies.Add("$(ProjectDir)/Licenses/ThirdPartyNotices.txt", StagedFileType.NonUFS);

		PublicIncludePaths.AddRange(new string[] {
			"DWO_Proj_01",
			"DWO_Proj_01/Variant_Platforming",
			"DWO_Proj_01/Variant_Platforming/Animation",
			"DWO_Proj_01/Variant_Combat",
			"DWO_Proj_01/Variant_Combat/AI",
			"DWO_Proj_01/Variant_Combat/Animation",
			"DWO_Proj_01/Variant_Combat/Gameplay",
			"DWO_Proj_01/Variant_Combat/Interfaces",
			"DWO_Proj_01/Variant_Combat/UI",
			"DWO_Proj_01/Variant_SideScrolling",
			"DWO_Proj_01/Variant_SideScrolling/AI",
			"DWO_Proj_01/Variant_SideScrolling/Gameplay",
			"DWO_Proj_01/Variant_SideScrolling/Interfaces",
			"DWO_Proj_01/Variant_SideScrolling/UI",
			"DWO_Proj_01/AttributeSample",
			"DWO_Proj_01/AttributeSample/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

// Editor-only tools for the Attribute Sample (never packaged)

using UnrealBuildTool;

public class DWO_Proj_01Editor : ModuleRules
{
	public DWO_Proj_01Editor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"DWO_Proj_01",
			"UMG",
			"UMGEditor",
			"UnrealEd",
			"Kismet",
			"ToolMenus",
			"Slate",
			"SlateCore"
		});
	}
}

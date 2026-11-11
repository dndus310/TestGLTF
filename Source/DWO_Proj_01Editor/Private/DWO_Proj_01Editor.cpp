// Editor-only tools for the Attribute Sample

#include "AttributeWidgetDesignerExporter.h"

#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "DWO_Proj_01Editor"

class FDWO_Proj_01EditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FDWO_Proj_01EditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped OwnerScoped(this);

		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		FToolMenuSection& Section = Menu->AddSection("AttributeSample", LOCTEXT("AttributeSampleSection", "Attribute Sample"));

		Section.AddMenuEntry(
			"ExportAttributeWidgets",
			LOCTEXT("ExportWidgets", "Export Widget Layouts to Designer"),
			LOCTEXT("ExportWidgetsTip", "Writes the C++ default layout into each Attribute Sample WBP whose designer tree is empty."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&FDWO_Proj_01EditorModule::RunExport, false)));

		Section.AddMenuEntry(
			"ExportAttributeWidgetsOverwrite",
			LOCTEXT("ExportWidgetsOverwrite", "Export Widget Layouts to Designer (Overwrite)"),
			LOCTEXT("ExportWidgetsOverwriteTip", "Replaces the designer tree of every Attribute Sample WBP with the C++ default layout."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateStatic(&FDWO_Proj_01EditorModule::RunExport, true)));
	}

	static void RunExport(bool bOverwrite)
	{
		if (bOverwrite && FMessageDialog::Open(EAppMsgType::YesNo,
			LOCTEXT("ConfirmOverwrite", "Replace the designer layout of all Attribute Sample widget blueprints?\nChanges made in the Designer tab will be lost.")) != EAppReturnType::Yes)
		{
			return;
		}

		TArray<FString> Log;
		FAttributeWidgetDesignerExporter::ExportAll(bOverwrite, Log);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Join(Log, TEXT("\n"))));
	}
};

IMPLEMENT_MODULE(FDWO_Proj_01EditorModule, DWO_Proj_01Editor)

#undef LOCTEXT_NAMESPACE

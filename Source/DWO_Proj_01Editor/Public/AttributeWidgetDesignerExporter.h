// Attribute Sample - writes the C++ default widget layouts into the Widget Blueprint designer trees

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AttributeWidgetDesignerExporter.generated.h"

class UUserWidget;
class UWidgetBlueprint;

/**
 * Runs each widget's BuildDefaultLayout() against its Widget Blueprint's designer WidgetTree.
 * Writing (clear / variables / compile / save) is done by a small helper in the .cpp.
 * Afterwards the layout is edited in the Designer tab; the C++ layout stays only as a fallback
 * for an empty tree.
 */
class DWO_PROJ_01EDITOR_API FAttributeWidgetDesignerExporter
{
public:
	/**
	 * Exports all Attribute Sample WBPs (children first, so nested WBPs already have their tree).
	 * @param bOverwrite  Replace a designer tree that already has content.
	 * @param OutLog      One line per blueprint.
	 * @return            Number of blueprints written.
	 */
	static int32 ExportAll(bool bOverwrite, TArray<FString>& OutLog);

	/** Exports one Widget Blueprint whose native parent is one of the Attribute Sample widgets. */
	static bool ExportBlueprint(UWidgetBlueprint* Blueprint, bool bOverwrite, FString& OutMessage);

private:
	static bool HasDesignerContent(const UWidgetBlueprint* Blueprint);
	static void ClearDesignerTree(UWidgetBlueprint* Blueprint);
	static bool BuildIntoTree(UUserWidget* Builder);
};

/** Python / Editor Utility access: unreal.AttributeWidgetDesignerLibrary.export_attribute_widgets_to_designer(False) */
UCLASS()
class DWO_PROJ_01EDITOR_API UAttributeWidgetDesignerLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Attribute Sample|Editor")
	static int32 ExportAttributeWidgetsToDesigner(bool bOverwrite);
};

/** UnrealEditor-Cmd.exe DWO_Proj_01.uproject -run=AttributeWidgetDesignerExport [-Overwrite] */
UCLASS()
class UAttributeWidgetDesignerExportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UAttributeWidgetDesignerExportCommandlet();

	virtual int32 Main(const FString& Params) override;
};

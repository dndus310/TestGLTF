"""
Attribute Sample - asset generator (Unreal Editor Python).

Creates / refreshes:
  /Game/AttributeSample/Data/DT_AttributeAssetData   (rows pre-filled)
  /Game/AttributeSample/UI/WBP_*                       (children of the C++ widgets)
  /Game/AttributeSample/Maps/Lvl_AttributeSample       (lighting + floor + game mode)

Run from the editor: Tools > Execute Python Script..., or headless:
  UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
Safe to run again: existing assets are reused, the DataTable rows are replaced.
"""

import unreal

ROOT = "/Game/AttributeSample"
DATA_DIR = ROOT + "/Data"
UI_DIR = ROOT + "/UI"
MAP_DIR = ROOT + "/Maps"
MAP_PATH = MAP_DIR + "/Lvl_AttributeSample"

MODULE = "/Script/DWO_Proj_01"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("[AttributeSample] " + msg)


def ensure_dir(path):
    if not eal.does_directory_exist(path):
        eal.make_directory(path)


def load_or_create(name, package_dir, asset_class, factory):
    path = package_dir + "/" + name
    if eal.does_asset_exist(path):
        return eal.load_asset(path), False
    return asset_tools.create_asset(name, package_dir, asset_class, factory), True


# ---------------------------------------------------------------------------
# DataTable
# ---------------------------------------------------------------------------
ROWS = [
    # RowName,            Name,             Type,       Mesh
    ("B_Box",             "Box Building",   "Building", "/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"),
    ("B_Chamfer",         "Chamfer Block",  "Building", "/Game/LevelPrototyping/Meshes/SM_ChamferCube.SM_ChamferCube"),
    ("B_Silo",            "Silo",           "Building", "/Game/LevelPrototyping/Meshes/SM_Cylinder.SM_Cylinder"),
    ("B_QuarterHall",     "Quarter Hall",   "Building", "/Game/LevelPrototyping/Meshes/SM_QuarterCylinder.SM_QuarterCylinder"),
    ("B_Tower",           "Cone Tower",     "Building", "/Engine/BasicShapes/Cone.Cone"),
    ("L_Crate",           "Crate",          "Load",     "/Engine/BasicShapes/Cube.Cube"),
    ("L_Barrel",          "Barrel",         "Load",     "/Engine/BasicShapes/Cylinder.Cylinder"),
    ("L_Ball",            "Ball",           "Load",     "/Engine/BasicShapes/Sphere.Sphere"),
    ("L_Ramp",            "Ramp",           "Load",     "/Game/LevelPrototyping/Meshes/SM_Ramp.SM_Ramp"),
    ("L_Curve",           "Curved Plate",   "Load",     "/Game/LevelPrototyping/Meshes/SM_QuarterCylinderOuter.SM_QuarterCylinderOuter"),
]


def build_data_table():
    ensure_dir(DATA_DIR)
    row_struct = unreal.load_object(None, MODULE + ".AttributeAssetData")
    if row_struct is None:
        raise RuntimeError("Row struct AttributeAssetData not found. Build the C++ module first.")

    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", row_struct)
    table, created = load_or_create("DT_AttributeAssetData", DATA_DIR, unreal.DataTable, factory)

    lines = ["---,Name,AssetType,SoftObjectStaticMeshAsset,ThumbnailPath"]
    for row_name, name, asset_type, mesh in ROWS:
        if not eal.does_asset_exist(mesh.split(".")[0]):
            unreal.log_warning("[AttributeSample] mesh missing, row skipped: " + mesh)
            continue
        lines.append('{0},"{1}",{2},"{3}",""'.format(row_name, name, asset_type, mesh))

    if not unreal.DataTableFunctionLibrary.fill_data_table_from_csv_string(table, "\n".join(lines)):
        raise RuntimeError("Failed to fill DT_AttributeAssetData")

    eal.save_loaded_asset(table)
    log("DataTable {0} ({1} rows)".format("created" if created else "updated", len(lines) - 1))


# ---------------------------------------------------------------------------
# Widget Blueprints (empty designer tree -> the C++ parent builds the layout)
# ---------------------------------------------------------------------------
WIDGETS = [
    ("WBP_AttributeAssetItem", "AttributeAssetItemWidget"),
    ("WBP_AttributeAssetList", "AttributeAssetListWidget"),
    ("WBP_AttributeList",      "AttributeListWidget"),
    ("WBP_LeftPanel",          "AttributeLeftPanelWidget"),
    ("WBP_RightPanel",         "AttributeRightPanelWidget"),
]


def build_widgets():
    ensure_dir(UI_DIR)
    for asset_name, class_name in WIDGETS:
        parent = unreal.load_class(None, MODULE + "." + class_name)
        if parent is None:
            raise RuntimeError("Widget class not found: " + class_name)

        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        try:
            # Keep the designer tree empty so the C++ default layout is used
            factory.set_editor_property("root_widget_class", None)
        except Exception:
            pass

        blueprint, created = load_or_create(asset_name, UI_DIR, unreal.WidgetBlueprint, factory)
        if blueprint is None:
            raise RuntimeError("Could not create " + asset_name)

        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        eal.save_loaded_asset(blueprint)
        log("{0} {1}".format(asset_name, "created" if created else "already exists"))


# ---------------------------------------------------------------------------
# Level: default lighting template + large grid floor + game mode override
# ---------------------------------------------------------------------------
def build_level():
    ensure_dir(MAP_DIR)
    level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

    if eal.does_asset_exist(MAP_PATH):
        level_subsystem.load_level(MAP_PATH)
        log("Level exists, refreshing floor and game mode")
    else:
        if not level_subsystem.new_level_from_template(MAP_PATH, "/Engine/Maps/Templates/Template_Default"):
            raise RuntimeError("Could not create level from Template_Default")
        log("Level created from Template_Default")

    # Replace template meshes with one 60m x 60m grid floor
    for actor in actor_subsystem.get_all_level_actors():
        if isinstance(actor, unreal.StaticMeshActor):
            actor_subsystem.destroy_actor(actor)

    floor = actor_subsystem.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    floor.set_actor_label("Floor")
    floor.set_actor_scale3d(unreal.Vector(60.0, 60.0, 1.0))
    mesh_component = floor.static_mesh_component
    mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Plane.Plane"))
    grid = unreal.load_asset("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray.MI_PrototypeGrid_Gray")
    if grid:
        mesh_component.set_material(0, grid)
    mesh_component.set_collision_profile_name("BlockAll")

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    game_mode = unreal.load_class(None, MODULE + ".AttributeSampleGameMode")
    world.get_world_settings().set_editor_property("default_game_mode", game_mode)

    level_subsystem.save_current_level()
    log("Level saved: " + MAP_PATH)


build_data_table()
build_widgets()
build_level()
log("Done")

"""
UE 5.7 editor commandlet script: FBX (+ optional PNG textures) -> GLB.

Run:
  UnrealEditor-Cmd.exe <Project.uproject> -run=pythonscript -script="fbx2glb_ue.py" -unattended -nosplash -nullrhi
Env vars (input):
  FBX2GLB_FBX     path to .fbx
  FBX2GLB_BASE    optional base color texture (png/jpg)
  FBX2GLB_NORMAL  optional normal map texture
  FBX2GLB_OUT     output .glb path
"""
import os
import unreal

FBX = os.environ["FBX2GLB_FBX"]
BASE = os.environ.get("FBX2GLB_BASE", "")
NORMAL = os.environ.get("FBX2GLB_NORMAL", "")
OUT = os.environ["FBX2GLB_OUT"]
DEST = "/Game/Converted"

log = lambda m: unreal.log("[fbx2glb] " + str(m))

# Commandlet has no Slate: stop Interchange from syncing the Content Browser after import
# (this CVar overrides AssetImportTask settings and crashes without UI).
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.SyncToBrowser 0")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def import_file(path, options=None):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DEST
    task.automated = True
    task.replace_existing = True
    task.save = False
    if options is not None:
        task.options = options
    asset_tools.import_asset_tasks([task])
    paths = list(task.imported_object_paths)
    log("imported %s -> %s" % (os.path.basename(path), paths))
    return [eal.load_asset(p) for p in paths]


# 1) FBX scene import (Interchange) into a blank level: keeps every part and its node transform
world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
log("world: %s" % world)
mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
src = unreal.InterchangeManager.create_source_data(FBX)
params = unreal.ImportAssetParameters()
params.set_editor_property("is_automated", True)
ok_scene = mgr.import_scene(DEST, src, params)
log("import_scene ok=%s" % ok_scene)

ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
world = ues.get_editor_world()
actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
comps = []
for a in actors:
    comps += list(a.get_components_by_class(unreal.StaticMeshComponent))
    comps += list(a.get_components_by_class(unreal.SkeletalMeshComponent))
comps = [c for c in comps if (c.get_editor_property("static_mesh") if isinstance(c, unreal.StaticMeshComponent)
                             else c.get_skeletal_mesh_asset())]
log("actors=%d mesh components=%d" % (len(actors), len(comps)))
if not comps:
    raise RuntimeError("no mesh imported from FBX")
mesh_name = os.path.splitext(os.path.basename(FBX))[0]

# 2) Optional textures -> simple material (texture params wired straight into BaseColor / Normal)
base_tex = import_file(BASE)[0] if BASE else None
normal_tex = import_file(NORMAL)[0] if NORMAL else None
if normal_tex:
    normal_tex.set_editor_property("srgb", False)
    normal_tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)

if base_tex or normal_tex:
    mat = asset_tools.create_asset("M_" + mesh_name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    y = 0
    if base_tex:
        n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -400, y)
        n.set_editor_property("parameter_name", "BaseColorTexture")
        n.set_editor_property("texture", base_tex)
        mel.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        y += 300
    if normal_tex:
        n = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -400, y)
        n.set_editor_property("parameter_name", "NormalTexture")
        n.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        n.set_editor_property("texture", normal_tex)
        mel.connect_material_property(n, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(mat)

    n_slots = 0
    for c in comps:
        for i in range(c.get_num_materials()):
            c.set_material(i, mat)
            n_slots += 1
    log("material %s assigned to %d slot(s) on %d component(s)" % (mat.get_name(), n_slots, len(comps)))

# 3) Export GLB with the built-in GLTFExporter
opts = unreal.GLTFExportOptions()
opts.set_editor_property("texture_image_format", unreal.GLTFTextureImageFormat.PNG)
# Material baking renders on the GPU: run the commandlet WITHOUT -nullrhi and WITH -AllowCommandletRendering.
opts.set_editor_property("bake_material_inputs", unreal.GLTFMaterialBakeMode.SIMPLE)
os.makedirs(os.path.dirname(OUT), exist_ok=True)
if os.path.exists(OUT):
    os.remove(OUT)
# Export only the imported actors (skip the blank map's Brush / PhysicsVolume / NavData)
skip = (unreal.Brush, unreal.PhysicsVolume, unreal.NavigationData)
export_actors = set(a for a in actors if not isinstance(a, skip))
log("exporting %d actor(s)" % len(export_actors))
msgs = unreal.GLTFExporter.export_to_gltf(world, OUT, opts, export_actors)
if msgs is not None:
    for kind in ("suggestions", "warnings", "errors"):
        for m in msgs.get_editor_property(kind) or []:
            log("%s: %s" % (kind, m))
ok = os.path.exists(OUT) and os.path.getsize(OUT) > 0
log("export ok=%s size=%s -> %s" % (ok, os.path.getsize(OUT) if ok else 0, OUT))

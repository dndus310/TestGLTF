"""
Load a GLB with UE 5.7 Interchange (glTF translator) into a blank level, report what came in,
and render capture images (base color + lit) with a SceneCapture2D.
Env: CHECK_GLB (input .glb), CHECK_OUT (output folder for png)
Run with -AllowCommandletRendering (no -nullrhi).
"""
import os
import unreal

GLB = os.environ["CHECK_GLB"]
OUT = os.environ["CHECK_OUT"]
DEST = "/Game/Check"
log = lambda m: unreal.log("[check] " + str(m))
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.SyncToBrowser 0")

unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
params = unreal.ImportAssetParameters()
params.set_editor_property("is_automated", True)
ok = mgr.import_scene(DEST, unreal.InterchangeManager.create_source_data(GLB), params)
log("import_scene ok=%s" % ok)

# ---- what came in
assets = unreal.EditorAssetLibrary.list_assets(DEST, recursive=True)
for p in assets:
    a = unreal.EditorAssetLibrary.load_asset(p)
    extra = ""
    if isinstance(a, unreal.Texture2D):
        extra = "%dx%d srgb=%s" % (a.blueprint_get_size_x(), a.blueprint_get_size_y(), a.get_editor_property("srgb"))
    elif isinstance(a, unreal.StaticMesh):
        extra = "tris(LOD0)=%d" % a.get_num_triangles(0)
    elif isinstance(a, unreal.MaterialInstance):
        tex = [(str(v.parameter_info.name), v.parameter_value.get_name() if v.parameter_value else None)
               for v in a.get_editor_property("texture_parameter_values")]
        extra = "parent=%s textures=%s" % (a.get_editor_property("parent").get_name(), tex)
    log("asset %-60s %-28s %s" % (p, type(a).__name__, extra))

actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
comps = []
for a in actors:
    comps += list(a.get_components_by_class(unreal.StaticMeshComponent))
comps = [c for c in comps if c.get_editor_property("static_mesh")]
log("actors=%d static mesh components=%d" % (len(actors), len(comps)))

lo, hi = None, None
for c in comps:
    o, e, _ = unreal.SystemLibrary.get_component_bounds(c)
    cmin, cmax = o - e, o + e
    lo = cmin if lo is None else unreal.Vector(min(lo.x, cmin.x), min(lo.y, cmin.y), min(lo.z, cmin.z))
    hi = cmax if hi is None else unreal.Vector(max(hi.x, cmax.x), max(hi.y, cmax.y), max(hi.z, cmax.z))
    mat = c.get_material(0)
    log("  comp %-14s mesh=%-12s scale=%s mat=%s" % (c.get_owner().get_actor_label(), c.get_editor_property("static_mesh").get_name(),
                                                   c.get_world_scale(), mat.get_name() if mat else None))
size = hi - lo
center = (lo + hi) * 0.5
log("world bounds (UE cm, Z-up): min=%s max=%s size=%s" % (lo, hi, size))

# ---- render
# Frame the gun body (most triangles) rather than the long bullet/muzzle planes
body = max(comps, key=lambda c: c.get_editor_property("static_mesh").get_num_triangles(0))
log("framing component: %s" % body.get_owner().get_actor_label())
bo, be, br = unreal.SystemLibrary.get_component_bounds(body)
radius = max(be.x, be.y, be.z)

light = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 0), unreal.Rotator(-40, -60, 0))
light.light_component.set_intensity(6.0)
sky = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
sky.light_component.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sky.light_component.set_intensity(1.0)

def capture(name, source, cam_dir):
    rt = unreal.RenderingLibrary.create_render_target2d(world, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8,
                                                        unreal.LinearColor(0.2, 0.2, 0.22, 1))
    cam_pos = bo + cam_dir * (radius * 2.6)
    rot = unreal.MathLibrary.find_look_at_rotation(cam_pos, bo)
    cap = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SceneCapture2D, cam_pos, rot)
    cc = cap.capture_component2d
    cc.set_editor_property("texture_target", rt)
    cc.set_editor_property("capture_source", source)
    cc.set_editor_property("fov_angle", 50.0)
    cc.capture_scene()
    unreal.RenderingLibrary.export_render_target(world, rt, OUT, name)
    log("captured %s" % os.path.join(OUT, name))

os.makedirs(OUT, exist_ok=True)
side = unreal.Vector(0.15, -1.0, 0.35); side = side / side.length()
capture("check_basecolor.png", unreal.SceneCaptureSource.SCS_BASE_COLOR, side)
capture("check_lit.png", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR, side)
front = unreal.Vector(-0.6, -0.8, 0.2); front = front / front.length()
capture("check_lit_2.png", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR, front)

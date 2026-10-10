"""Build the frontier maps: M2_World (seeded procedural world) and MainMenu (live preview).

Run headless through scripts/regenerate_frontier.ps1. Everything here is generated and
safe to re-run: both maps are rebuilt from scratch, materials and imports are refreshed.
"""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve().parents[1]
WORLD_MAP = '/Game/Downstream/Maps/M2_World'
MENU_MAP = '/Game/Downstream/Maps/MainMenu'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
report = {'passed': False}
MEL = unreal.MaterialEditingLibrary

# ---- Imports: frontier audio (synthesized originals) and the water ripple normal map.
def import_file(source, destination, name, factory):
    task = unreal.AssetImportTask()
    task.filename = str(source)
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    task.factory = factory
    tools.import_asset_tasks([task])
    return unreal.load_asset(destination + '/' + name)

# Kenney meshes import with Nanite on, but their glTF material instances lack the Nanite
# usage flag, so meshes spawned at runtime (outposts, props) would render with the default
# material. They are a few hundred triangles each: turn Nanite off instead.
nanite_disabled = 0
for path in unreal.EditorAssetLibrary.list_assets('/Game/Downstream/ThirdParty/Kenney', recursive=True, include_folder=False):
    if unreal.EditorAssetLibrary.find_asset_data(path).asset_class_path.asset_name != 'StaticMesh':
        continue
    mesh = unreal.load_asset(path)
    settings = mesh.get_editor_property('nanite_settings')
    if settings.get_editor_property('enabled'):
        settings.set_editor_property('enabled', False)
        mesh_tools = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
        if hasattr(mesh_tools, 'set_nanite_settings'):
            mesh_tools.set_nanite_settings(mesh, settings, True)
        else:
            mesh.set_editor_property('nanite_settings', settings)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        nanite_disabled += 1
report['kenney_nanite_disabled'] = nanite_disabled

sounds = {}
for source in sorted((ROOT / 'art' / 'audio').glob('S_Frontier*.wav')):
    wave = import_file(source, '/Game/Downstream/Audio', source.stem, unreal.SoundFactory())
    assert wave, 'Audio import failed: ' + source.name
    if source.stem == 'S_FrontierWaterLoop':
        wave.set_editor_property('looping', True)
        unreal.EditorAssetLibrary.save_loaded_asset(wave, False)
    sounds[source.stem] = wave
report['sounds'] = sorted(sounds)

ripple = import_file(ROOT / 'art' / 'textures' / 'T_WaterRipple_N.png', '/Game/Downstream/Textures', 'T_WaterRipple_N', unreal.TextureFactory())
assert ripple, 'Ripple normal import failed'
ripple.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
ripple.set_editor_property('srgb', False)
unreal.EditorAssetLibrary.save_loaded_asset(ripple, False)

tuning = unreal.load_asset('/Game/Downstream/Data/DA_PrototypeTuning')
assert tuning, 'Run scripts/regenerate_expedition.ps1 first (creates DA_PrototypeTuning)'
for prop, name in [('alarm_sound', 'Alarm'), ('collapse_sound', 'Collapse'), ('sandbag_sound', 'Sandbag'),
                   ('breach_sound', 'Breach'), ('wreck_sound', 'Wreck'), ('water_loop_sound', 'WaterLoop')]:
    if 'S_Frontier' + name in sounds:
        tuning.set_editor_property(prop, sounds['S_Frontier' + name])
assert unreal.EditorAssetLibrary.save_loaded_asset(tuning, False)

# ---- Material helpers (same graph style as create_expedition_level.py).
def fresh_material(name):
    path = '/Game/Downstream/Materials/' + name
    mat = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name, '/Game/Downstream/Materials', unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    return mat
def node(mat, cls, x, y, **props):
    expr = MEL.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        expr.set_editor_property(key, value)
    return expr
def const1(mat, v, x, y):
    return node(mat, unreal.MaterialExpressionConstant, x, y, r=v)
def const3(mat, rgb, x, y):
    return node(mat, unreal.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(*rgb, 1))
def op(mat, cls, a, b, x, y, a_out='', b_out=''):
    expr = node(mat, cls, x, y)
    MEL.connect_material_expressions(a, a_out, expr, 'A')
    MEL.connect_material_expressions(b, b_out, expr, 'B')
    return expr
def lerp(mat, a, b, alpha, x, y):
    expr = node(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    MEL.connect_material_expressions(a, '', expr, 'A')
    MEL.connect_material_expressions(b, '', expr, 'B')
    MEL.connect_material_expressions(alpha, '', expr, 'Alpha')
    return expr
def sat(mat, a, x, y):
    expr = node(mat, unreal.MaterialExpressionSaturate, x, y)
    MEL.connect_material_expressions(a, '', expr, '')
    return expr
def mask(mat, src, channel, x, y):
    expr = node(mat, unreal.MaterialExpressionComponentMask, x, y, r=channel == 'r', g=channel == 'g', b=False, a=False)
    MEL.connect_material_expressions(src, '', expr, '')
    return expr
ACG = '/Game/Downstream/ThirdParty/AmbientCG/'
def tex(mat, asset, uv, x, y, kind='color'):
    t = unreal.load_asset(asset if asset.startswith('/') else ACG + asset)
    assert t, 'Missing texture ' + asset + '; run scripts/import_third_party.ps1'
    sampler = {'color': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 'normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL}[kind]
    expr = node(mat, unreal.MaterialExpressionTextureSample, x, y, texture=t, sampler_type=sampler)
    MEL.connect_material_expressions(uv, '', expr, 'UVs')
    return expr
def world_uv(mat, tile_cm, x, y):
    wp = node(mat, unreal.MaterialExpressionWorldPosition, x - 300, y)
    return op(mat, unreal.MaterialExpressionDivide, mask_rg(mat, wp, x - 150, y), const1(mat, tile_cm, x - 150, y + 80), x, y)
def mask_rg(mat, src, x, y):
    expr = node(mat, unreal.MaterialExpressionComponentMask, x, y, r=True, g=True, b=False, a=False)
    MEL.connect_material_expressions(src, '', expr, '')
    return expr

# World ground: UV0.x = metres dug(-)/piled(+), UV0.y = height (m, about 0-8 on the frontier).
# Low ground and fresh cuts are dark wet dirt, the plains are sand, ridges and levee rims rocky.
g = fresh_material('M_WorldGround')
uv = node(g, unreal.MaterialExpressionTextureCoordinate, -1500, 0)
delta = mask(g, uv, 'r', -1300, 0)
height = mask(g, uv, 'g', -1300, 150)
wuv = world_uv(g, 520, -1500, 700)
sand = op(g, unreal.MaterialExpressionMultiply, tex(g, 'T_Ground054_Color', wuv, -1100, 600), const3(g, (.9, .84, .74), -1100, 760), -900, 620)
dirt = tex(g, 'T_Ground103_Color', wuv, -1100, 850)
rocky = op(g, unreal.MaterialExpressionMultiply, tex(g, 'T_Ground109_Color', wuv, -1100, 1100), const3(g, (.85, .82, .78), -1100, 1250), -900, 1120)
low = sat(g, op(g, unreal.MaterialExpressionMultiply, op(g, unreal.MaterialExpressionSubtract, height, const1(g, .6, -1300, 260), -1150, 220), const1(g, .45, -1300, 320), -1050, 220), -950, 220)
high = sat(g, op(g, unreal.MaterialExpressionMultiply, op(g, unreal.MaterialExpressionSubtract, height, const1(g, 4.6, -1300, 400), -1150, 380), const1(g, .7, -1300, 460), -1050, 380), -950, 380)
base = lerp(g, lerp(g, dirt, sand, low, -750, -200), rocky, high, -650, -150)
dug = sat(g, op(g, unreal.MaterialExpressionMultiply, delta, const1(g, -4, -1300, 60), -1150, 0), -1000, 0)
dark_dirt = op(g, unreal.MaterialExpressionMultiply, dirt, const1(g, .62, -900, 900), -800, 880)
dug_col = lerp(g, base, dark_dirt, op(g, unreal.MaterialExpressionMultiply, dug, const1(g, .9, -1000, 60), -850, 20), -550, -100)
berm = sat(g, op(g, unreal.MaterialExpressionMultiply, delta, const1(g, 4, -1300, 110), -1150, 60), -1000, 60)
berm_col = lerp(g, dug_col, rocky, op(g, unreal.MaterialExpressionMultiply, berm, const1(g, .8, -1000, 120), -850, 90), -400, -50)
frac = node(g, unreal.MaterialExpressionFrac, -900, 300)
MEL.connect_material_expressions(op(g, unreal.MaterialExpressionMultiply, height, const1(g, 1, -1300, 520), -1150, 520), '', frac, '')
band = sat(g, op(g, unreal.MaterialExpressionMultiply, op(g, unreal.MaterialExpressionSubtract, const1(g, .06, -900, 400), frac, -800, 350), const1(g, 16, -800, 450), -700, 380), -600, 380)
shade = op(g, unreal.MaterialExpressionSubtract, const1(g, 1, -600, 300), op(g, unreal.MaterialExpressionMultiply, band, const1(g, .14, -600, 450), -500, 400), -400, 350)
MEL.connect_material_property(op(g, unreal.MaterialExpressionMultiply, berm_col, shade, -250, 0), '', unreal.MaterialProperty.MP_BASE_COLOR)
normal = lerp(g, tex(g, 'T_Ground103_NormalDX', wuv, -1100, 1400, 'normal'), tex(g, 'T_Ground054_NormalDX', wuv, -1100, 1650, 'normal'), low, -500, 1400)
MEL.connect_material_property(normal, '', unreal.MaterialProperty.MP_NORMAL)
MEL.connect_material_property(lerp(g, const1(g, .95, -500, 200), const1(g, .7, -500, 260), dug, -300, 220), '', unreal.MaterialProperty.MP_ROUGHNESS)
MEL.recompile_material(g)
assert unreal.EditorAssetLibrary.save_loaded_asset(g, False)

# Flowing water: UV0.x = depth (m), UV0.y = speed (m/s). Two ripple normals pan across each
# other; faster water gets stronger ripples and more foam.
w = fresh_material('M_WaterFlow')
w.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
w.set_editor_property('two_sided', True)
w.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE)
uv = node(w, unreal.MaterialExpressionTextureCoordinate, -1500, 0)
depth = mask(w, uv, 'r', -1300, 0)
speed = mask(w, uv, 'g', -1300, 150)
d01 = sat(w, op(w, unreal.MaterialExpressionMultiply, depth, const1(w, 1 / 1.4, -1300, 80), -1150, 0), -1000, 0)
foam = sat(w, op(w, unreal.MaterialExpressionMultiply, speed, const1(w, .55, -1300, 220), -1150, 150), -1000, 150)
col = lerp(w, const3(w, (.16, .5, .56), -1000, -250), const3(w, (.025, .13, .29), -1000, -150), d01, -800, -200)
col = lerp(w, col, const3(w, (.82, .9, .93), -800, -50), op(w, unreal.MaterialExpressionMultiply, foam, const1(w, .45, -1000, 220), -900, 180), -600, -100)
MEL.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(op(w, unreal.MaterialExpressionMultiply, col, const1(w, .3, -600, 80), -400, 60), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
ruv = world_uv(w, 380, -1500, 700)
pan_a = node(w, unreal.MaterialExpressionPanner, -1150, 650, speed_x=0.035, speed_y=0.02)
MEL.connect_material_expressions(ruv, '', pan_a, 'Coordinate')
ruv2 = op(w, unreal.MaterialExpressionMultiply, ruv, const1(w, 1.7, -1300, 900), -1200, 850)
pan_b = node(w, unreal.MaterialExpressionPanner, -1150, 900, speed_x=-0.025, speed_y=0.04)
MEL.connect_material_expressions(ruv2, '', pan_b, 'Coordinate')
ripples = op(w, unreal.MaterialExpressionAdd, tex(w, '/Game/Downstream/Textures/T_WaterRipple_N', pan_a, -950, 650, 'normal'),
             tex(w, '/Game/Downstream/Textures/T_WaterRipple_N', pan_b, -950, 900, 'normal'), -750, 750)
flat = const3(w, (0, 0, 1), -750, 950)
strength = op(w, unreal.MaterialExpressionAdd, const1(w, .25, -900, 1050), op(w, unreal.MaterialExpressionMultiply, foam, const1(w, .75, -900, 1120), -800, 1080), -700, 1050)
norm = node(w, unreal.MaterialExpressionNormalize, -450, 800)
MEL.connect_material_expressions(lerp(w, flat, ripples, strength, -600, 800), '', norm, '')
MEL.connect_material_property(norm, '', unreal.MaterialProperty.MP_NORMAL)
wet = sat(w, op(w, unreal.MaterialExpressionMultiply, depth, const1(w, 60, -1300, 330), -1150, 300), -1000, 300)
alpha = op(w, unreal.MaterialExpressionAdd, const1(w, .52, -1000, 400), op(w, unreal.MaterialExpressionMultiply, d01, const1(w, .38, -1000, 480), -900, 450), -800, 420)
MEL.connect_material_property(op(w, unreal.MaterialExpressionMultiply, wet, alpha, -600, 350), '', unreal.MaterialProperty.MP_OPACITY)
MEL.connect_material_property(const1(w, .04, -300, 250), '', unreal.MaterialProperty.MP_ROUGHNESS)
MEL.connect_material_property(const1(w, .9, -300, 300), '', unreal.MaterialProperty.MP_SPECULAR)
MEL.recompile_material(w)
assert unreal.EditorAssetLibrary.save_loaded_asset(w, False)

open_ground = unreal.load_asset('/Game/Downstream/Materials/M_OpenGround')
marker_valid = unreal.load_asset('/Game/Downstream/Materials/M_PlacementValid')
marker_invalid = unreal.load_asset('/Game/Downstream/Materials/M_PlacementInvalid')

# ---- Shared map dressing.
def lighting(sun_pitch=-46, sun_yaw=-35, fog=0.012):
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000), unreal.Rotator(pitch=sun_pitch, yaw=sun_yaw, roll=0))
    sun.set_actor_label('Sun')
    sun.light_component.set_editor_property('intensity', 5.0)
    sun.light_component.set_editor_property('light_color', unreal.Color(255, 222, 176, 255))
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_editor_property('atmosphere_sun_light', True)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1500))
    sky.set_actor_label('SkyLight')
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property('intensity', 0.85)
    sky.light_component.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    sky.light_component.set_editor_property('cubemap', unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, -1000)).set_actor_label('SkyAtmosphere')
    height_fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, -200))
    height_fog.set_actor_label('HeightFog')
    fog_comp = height_fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    fog_comp.set_editor_property('fog_density', fog)
    fog_comp.set_editor_property('fog_inscattering_luminance', unreal.LinearColor(.55, .62, .66, 1))

def ground_plane(z=-25.0, size_m=420):
    plane = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, z))
    plane.set_actor_label('Ground_Outer')
    plane.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
    plane.set_actor_scale3d(unreal.Vector(size_m, size_m, 1))
    plane.static_mesh_component.set_collision_profile_name('BlockAll')
    if open_ground:
        plane.static_mesh_component.set_material(0, open_ground)
    return plane

def water_site(kind, label):
    site = actors.spawn_actor_from_class(unreal.DownstreamWaterSite, unreal.Vector(0, 0, 0))
    site.set_actor_label(label)
    site.set_editor_property('site_kind', kind)
    site.set_editor_property('terrain_material', g)
    site.set_editor_property('water_material', w)
    if marker_valid:
        site.set_editor_property('valid_marker_material', marker_valid)
        site.set_editor_property('queue_marker_material', marker_valid)
    if marker_invalid:
        site.set_editor_property('invalid_marker_material', marker_invalid)
    return site

def new_map(path):
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        assert level.load_level(path), 'Could not open ' + path
        for actor in actors.get_all_level_actors():
            if not isinstance(actor, unreal.WorldSettings):
                actors.destroy_actor(actor)
    else:
        assert level.new_level(path), 'Could not create ' + path
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

# ---- M2_World: the procedural frontier. The terrain, outposts, vault, caches, patrols and
# props all come from the seed at runtime (DownstreamWorldDirector); this map only holds the
# lighting, the presentation actor, the director and navigation bounds.
world = new_map(WORLD_MAP)
world.get_world_settings().set_editor_property('default_game_mode', unreal.DownstreamGameMode)
world.get_world_settings().set_editor_property('kill_z', -3000.0)
lighting()
ground_plane()
world_site = water_site(unreal.DownstreamSiteKind.WORLD, 'Frontier_WaterSite')
director = actors.spawn_actor_from_class(unreal.DownstreamWorldDirector, unreal.Vector(0, 0, 0))
director.set_actor_label('Frontier_Director')
start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 1400), unreal.Rotator(pitch=0, yaw=45, roll=0))
start.set_actor_label('PlayerStart_Frontier')
nav = actors.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0, 0, 700))
nav.set_actor_label('Navigation_Frontier')
nav.set_actor_scale3d(unreal.Vector(60, 48, 9))   # 120 x 96 x 18 m around the 112 x 88 m world
report['world_site'] = world_site.get_path_name()
report['director'] = director.get_path_name()
assert level.save_current_level(), 'M2_World save failed'
report['world_actors'] = len(actors.get_all_level_actors())

# ---- MainMenu: a live preview frontier under an orbiting camera.
menu = new_map(MENU_MAP)
menu.get_world_settings().set_editor_property('default_game_mode', unreal.DownstreamMenuGameMode)
lighting(-32, -60, 0.018)
ground_plane()
water_site(unreal.DownstreamSiteKind.PREVIEW, 'Menu_WaterSite')
assert level.save_current_level(), 'MainMenu save failed'
report['menu_actors'] = len(actors.get_all_level_actors())

# ---- The training grounds (M1) gets the flowing water too.
M1 = '/Game/Downstream/Maps/M1_Expedition'
if unreal.EditorAssetLibrary.does_asset_exist(M1) and level.load_level(M1):
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.DownstreamWaterSite):
            actor.set_editor_property('water_material', w)
    level.save_current_level()
    report['m1_water_updated'] = True

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
report.update({'passed': True, 'world_map': WORLD_MAP, 'menu_map': MENU_MAP})
(ROOT / 'build' / 'frontier-generation.json').write_text(json.dumps(report, indent=2))
unreal.log('DOWNSTREAM_FRONTIER_GENERATED ' + json.dumps(report))

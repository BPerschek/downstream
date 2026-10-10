"""Build M1 gameplay over the saved M0 environment. Generated M1 actors are replaceable."""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve().parents[1]
MAP = '/Game/Downstream/Maps/M1_Expedition'
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
if unreal.EditorAssetLibrary.does_asset_exist(MAP):
    assert level.load_level(MAP)
else:
    assert level.new_level_from_template(MAP, '/Game/Downstream/Maps/M0_TestWorld')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.DownstreamGameMode)

def material(name, color, emissive=0):
    path = '/Game/Downstream/Materials/' + name
    mat = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else tools.create_asset(name, '/Game/Downstream/Materials', unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(mat)
    expr = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -240, 0)
    expr.set_editor_property('parameter_name', 'Color')
    expr.set_editor_property('default_value', unreal.LinearColor(*color, 1))
    unreal.MaterialEditingLibrary.connect_material_property(expr, '', unreal.MaterialProperty.MP_BASE_COLOR)
    if emissive:
        glow = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -240, 150)
        glow.set_editor_property('constant', unreal.LinearColor(*(c * emissive for c in color), 1))
        unreal.MaterialEditingLibrary.connect_material_property(glow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # Canal markers render these through instanced meshes.
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    return mat

enemy_mat = material('M_PrototypeEnemy', (.72, .16, .055))
projectile_mat = material('M_PrototypeProjectile', (.05, .9, .7), 1.8)
material('M_PrototypeSalvage', (.06,.72,.56), .35)
material('M_PrototypeRecovery', (1,.62,.08), .5)
material('M_PrototypeHome', (.025,.7,.5), .35)
material('M_PrototypeFriendly', (.07,.7,.57), .1)
for name, color in [('M_PlacementValid',(.05,.8,.3)),('M_PlacementInvalid',(.95,.08,.035))]:
    preview=material(name,color,.2)
    preview.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    opacity=unreal.MaterialEditingLibrary.create_material_expression(preview,unreal.MaterialExpressionConstant,-220,300)
    opacity.set_editor_property('r',.35)
    unreal.MaterialEditingLibrary.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
    unreal.MaterialEditingLibrary.recompile_material(preview)
    assert unreal.EditorAssetLibrary.save_loaded_asset(preview,False)
factory = unreal.DataAssetFactory()
factory.set_editor_property('data_asset_class', unreal.DownstreamPrototypeTuning)
tuning = unreal.load_asset('/Game/Downstream/Data/DA_PrototypeTuning') if unreal.EditorAssetLibrary.does_asset_exist('/Game/Downstream/Data/DA_PrototypeTuning') else tools.create_asset('DA_PrototypeTuning', '/Game/Downstream/Data', unreal.DownstreamPrototypeTuning, factory)
assert tuning
tuning.set_editor_property('enemy_material', enemy_mat)
tuning.set_editor_property('projectile_material', projectile_mat)
tasks = []
for source in sorted(list((ROOT / 'art' / 'audio').glob('*.wav'))+list((ROOT / 'art' / 'cc0_feedback').glob('*.wav'))):
    path = '/Game/Downstream/Audio/' + source.stem
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = '/Game/Downstream/Audio'
        task.destination_name = source.stem
        task.automated = True
        task.save = True
        task.factory = unreal.SoundFactory()
        tasks.append(task)
if tasks:
    tools.import_asset_tasks(tasks)
for prop, name in [('shot_sound','Shot'),('hit_sound','Hit'),('pickup_sound','Pickup'),('extraction_sound','Extract')]:
    tuning.set_editor_property(prop, unreal.load_asset('/Game/Downstream/Audio/S_Prototype' + name))
for prop,name in [('build_sound','Build'),('error_sound','Error'),('dock_sound','Cargo')]:
    tuning.set_editor_property(prop,unreal.load_asset('/Game/Downstream/Audio/S_Settlement'+name))
assert unreal.EditorAssetLibrary.save_loaded_asset(tuning, False)

for actor in actors.get_all_level_actors():
    name = actor.get_actor_label()
    if name.startswith('M1_') or name.startswith('M3_') or name in ('Robot_AssemblyPreview', 'Label_CLICK GROUND TO MOVE', 'Label_HOME  /  TECHNICAL PROTOTYPE', 'Label_DOWNSTREAM'):
        actors.destroy_actor(actor)

def spawn(cls, name, pos, yaw=0):
    result = actors.spawn_actor_from_class(cls, unreal.Vector(*pos), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    assert result, name
    result.set_actor_label('M1_' + name)
    return result

spawn(unreal.DownstreamExtractionPoint, 'Extraction_Home', (-1450,-850,15))
managers=[a for a in actors.get_all_level_actors() if isinstance(a,unreal.DownstreamSettlementManager)]
assert len(managers)<=1,'One settlement manager per game world'
manager=managers[0] if managers else actors.spawn_actor_from_class(unreal.DownstreamSettlementManager,unreal.Vector(-1500,-550,10))
manager.set_actor_label('M2_SettlementManager')
enemy_layout = [
    ('Chaser_01', (-420,180,90), unreal.DownstreamEnemyKind.CHASER),
    ('Chaser_02', (140,320,90), unreal.DownstreamEnemyKind.CHASER),
    ('Chaser_03', (700,830,90), unreal.DownstreamEnemyKind.CHASER),
    ('Chaser_04', (1280,450,90), unreal.DownstreamEnemyKind.CHASER),
    ('Chaser_05', (1980,780,90), unreal.DownstreamEnemyKind.CHASER),
    ('Chaser_06', (2430,1440,90), unreal.DownstreamEnemyKind.CHASER),
    ('Shooter_01', (880,-330,90), unreal.DownstreamEnemyKind.SHOOTER),
    ('Shooter_02', (1780,1040,90), unreal.DownstreamEnemyKind.SHOOTER),
    ('Shooter_03', (2480,380,90), unreal.DownstreamEnemyKind.SHOOTER),
]
for name, pos, kind in enemy_layout:
    spawn(unreal.DownstreamEnemy, name, pos).set_editor_property('kind', kind)
for i, pos in enumerate([(500,650,60), (1330,720,60), (2070,1080,60)]):
    pickup = spawn(unreal.DownstreamSalvagePickup, 'Salvage_' + str(i+1), pos)
    pickup.set_editor_property('amount', 4)

cube = unreal.load_asset('/Engine/BasicShapes/Cube')
for i, (pos, scale) in enumerate([((3550,0,180),(1,54,5)),((-3550,0,180),(1,54,5)),((0,2650,180),(72,1,5)),((0,-2650,180),(72,1,5))]):
    border = spawn(unreal.StaticMeshActor, 'Boundary_' + str(i), pos)
    border.static_mesh_component.set_static_mesh(cube)
    border.static_mesh_component.set_collision_profile_name('BlockAll')
    border.set_actor_scale3d(unreal.Vector(*scale))
    border.set_actor_hidden_in_game(True)

def label(name, text, pos, color, size=40):
    obj = spawn(unreal.TextRenderActor, name, pos)
    obj.set_actor_rotation(unreal.Rotator(pitch=55, yaw=-135, roll=0), False)
    comp = obj.get_component_by_class(unreal.TextRenderComponent)
    comp.set_text(text)
    comp.set_world_size(size)
    comp.set_text_render_color(unreal.Color(*color,255))
    comp.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
label('HomeSign', 'HOME / EXTRACT [E]', (-1450,-1400,190), (95,235,206), 45)

# ---- M3: Drowned Vault water site (south-east). Terrain/water are native presentation of the WaterGrid.
SITE_ORIGIN = (300, -2600, 0)
SITE_RECT = (250, -2660, 3560, -540)  # x0, y0, x1, y1 footprint plus margin
MEL = unreal.MaterialEditingLibrary
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
def const3(mat, rgb, x, y):
    return node(mat, unreal.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(*rgb, 1))
def const1(mat, v, x, y):
    return node(mat, unreal.MaterialExpressionConstant, x, y, r=v)
def op(mat, cls, a, b, x, y, a_out='', b_out=''):
    expr = node(mat, cls, x, y)
    MEL.connect_material_expressions(a, a_out, expr, 'A')
    MEL.connect_material_expressions(b, b_out, expr, 'B')
    return expr
def lerp(mat, a, b, alpha, x, y, alpha_out=''):
    expr = node(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    MEL.connect_material_expressions(a, '', expr, 'A')
    MEL.connect_material_expressions(b, '', expr, 'B')
    MEL.connect_material_expressions(alpha, alpha_out, expr, 'Alpha')
    return expr
def sat(mat, a, x, y, a_out=''):
    expr = node(mat, unreal.MaterialExpressionSaturate, x, y)
    MEL.connect_material_expressions(a, a_out, expr, '')
    return expr

# Ground: UV0.x = metres dug(-)/piled(+), UV0.y = height (m). Contour bands every 0.5 m.
ground_mat = fresh_material('M_SiteGround')
uv = node(ground_mat, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
delta = node(ground_mat, unreal.MaterialExpressionComponentMask, -1200, 0, r=True, g=False, b=False, a=False)
MEL.connect_material_expressions(uv, '', delta, '')
height = node(ground_mat, unreal.MaterialExpressionComponentMask, -1200, 150, r=False, g=True, b=False, a=False)
MEL.connect_material_expressions(uv, '', height, '')
h01 = sat(ground_mat, op(ground_mat, unreal.MaterialExpressionMultiply, height, const1(ground_mat, 1/1.6, -1200, 260), -1050, 150), -900, 150)
base = lerp(ground_mat, const3(ground_mat, (.10, .08, .055), -900, -200), const3(ground_mat, (.34, .25, .15), -900, -100), h01, -700, -150)
dug = sat(ground_mat, op(ground_mat, unreal.MaterialExpressionMultiply, delta, const1(ground_mat, -4, -1200, 60), -1050, 0), -900, 0)
dug_col = lerp(ground_mat, base, const3(ground_mat, (.20, .11, .05), -700, -40), op(ground_mat, unreal.MaterialExpressionMultiply, dug, const1(ground_mat, .8, -900, 60), -800, 20), -550, -100)
berm = sat(ground_mat, op(ground_mat, unreal.MaterialExpressionMultiply, delta, const1(ground_mat, 4, -1200, 110), -1050, 60), -900, 60)
berm_col = lerp(ground_mat, dug_col, const3(ground_mat, (.47, .37, .22), -550, 40), op(ground_mat, unreal.MaterialExpressionMultiply, berm, const1(ground_mat, .75, -900, 120), -800, 90), -400, -50)
frac = node(ground_mat, unreal.MaterialExpressionFrac, -900, 300)
MEL.connect_material_expressions(op(ground_mat, unreal.MaterialExpressionMultiply, height, const1(ground_mat, 2, -1200, 360), -1050, 300), '', frac, '')
band = sat(ground_mat, op(ground_mat, unreal.MaterialExpressionMultiply, op(ground_mat, unreal.MaterialExpressionSubtract, const1(ground_mat, .1, -900, 400), frac, -800, 350), const1(ground_mat, 12, -800, 450), -700, 380), -600, 380)
shade = op(ground_mat, unreal.MaterialExpressionSubtract, const1(ground_mat, 1, -600, 300), op(ground_mat, unreal.MaterialExpressionMultiply, band, const1(ground_mat, .25, -600, 450), -500, 400), -400, 350)
ground_col = op(ground_mat, unreal.MaterialExpressionMultiply, berm_col, shade, -250, 0)
MEL.connect_material_property(ground_col, '', unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(const1(ground_mat, .92, -250, 200), '', unreal.MaterialProperty.MP_ROUGHNESS)
MEL.connect_material_property(op(ground_mat, unreal.MaterialExpressionMultiply, ground_col, const1(ground_mat, .03, -400, 250), -150, 250), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(ground_mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(ground_mat, False)

# Water: UV0.x = depth (m), UV0.y = flow speed (m/s).
water_mat = fresh_material('M_Water')
water_mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
water_mat.set_editor_property('two_sided', True)
wuv = node(water_mat, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
depth = node(water_mat, unreal.MaterialExpressionComponentMask, -1200, 0, r=True, g=False, b=False, a=False)
MEL.connect_material_expressions(wuv, '', depth, '')
speed = node(water_mat, unreal.MaterialExpressionComponentMask, -1200, 150, r=False, g=True, b=False, a=False)
MEL.connect_material_expressions(wuv, '', speed, '')
d01 = sat(water_mat, op(water_mat, unreal.MaterialExpressionMultiply, depth, const1(water_mat, 1/1.4, -1200, 80), -1050, 0), -900, 0)
wcol = lerp(water_mat, const3(water_mat, (.18, .55, .62), -900, -200), const3(water_mat, (.03, .16, .34), -900, -100), d01, -700, -150)
foam = sat(water_mat, op(water_mat, unreal.MaterialExpressionMultiply, speed, const1(water_mat, .5, -1200, 220), -1050, 150), -900, 150)
wfinal = lerp(water_mat, wcol, const3(water_mat, (.85, .93, .96), -700, 0), op(water_mat, unreal.MaterialExpressionMultiply, foam, const1(water_mat, .5, -900, 220), -800, 180), -500, -50)
MEL.connect_material_property(wfinal, '', unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(op(water_mat, unreal.MaterialExpressionMultiply, wfinal, const1(water_mat, .35, -500, 120), -300, 100), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
wet = sat(water_mat, op(water_mat, unreal.MaterialExpressionMultiply, depth, const1(water_mat, 60, -1200, 330), -1050, 300), -900, 300)
alpha = op(water_mat, unreal.MaterialExpressionAdd, const1(water_mat, .5, -900, 400), op(water_mat, unreal.MaterialExpressionMultiply, d01, const1(water_mat, .38, -900, 480), -800, 450), -700, 420)
MEL.connect_material_property(op(water_mat, unreal.MaterialExpressionMultiply, wet, alpha, -500, 350), '', unreal.MaterialProperty.MP_OPACITY)
MEL.connect_material_property(const1(water_mat, .06, -300, 250), '', unreal.MaterialProperty.MP_ROUGHNESS)
MEL.connect_material_property(const1(water_mat, .9, -300, 300), '', unreal.MaterialProperty.MP_SPECULAR)
MEL.recompile_material(water_mat)
assert unreal.EditorAssetLibrary.save_loaded_asset(water_mat, False)
concrete = material('M_SiteConcrete', (.33, .34, .32))
terrain_mat = ground_mat
removed_for_site = []
for actor in actors.get_all_level_actors():
    name = actor.get_actor_label()
    loc = actor.get_actor_location()
    in_site = SITE_RECT[0] <= loc.x <= SITE_RECT[2] and SITE_RECT[1] <= loc.y <= SITE_RECT[3]
    # The M0 developer patch stays in M0_TestWorld; M1 uses the playable water site instead.
    if name in ('Native_Terrain_32x32', 'Label_NativeTerrain') or (in_site and name.startswith(('Rock_', 'DryTree_', 'RouteBeacon_'))):
        removed_for_site.append(name)
        actors.destroy_actor(actor)
site = spawn(unreal.DownstreamWaterSite, 'WaterSite', SITE_ORIGIN)
site.set_actor_label('M3_WaterSite')
if terrain_mat:
    site.set_editor_property('terrain_material', terrain_mat)
site.set_editor_property('water_material', water_mat)
try:
    site.rerun_construction_scripts()
except Exception:
    pass
label('SiteSign', 'DROWNED VAULT / drain it with a digger canal', (1900, -1500, 460), (120, 200, 255), 38)
for actor in actors.get_all_level_actors():
    if actor.get_actor_label() == 'M1_SiteSign':
        actor.set_actor_label('M3_SiteSign')
label('RouteSign', 'SALVAGE ROUTE', (-180,-450,190), (239,212,163), 35)
assert level.save_current_level(), 'M1 save failed; close other processes using this map'
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
report = {'passed':True,'map':MAP,'enemies':len(enemy_layout),'chasers':6,'shooters':3,'salvage_crates':3,'home':[-1450,-850,105],'audio_imports':len(tasks),'settlement_manager':manager.get_path_name(),'water_site':site.get_path_name(),'removed_for_site':removed_for_site,'actors':len(actors.get_all_level_actors())}
(ROOT / 'build' / 'expedition-generation.json').write_text(json.dumps(report, indent=2))
unreal.log('DOWNSTREAM_EXPEDITION_GENERATED ' + json.dumps(report))

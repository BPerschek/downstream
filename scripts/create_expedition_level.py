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
    if name.startswith(('M1_', 'M3_', 'M4_')) or name in ('Robot_AssemblyPreview', 'Label_CLICK GROUND TO MOVE', 'Label_HOME  /  TECHNICAL PROTOTYPE', 'Label_DOWNSTREAM'):
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

# ---- CC0 ambientCG textures (third_party/ambientcg, imported by art/import_third_party.py).
ACG = '/Game/Downstream/ThirdParty/AmbientCG/'
def tex(mat, name, uv, x, y, kind='color'):
    t = unreal.load_asset(ACG + name)
    assert t, 'Missing texture ' + name + '; run scripts/import_third_party.ps1'
    sampler = {'color': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 'normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
               'linear': unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR}[kind]
    expr = node(mat, unreal.MaterialExpressionTextureSample, x, y, texture=t, sampler_type=sampler)
    MEL.connect_material_expressions(uv, '', expr, 'UVs')
    return expr
def world_uv(mat, tile_cm, x, y):
    wp = node(mat, unreal.MaterialExpressionWorldPosition, x - 300, y)
    rg = node(mat, unreal.MaterialExpressionComponentMask, x - 150, y, r=True, g=True, b=False, a=False)
    MEL.connect_material_expressions(wp, '', rg, '')
    return op(mat, unreal.MaterialExpressionDivide, rg, const1(mat, tile_cm, x - 150, y + 80), x, y)

def tiled_material(name, prefix, tile_cm, tint=(1, 1, 1)):
    mat = fresh_material(name)
    uv = world_uv(mat, tile_cm, -900, 0)
    color = op(mat, unreal.MaterialExpressionMultiply, tex(mat, prefix + '_Color', uv, -600, -200), const3(mat, tint, -600, -50), -350, -150)
    MEL.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(tex(mat, prefix + '_NormalDX', uv, -600, 100, 'normal'), 'RGB', unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(tex(mat, prefix + '_Roughness', uv, -600, 350, 'linear'), 'R', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    return mat
open_ground_mat = tiled_material('M_OpenGround', 'T_Ground054', 450, (.92, .86, .78))
concrete_tex_mat = tiled_material('M_ConcretePad', 'T_Concrete034', 300, (.8, .8, .78))

# Site ground: UV0.x = metres dug(-)/piled(+), UV0.y = height (m). Sand on high ground,
# brown dirt low and in fresh cuts, rocky spoil on berms; contour bands every 0.5 m.
ground_mat = fresh_material('M_SiteGround')
uv = node(ground_mat, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
delta = node(ground_mat, unreal.MaterialExpressionComponentMask, -1200, 0, r=True, g=False, b=False, a=False)
MEL.connect_material_expressions(uv, '', delta, '')
height = node(ground_mat, unreal.MaterialExpressionComponentMask, -1200, 150, r=False, g=True, b=False, a=False)
MEL.connect_material_expressions(uv, '', height, '')
wuv = world_uv(ground_mat, 450, -1400, 700)
sand = op(ground_mat, unreal.MaterialExpressionMultiply, tex(ground_mat, 'T_Ground054_Color', wuv, -1100, 600), const3(ground_mat, (.92, .86, .78), -1100, 760), -900, 620)
dirt = tex(ground_mat, 'T_Ground103_Color', wuv, -1100, 850)
rocky = tex(ground_mat, 'T_Ground109_Color', wuv, -1100, 1100)
h01 = sat(ground_mat, op(ground_mat, unreal.MaterialExpressionMultiply, height, const1(ground_mat, 1/1.4, -1200, 260), -1050, 150), -900, 150)
base = lerp(ground_mat, dirt, sand, h01, -700, -150)
dug = sat(ground_mat, op(ground_mat, unreal.MaterialExpressionMultiply, delta, const1(ground_mat, -4, -1200, 60), -1050, 0), -900, 0)
dark_dirt = op(ground_mat, unreal.MaterialExpressionMultiply, dirt, const1(ground_mat, .7, -900, 900), -800, 880)
dug_col = lerp(ground_mat, base, dark_dirt, op(ground_mat, unreal.MaterialExpressionMultiply, dug, const1(ground_mat, .85, -900, 60), -800, 20), -550, -100)
berm = sat(ground_mat, op(ground_mat, unreal.MaterialExpressionMultiply, delta, const1(ground_mat, 4, -1200, 110), -1050, 60), -900, 60)
berm_col = lerp(ground_mat, dug_col, rocky, op(ground_mat, unreal.MaterialExpressionMultiply, berm, const1(ground_mat, .85, -900, 120), -800, 90), -400, -50)
frac = node(ground_mat, unreal.MaterialExpressionFrac, -900, 300)
MEL.connect_material_expressions(op(ground_mat, unreal.MaterialExpressionMultiply, height, const1(ground_mat, 2, -1200, 360), -1050, 300), '', frac, '')
band = sat(ground_mat, op(ground_mat, unreal.MaterialExpressionMultiply, op(ground_mat, unreal.MaterialExpressionSubtract, const1(ground_mat, .1, -900, 400), frac, -800, 350), const1(ground_mat, 12, -800, 450), -700, 380), -600, 380)
shade = op(ground_mat, unreal.MaterialExpressionSubtract, const1(ground_mat, 1, -600, 300), op(ground_mat, unreal.MaterialExpressionMultiply, band, const1(ground_mat, .22, -600, 450), -500, 400), -400, 350)
ground_col = op(ground_mat, unreal.MaterialExpressionMultiply, berm_col, shade, -250, 0)
MEL.connect_material_property(ground_col, '', unreal.MaterialProperty.MP_BASE_COLOR)
normal = lerp(ground_mat, tex(ground_mat, 'T_Ground103_NormalDX', wuv, -1100, 1350, 'normal'), tex(ground_mat, 'T_Ground054_NormalDX', wuv, -1100, 1600, 'normal'), h01, -500, 1400)
MEL.connect_material_property(normal, '', unreal.MaterialProperty.MP_NORMAL)
MEL.connect_material_property(const1(ground_mat, .9, -250, 200), '', unreal.MaterialProperty.MP_ROUGHNESS)
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
# ---- CC0 surfaces on the inherited M0 environment.
for actor in actors.get_all_level_actors():
    name = actor.get_actor_label()
    if name in ('Ground_70m', 'HomeClearing', 'RuinFoundation') and isinstance(actor, unreal.StaticMeshActor):
        actor.static_mesh_component.set_material(0, open_ground_mat if name == 'Ground_70m' else concrete_tex_mat)

# ---- CC0 Kenney props (third_party/kenney-*). Generated; replaced on every run.
KENNEY = '/Game/Downstream/ThirdParty/Kenney/'
def prop(label, model, pos, yaw=0, scale=1.0, collide=True):
    kit, name = model.split('/')
    mesh = unreal.load_asset(f'{KENNEY}{kit}/{name}/SM_K_{name}')
    assert mesh, 'Missing Kenney mesh ' + model + '; run scripts/import_third_party.ps1'
    a = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*pos), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    a.set_actor_label('M4_' + label)
    a.static_mesh_component.set_static_mesh(mesh)
    a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    a.static_mesh_component.set_collision_profile_name('BlockAll' if collide else 'NoCollision')
    return a
props = []
# Home perimeter: fortified fence on the north and west edges, outside the build grid.
for i, x in enumerate(range(-2300, -700, 150)):
    props.append(prop(f'HomeFenceN_{i}', 'Survival/fence_fortified', (x, 330, 0), 0, 3.0))
for i, y in enumerate(range(-1200, 250, 150)):
    props.append(prop(f'HomeFenceW_{i}', 'Survival/fence_fortified', (-2480, y, 0), 90, 3.0))
props.append(prop('HomeTent', 'Survival/tent_canvas', (-2750, -650, 0), 90, 3.5))
for i, (x, y) in enumerate([(-2400, 230), (-2330, 200), (-2390, 140)]):
    props.append(prop(f'HomeBarrel_{i}', 'Survival/barrel', (x, y, 0), i * 40, 2.6))
props.append(prop('HomeSignpost', 'Survival/signpost', (-650, -350, 0), -45, 3.0, False))
# Industrial ruin cover.
for i, (x, y, yaw) in enumerate([(1050, 80, 10), (2100, 250, 95), (1600, 1350, 15), (900, 1250, 75)]):
    props.append(prop(f'RuinContainer_{i}', 'Industrial/shipping_container_' + ('a' if i % 2 == 0 else 'b'), (x, y, 0), yaw, 6.0))
props.append(prop('RuinTank', 'Industrial/detail_tank_large', (2000, -150, 0), 30, 2.0))
props.append(prop('RuinChimney', 'Industrial/chimney_medium', (2350, 1000, 0), 0, 3.0))
props.append(prop('RuinMachine', 'Factory/machine', (1250, 1000, 0), 200, 1.6))
props.append(prop('RuinHopper', 'Factory/hopper_square', (2300, 650, 0), 0, 1.5))
for i, (x, y) in enumerate([(1450, 300), (1520, 340), (1180, 1100), (2250, 820), (980, 600)]):
    props.append(prop(f'RuinBarrel_{i}', 'Survival/barrel' + ('_open' if i % 2 else ''), (x, y, 0), i * 70, 2.6))
for i, (x, y) in enumerate([(1600, -50), (2220, 1260), (850, 450)]):
    props.append(prop(f'RuinCrate_{i}', 'Survival/box_large', (x, y, 0), i * 35, 2.6))
# Open ground.
for i, (x, y, model, yaw) in enumerate([(-600, 900, 'a', 10), (-300, -1750, 'b', 80), (-1800, 1350, 'c', 200), (2950, -330, 'b', 140),
                                        (3150, 1800, 'a', 30), (-2900, -1900, 'c', 300), (300, 1900, 'b', 60), (-1100, 2000, 'a', 120)]):
    props.append(prop(f'Rock_{i}', 'Survival/rock_sand_' + model, (x, y, 0), yaw, 3.5))
props.append(prop('SiteCone_0', 'Factory/cone', (850, -620, 0), 0, 2.0, False))
props.append(prop('SiteCone_1', 'Factory/cone', (1050, -600, 0), 0, 2.0, False))
label('SiteSign', 'DROWNED VAULT / drain it with a digger canal', (1900, -1500, 460), (120, 200, 255), 38)
for actor in actors.get_all_level_actors():
    if actor.get_actor_label() == 'M1_SiteSign':
        actor.set_actor_label('M3_SiteSign')
label('RouteSign', 'SALVAGE ROUTE', (-180,-450,190), (239,212,163), 35)
assert level.save_current_level(), 'M1 save failed; close other processes using this map'
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
report = {'passed':True,'map':MAP,'enemies':len(enemy_layout),'chasers':6,'shooters':3,'salvage_crates':3,'home':[-1450,-850,105],'audio_imports':len(tasks),'settlement_manager':manager.get_path_name(),'water_site':site.get_path_name(),'kenney_props':len(props),'removed_for_site':removed_for_site,'actors':len(actors.get_all_level_actors())}
(ROOT / 'build' / 'expedition-generation.json').write_text(json.dumps(report, indent=2))
unreal.log('DOWNSTREAM_EXPEDITION_GENERATED ' + json.dumps(report))

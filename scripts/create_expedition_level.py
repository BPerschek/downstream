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
    unreal.MaterialEditingLibrary.recompile_material(mat)
    assert unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    return mat

enemy_mat = material('M_PrototypeEnemy', (.72, .16, .055))
projectile_mat = material('M_PrototypeProjectile', (.05, .9, .7), 1.8)
material('M_PrototypeSalvage', (.06,.72,.56), .35)
material('M_PrototypeRecovery', (1,.62,.08), .5)
material('M_PrototypeHome', (.025,.7,.5), .35)
factory = unreal.DataAssetFactory()
factory.set_editor_property('data_asset_class', unreal.DownstreamPrototypeTuning)
tuning = unreal.load_asset('/Game/Downstream/Data/DA_PrototypeTuning') if unreal.EditorAssetLibrary.does_asset_exist('/Game/Downstream/Data/DA_PrototypeTuning') else tools.create_asset('DA_PrototypeTuning', '/Game/Downstream/Data', unreal.DownstreamPrototypeTuning, factory)
assert tuning
tuning.set_editor_property('enemy_material', enemy_mat)
tuning.set_editor_property('projectile_material', projectile_mat)
tasks = []
for source in sorted((ROOT / 'art' / 'audio').glob('*.wav')):
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
assert unreal.EditorAssetLibrary.save_loaded_asset(tuning, False)

for actor in actors.get_all_level_actors():
    name = actor.get_actor_label()
    if name.startswith('M1_') or name in ('Robot_AssemblyPreview', 'Label_CLICK GROUND TO MOVE', 'Label_HOME  /  TECHNICAL PROTOTYPE', 'Label_DOWNSTREAM'):
        actors.destroy_actor(actor)

def spawn(cls, name, pos, yaw=0):
    result = actors.spawn_actor_from_class(cls, unreal.Vector(*pos), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    assert result, name
    result.set_actor_label('M1_' + name)
    return result

spawn(unreal.DownstreamExtractionPoint, 'Extraction_Home', (-1450,-850,15))
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
label('RouteSign', 'SALVAGE ROUTE', (-180,-450,190), (239,212,163), 35)
assert level.save_current_level(), 'M1 save failed; close other processes using this map'
assert unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
report = {'passed':True,'map':MAP,'enemies':len(enemy_layout),'chasers':6,'shooters':3,'salvage_crates':3,'home':[-1450,-850,105],'original_audio':len(tasks),'actors':len(actors.get_all_level_actors())}
(ROOT / 'build' / 'expedition-generation.json').write_text(json.dumps(report, indent=2))
unreal.log('DOWNSTREAM_EXPEDITION_GENERATED ' + json.dumps(report))

"""Rebuild the original M0 map in a separate Unreal editor commandlet."""
import json
import hashlib
import math
import random
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve().parents[1]
MAP = '/Game/Downstream/Maps/M0_TestWorld'
ASSETS = '/Game/Downstream/Art'
MATERIALS = '/Game/Downstream/Materials'
actor_system = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_system = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def material(name, color, metallic=0.0, roughness=0.85, emissive=0.0):
    path = f'{MATERIALS}/{name}'
    result = unreal.load_asset(path)
    if result:
        unreal.MaterialEditingLibrary.delete_all_material_expressions(result)
    else:
        result = asset_tools.create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    expr = unreal.MaterialEditingLibrary.create_material_expression(result, unreal.MaterialExpressionConstant3Vector, -250, 0)
    expr.set_editor_property('constant', unreal.LinearColor(*color[:3], 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(expr, '', unreal.MaterialProperty.MP_BASE_COLOR)
    for offset, value, prop in [(120, roughness, unreal.MaterialProperty.MP_ROUGHNESS), (240, metallic, unreal.MaterialProperty.MP_METALLIC)]:
        scalar = unreal.MaterialEditingLibrary.create_material_expression(result, unreal.MaterialExpressionConstant, -250, offset)
        scalar.set_editor_property('r', value)
        unreal.MaterialEditingLibrary.connect_material_property(scalar, '', prop)
    if emissive:
        glow = unreal.MaterialEditingLibrary.create_material_expression(result, unreal.MaterialExpressionConstant3Vector, -250, 360)
        glow.set_editor_property('constant', unreal.LinearColor(*(c * emissive for c in color[:3]), 1.0))
        unreal.MaterialEditingLibrary.connect_material_property(glow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(result)
    unreal.EditorAssetLibrary.save_loaded_asset(result, False)
    return result


def spawn_mesh(name, mesh, position, scale=(1, 1, 1), yaw=0, mat=None, collision=True):
    actor = actor_system.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*position), unreal.Rotator(pitch=0, yaw=yaw, roll=0))
    actor.set_actor_label(name)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    component = actor.static_mesh_component
    component.set_static_mesh(mesh)
    component.set_mobility(unreal.ComponentMobility.STATIC)
    component.set_collision_profile_name('BlockAll' if collision else 'NoCollision')
    if mat:
        component.set_material(0, mat)
    return actor


def label(text, position, size=52, color=(180, 218, 209)):
    actor = actor_system.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(*position), unreal.Rotator(pitch=55, yaw=-135, roll=0))
    actor.set_actor_label('Label_' + text.split('\n')[0])
    comp = actor.get_component_by_class(unreal.TextRenderComponent)
    comp.set_text(text)
    comp.set_world_size(size)
    comp.set_text_render_color(unreal.Color(*color, 255))
    comp.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    return actor


def import_original_art():
    # The exporter supplies material colors; the fallback palette also keeps regeneration independent of manifest layout.
    manifest = json.loads((ROOT / 'art' / 'asset_manifest.json').read_text())
    mats = {name: material(name, values['rgba'], metallic=values.get('metallic', 0.0), roughness=values.get('roughness', 0.85)) for name, values in manifest['palette'].items()}
    exports = sorted((ROOT / 'art' / 'exports').glob('SM_*.fbx'))
    if not exports:
        raise RuntimeError('No original FBX exports found. Run art/create_blockouts.py with Blender first.')
    tasks = []
    state_path = ROOT / 'build' / 'art-import-state.json'
    previous = json.loads(state_path.read_text()) if state_path.exists() else {}
    current = {source.stem: hashlib.sha256(source.read_bytes()).hexdigest() for source in exports}
    for source in exports:
        if previous.get(source.stem) == current[source.stem] and unreal.EditorAssetLibrary.does_asset_exist(f'{ASSETS}/{source.stem}'):
            continue
        task = unreal.AssetImportTask()
        task.filename = str(source)
        task.destination_path = ASSETS
        task.destination_name = source.stem
        task.automated = True
        task.replace_existing = True
        task.save = True
        opts = unreal.FbxImportUI()
        opts.import_mesh = True
        opts.import_materials = False
        opts.import_textures = False
        opts.import_as_skeletal = False
        opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        opts.static_mesh_import_data.combine_meshes = True
        opts.static_mesh_import_data.auto_generate_collision = True
        task.options = opts
        tasks.append(task)
    if tasks:
        asset_tools.import_asset_tasks(tasks)
    meshes = {}
    for source in exports:
        mesh = unreal.load_asset(f'{ASSETS}/{source.stem}')
        if not mesh:
            raise RuntimeError(f'Missing imported mesh {source.stem}')
        slots = mesh.get_editor_property('static_materials')
        updated_slots = []
        for slot in slots:
            name = str(slot.material_slot_name)
            if name in mats:
                slot.material_interface = mats[name]
            updated_slots.append(slot)
        mesh.set_editor_property('static_materials', updated_slots)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh, False)
        meshes[source.stem] = mesh
    state_path.parent.mkdir(parents=True, exist_ok=True)
    state_path.write_text(json.dumps(current, indent=2))
    return meshes


def configure_player_template():
    bp = unreal.load_asset('/Game/TopDown/Blueprints/BP_TopDownCharacter')
    if not bp:
        raise RuntimeError('Install/copy the Epic Top Down Blueprint template dependencies first.')
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    player_mesh = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple')
    player_animation = unreal.EditorAssetLibrary.load_blueprint_class('/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed')
    if not player_mesh or not player_animation:
        raise RuntimeError('Required Epic character assets must be mounted under /Game/Characters')
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, bp)
        if isinstance(obj, unreal.SpringArmComponent):
            obj.set_editor_property('target_arm_length', 1850.0)
            obj.set_editor_property('relative_rotation', unreal.Rotator(pitch=-55, yaw=45, roll=0))
            obj.set_editor_property('absolute_rotation', True)
            obj.set_editor_property('do_collision_test', False)
        elif isinstance(obj, unreal.CameraComponent):
            obj.set_editor_property('field_of_view', 55.0)
        elif isinstance(obj, unreal.SkeletalMeshComponent):
            obj.set_skeletal_mesh_asset(player_mesh)
            obj.set_editor_property('relative_location', unreal.Vector(0, 0, -96))
            obj.set_editor_property('relative_rotation', unreal.Rotator(pitch=0, yaw=-90, roll=0))
            obj.set_anim_instance_class(player_animation)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    # Force class defaults to be refreshed after template component changes.
    if hasattr(unreal, 'BlueprintEditorLibrary'):
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)


def main():
    meshes = import_original_art()
    configure_player_template()
    unreal.EditorAssetLibrary.make_directory('/Game/Downstream/Maps')
    if unreal.EditorAssetLibrary.does_asset_exist(MAP):
        if not level_system.load_level(MAP):
            raise RuntimeError('Could not load generated test map')
        # Regeneration replaces only actors in this dedicated generated map.
        for actor in actor_system.get_all_level_actors():
            if not isinstance(actor, unreal.WorldSettings) and actor.get_class().get_name() != 'Brush':
                actor_system.destroy_actor(actor)
    elif not level_system.new_level(MAP):
        raise RuntimeError('Could not create test map')
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    game_mode = unreal.EditorAssetLibrary.load_blueprint_class('/Game/TopDown/Blueprints/BP_TopDownGameMode')
    world.get_world_settings().set_editor_property('default_game_mode', game_mode)
    cube = unreal.load_asset('/Engine/BasicShapes/Cube')
    sand = material('M_Ground', (0.31, 0.235, 0.15, 1))
    path_mat = material('M_Path', (0.52, 0.405, 0.27, 1))
    home_mat = material('M_HomePad', (0.18, 0.29, 0.265, 1))
    ruin_mat = material('M_RuinPad', (0.24, 0.225, 0.19, 1))
    teal = material('M_Beacon', (0.04, 0.65, 0.53, 1), emissive=0.3)
    rust = material('M_Rust', (0.55, 0.18, 0.065, 1))
    spawn_mesh('Ground_70m', cube, (0, 0, -65), (70, 52, 1.2), mat=sand)
    spawn_mesh('HomeClearing', cube, (-1500, -550, -3), (17, 15, 0.12), mat=home_mat)
    spawn_mesh('RuinFoundation', cube, (1550, 650, -2), (17, 16, 0.12), mat=ruin_mat)
    spawn_mesh('SalvageRoute', cube, (0, 180, 1), (26, 3.8, 0.04), yaw=22, mat=path_mat, collision=False)
    spawn_mesh('HomeShelter', meshes['SM_Shelter'], (-1740, -240, 6), yaw=0)
    spawn_mesh('Robot_AssemblyPreview', meshes['SM_SalvageBot'], (-1370, -300, 6), yaw=45)
    for i, pos in enumerate([(-1970, -600, 6), (-1920, -710, 6), (-1130, -190, 6)]):
        spawn_mesh(f'HomeCrate_{i}', meshes['SM_Crate'], pos, yaw=i * 40)
    spawn_mesh('HomePowerPole', meshes['SM_PowerPole'], (-2150, -70, 6), yaw=15)
    spawn_mesh('HomeDrum', meshes['SM_Drum'], (-1450, 30, 6))
    spawn_mesh('SalvageStack', meshes['SM_ScrapStack'], (1700, 680, 6), yaw=35)
    for i, (x, y, yaw) in enumerate([(950, 100, 90), (950, 400, 90), (1450, 1250, 0), (1780, 1250, 0), (2220, 860, 90), (2200, 100, 90)]):
        spawn_mesh(f'RuinWall_{i}', meshes['SM_BrokenWall'], (x, y, 6), yaw=yaw)
    for i, pos in enumerate([(1500, 1100, 6), (1800, 180, 6), (1230, 750, 6)]):
        spawn_mesh(f'RuinDrum_{i}', meshes['SM_Drum'], pos, yaw=i * 20)
    rng = random.Random(41009)
    for i in range(34):
        x, y = rng.uniform(-3200, 3200), rng.uniform(-2300, 2300)
        if (abs(x + 1500) < 1100 and abs(y + 550) < 900) or (abs(x - 1550) < 1100 and abs(y - 650) < 1100) or abs(y - 0.4*x - 180) < 320:
            continue
        scale = rng.uniform(0.8, 1.8)
        spawn_mesh(f'Rock_{i}', meshes['SM_RockCluster'], (x, y, 0), (scale, scale, scale), yaw=rng.uniform(0, 360))
    for i, pos in enumerate([(-2900, 900, 0), (-2500, -1800, 0), (2800, 1600, 0), (500, -1900, 0), (400, 1900, 0)]):
        spawn_mesh(f'DryTree_{i}', meshes['SM_AngularTree'], pos, yaw=i * 53)
    for i in range(7):
        x, y = -750 + i*280, -120 + i*110
        spawn_mesh(f'RouteBeacon_{i}', cube, (x, y, 45), (0.18, 0.18, 0.9), mat=teal, collision=False)
    label('DOWNSTREAM', (-1860, -1160, 220), 96)
    label('HOME  /  TECHNICAL PROTOTYPE', (-1800, -1190, 115), 28)
    label('INDUSTRIAL RUIN', (1650, 1350, 330), 50, (238, 187, 115))
    label('CLICK GROUND TO MOVE', (-920, -1070, 160), 30)
    spawn = actor_system.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-1450, -850, 105), unreal.Rotator(pitch=0, yaw=45, roll=0))
    spawn.set_actor_label('PlayerStart_Home')
    sun = actor_system.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 2000), unreal.Rotator(pitch=-48, yaw=-35, roll=0))
    sun.set_actor_label('Sun')
    sun.light_component.set_editor_property('intensity', 5.0)
    sun.light_component.set_editor_property('light_color', unreal.Color(255, 223, 174, 255))
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky = actor_system.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1000))
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property('intensity', 0.8)
    sky.light_component.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
    sky.light_component.set_editor_property('cubemap', unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
    atmosphere = actor_system.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, -1000))
    bounds = actor_system.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0, 0, 150))
    bounds.set_actor_label('Navigation_TestWorld')
    bounds.set_actor_scale3d(unreal.Vector(36, 27, 5))
    unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
    # Dynamic generation builds the prototype navmesh at game start without editor-only build calls.
    level_system.set_level_viewport_camera_info(unreal.Vector(-3000, -3300, 4500), unreal.Rotator(pitch=-55, yaw=45, roll=0), 'None')
    if not level_system.save_current_level():
        raise RuntimeError('Saving map failed')
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    result = {
        'map': MAP, 'actors': len(actor_system.get_all_level_actors()),
        'original_meshes': sorted(meshes), 'seed': 41009,
        'navigation_bounds': str(bounds.get_actor_bounds(False)),
        'status': 'map_generated', 'runtime_simulation_connected': False,
    }
    out = ROOT / 'build' / 'map-generation.json'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2))
    unreal.log('DOWNSTREAM_MAP_GENERATED ' + json.dumps(result))


main()

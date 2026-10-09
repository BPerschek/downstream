"""Commandlet checks for generated content and template defaults."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve().parents[1]
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert level.load_level('/Game/Downstream/Maps/M0_TestWorld'), 'Map failed to load'
all_actors = actors.get_all_level_actors()
starts = [a for a in all_actors if isinstance(a, unreal.PlayerStart)]
navs = [a for a in all_actors if isinstance(a, unreal.NavMeshBoundsVolume)]
nav_data = [a for a in all_actors if isinstance(a, unreal.RecastNavMesh)]
assert len(starts) == 1, 'Expected exactly one player start'
assert len(navs) == 1, 'Expected exactly one navigation bounds volume'
assert len(nav_data) == 1, 'Navigation data was not baked into the saved map'
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_world_settings().get_editor_property('default_game_mode'), 'GameMode not configured'
meshes = []
manifest = json.loads((root / 'art' / 'asset_manifest.json').read_text())
for asset in unreal.EditorAssetLibrary.list_assets('/Game/Downstream/Art', recursive=False):
    mesh = unreal.load_asset(asset)
    if isinstance(mesh, unreal.StaticMesh):
        bounds = mesh.get_bounding_box()
        dimensions = bounds.max - bounds.min
        slots = mesh.get_editor_property('static_materials')
        assert all(slot.material_interface for slot in slots), 'Unassigned original art material'
        assert all(slot.material_interface.get_name() == str(slot.material_slot_name) for slot in slots), 'Original art palette assignment mismatch: ' + str([(str(s.material_slot_name), s.material_interface.get_name()) for s in slots])
        meshes.append({'name': mesh.get_name(), 'dimensions_cm': [dimensions.x, dimensions.y, dimensions.z], 'materials': [str(s.material_slot_name) for s in slots]})
assert len(meshes) == 9, 'Expected all nine original props'
shelter = next(m for m in meshes if m['name'] == 'SM_Shelter')
assert 420 < max(shelter['dimensions_cm'][:2]) < 470, 'FBX scale should produce a ~4.4m shelter'
bp = unreal.load_asset('/Game/TopDown/Blueprints/BP_TopDownCharacter')
player_class = unreal.EditorAssetLibrary.load_blueprint_class('/Game/TopDown/Blueprints/BP_TopDownCharacter')
player_mesh = unreal.get_default_object(player_class).get_component_by_class(unreal.SkeletalMeshComponent)
assert player_mesh and player_mesh.get_skeletal_mesh_asset(), 'Visible player skeletal mesh is missing'
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
arms = []
for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, bp)
    if isinstance(obj, unreal.SpringArmComponent):
        rot = obj.get_editor_property('relative_rotation')
        assert abs(rot.pitch + 55) < 0.01 and abs(rot.yaw - 45) < 0.01, 'Fixed camera angle was not saved: ' + str(rot)
        assert obj.get_editor_property('absolute_rotation'), 'Camera must ignore player yaw'
        arms.append({'pitch': rot.pitch, 'yaw': rot.yaw, 'arm_cm': obj.get_editor_property('target_arm_length')})
assert arms, 'Spring arm template not found'
report = {'passed': True, 'actor_count': len(all_actors), 'player_starts': len(starts), 'nav_volumes': len(navs), 'camera': arms, 'original_meshes': meshes}
(root / 'build' / 'world-verification.json').write_text(json.dumps(report, indent=2))
unreal.log('DOWNSTREAM_WORLD_VERIFIED ' + json.dumps(report))

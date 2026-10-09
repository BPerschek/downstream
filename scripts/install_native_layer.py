"""Attach the compiled M0 presentation spike to the existing saved test map."""
import json
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve().parents[1]
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert level.load_level('/Game/Downstream/Maps/M0_TestWorld')
tools = unreal.AssetToolsHelpers.get_asset_tools()
material_path = '/Game/Downstream/Materials/M_TerrainDebug'
mat = unreal.load_asset(material_path) if unreal.EditorAssetLibrary.does_asset_exist(material_path) else None
if not mat:
    mat = tools.create_asset('M_TerrainDebug', '/Game/Downstream/Materials', unreal.Material, unreal.MaterialFactoryNew())
    vertex = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -220, 0)
    unreal.MaterialEditingLibrary.connect_material_property(vertex, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = unreal.MaterialEditingLibrary.create_material_expression(mat, unreal.MaterialExpressionConstant, -220, 140)
    rough.set_editor_property('r', 0.9)
    unreal.MaterialEditingLibrary.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
existing = [actor for actor in actors.get_all_level_actors() if isinstance(actor, unreal.DownstreamTerrainChunk)]
assert len(existing) <= 1, 'This technical spike supports one dirty-set consumer'
chunk = existing[0] if existing else actors.spawn_actor_from_class(unreal.DownstreamTerrainChunk, unreal.Vector(1500, -2300, 210))
chunk.set_actor_label('Native_Terrain_32x32')
chunk.set_editor_property('terrain_material', mat)
chunk.set_editor_property('chunk_cell_count', 32)
chunk.set_editor_property('cell_size', 50.0)
chunk.set_editor_property('height_visual_scale', 15.0)
if not any(actor.get_actor_label() == 'Label_NativeTerrain' for actor in actors.get_all_level_actors()):
    label = actors.spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(2300, -1200, 470), unreal.Rotator(pitch=55, yaw=-135, roll=0))
    label.set_actor_label('Label_NativeTerrain')
    text = label.get_component_by_class(unreal.TextRenderComponent)
    text.set_text('NATIVE TERRAIN / 32 x 32')
    text.set_world_size(44)
    text.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
assert level.save_current_level(), 'Saving native map layer failed'
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
report = {'passed': True, 'native_actor': chunk.get_path_name(), 'material': mat.get_path_name(), 'cells': 32, 'collision': False, 'game_thread_simulation': True}
(root / 'build' / 'native-layer-install.json').write_text(json.dumps(report, indent=2))
unreal.log('DOWNSTREAM_NATIVE_LAYER_INSTALLED ' + json.dumps(report))

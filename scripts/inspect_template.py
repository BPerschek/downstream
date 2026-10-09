import json
from pathlib import Path
import unreal

report = {}
bp = unreal.load_asset('/Game/TopDown/Blueprints/BP_TopDownCharacter')
report['blueprint_loaded'] = bool(bp)
if bp:
    cls = unreal.EditorAssetLibrary.load_blueprint_class('/Game/TopDown/Blueprints/BP_TopDownCharacter')
    default = unreal.get_default_object(cls)
    report['default_class'] = str(cls)
    report['components'] = []
    for comp in default.get_components_by_class(unreal.ActorComponent):
        entry = {'name': comp.get_name(), 'class': comp.get_class().get_name()}
        if isinstance(comp, unreal.SkeletalMeshComponent):
            entry['mesh_asset'] = str(comp.get_skeletal_mesh_asset())
            entry['visible'] = comp.get_editor_property('visible')
            entry['hidden_in_game'] = comp.get_editor_property('hidden_in_game')
        report['components'].append(entry)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    report['templates'] = []
    for handle in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        report['templates'].append({'name': obj.get_name() if obj else None, 'class': obj.get_class().get_name() if obj else None})
report['available'] = {name: hasattr(unreal, name) for name in ['NavigationSystemV1','EditorLevelLibrary','EditorLoadingAndSavingUtils','NavMeshBoundsVolume','AutomationLibrary','LevelEditorSubsystem','EditorActorSubsystem']}
out = Path(unreal.Paths.project_dir()) / 'Saved' / 'template-inspection.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2))
unreal.log('DOWNSTREAM_TEMPLATE_INSPECTION ' + json.dumps(report))

"""Build and save navigation in a full editor; commandlets lack editor ticks."""
import json
import time
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve().parents[1]
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
map_path = unreal.SystemLibrary.get_command_line().split('-DownstreamMap=')[-1].split(' ')[0] if '-DownstreamMap=' in unreal.SystemLibrary.get_command_line() else '/Game/Downstream/Maps/M0_TestWorld'
assert level.load_level(map_path)
state = {'time': time.monotonic(), 'handle': None}


def complete(passed, message, nav_count):
    report = {'passed': passed, 'message': message, 'nav_data_actors': nav_count, 'map': map_path}
    (root / 'build' / 'navigation-bake.json').write_text(json.dumps(report, indent=2))
    unreal.log('DOWNSTREAM_NAVIGATION_BAKED ' + json.dumps(report))
    unreal.unregister_slate_post_tick_callback(state['handle'])
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    if time.monotonic() - state['time'] < 2:
        return
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        navs = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.RecastNavMesh)]
        unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation')
        navs = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.RecastNavMesh)]
        if not navs:
            if time.monotonic() - state['time'] > 30:
                complete(False, 'No editor navigation data generated', 0)
            return
        for nav in navs:
            nav.set_editor_property('runtime_generation', unreal.RuntimeGenerationType.DYNAMIC)
        saved = level.save_current_level()
        unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
        complete(bool(saved), 'Navigation built and map saved' if saved else 'Map save failed', len(navs))
    except Exception as exc:
        complete(False, repr(exc), 0)


state['handle'] = unreal.register_slate_post_tick_callback(tick)

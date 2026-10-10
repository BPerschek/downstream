"""Capture review frames of the Drowned Vault (flooded or drained) in PIE with an isolated save."""
import re, time
from pathlib import Path
import unreal
root = Path(unreal.Paths.project_dir()).resolve().parents[1]
mode = re.search(r'-DownstreamCaptureMode=(\w+)', unreal.SystemLibrary.get_command_line()).group(1)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert level.load_level('/Game/Downstream/Maps/M1_Expedition')
state = {'stage': 'begin', 't': time.monotonic(), 'h': None}
def quit_():
    unreal.unregister_slate_post_tick_callback(state['h'])
    level.editor_request_end_play(); unreal.SystemLibrary.quit_editor()
def tick(dt):
    try:
        now = time.monotonic()
        if state['stage'] == 'begin':
            level.editor_request_begin_play(); state.update(stage='spawn', t=now); return
        world = editor.get_game_world()
        if not world: return
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not pawn: return
        water = unreal.DownstreamWaterSubsystem.get_for_world(world)
        pc = unreal.GameplayStatics.get_player_controller(world, 0)
        if state['stage'] == 'spawn' and now - state['t'] > 3:
            for e in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamEnemy):
                e.set_actor_tick_enabled(False)
            spots = {'home': unreal.Vector(-1550, -500, 120), 'ruin': unreal.Vector(1400, 650, 120)}
            if mode in spots:
                pawn.set_actor_location(spots[mode], False, True)
            else:
                cell = unreal.IntPoint(10, 13) if mode == 'flooded' else unreal.IntPoint(11, 6)
                pawn.set_actor_location(water.cell_to_world(cell) + unreal.Vector(0, 0, 120), False, True)
            if mode == 'planning':
                pawn.toggle_dig_planner()
            state.update(stage='settle', t=now)
        elif state['stage'] == 'settle' and now - state['t'] > 3:
            name = (root / 'build' / ('Downstream_Water_' + mode + '.png')).as_posix()
            unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1280x720 filename=' + name, pc)
            state.update(stage='shot', t=now)
        elif state['stage'] == 'shot' and now - state['t'] > 6:
            quit_()
    except Exception as exc:
        unreal.log_error('CAPTURE_FAILED ' + repr(exc)); quit_()
state['h'] = unreal.register_slate_post_tick_callback(tick)

"""Editor automation smoke test for pawn, navigation, movement and camera.

Launch in a separate hidden editor with -ExecCmds="py <script>"; the active editor
is never controlled or closed. Results are written under ignored build/.
"""
import json
import time
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve().parents[1]
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
level.load_level('/Game/Downstream/Maps/M0_TestWorld')
state = {'stage': 'begin', 'time': time.monotonic(), 'start': None, 'handle': None, 'report': {}}


def screenshot(world, controller):
    filename = str(root / 'build' / 'Downstream_Unreal_Preview.png').replace('\\', '/')
    unreal.SystemLibrary.execute_console_command(world, 'HighResShot 1280x720 filename=' + filename, controller)


def finish(passed, message):
    state['report'].update({'passed': passed, 'message': message})
    (root / 'build' / 'play-verification.json').write_text(json.dumps(state['report'], indent=2))
    unreal.log('DOWNSTREAM_PLAY_VERIFIED ' + json.dumps(state['report']))
    if state['handle'] is not None:
        unreal.unregister_slate_post_tick_callback(state['handle'])
    level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()


def tick(delta):
    try:
        if time.monotonic() - state['time'] > 90:
            finish(False, 'Timed out while verifying ' + state['stage'])
            return
        if state['stage'] == 'begin':
            level.editor_request_begin_play()
            state['stage'] = 'pawn'
            return
        world = editor.get_game_world()
        if not world:
            return
        controller = unreal.GameplayStatics.get_player_controller(world, 0)
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0) if controller else None
        if not pawn:
            return
        if state['stage'] == 'pawn':
            if time.monotonic() - state['time'] < 3:
                return
            start = pawn.get_actor_location()
            goal = unreal.Vector(start.x + 600, start.y, start.z)
            state['start'] = start
            state['goal'] = goal
            state['report']['pawn_class'] = pawn.get_class().get_name()
            player_mesh = pawn.get_component_by_class(unreal.SkeletalMeshComponent)
            if not player_mesh or not player_mesh.get_skeletal_mesh_asset():
                finish(False, 'Player spawned without a visible skeletal mesh')
                return
            state['report']['player_mesh'] = player_mesh.get_skeletal_mesh_asset().get_name()
            native = unreal.DownstreamSimulationSubsystem.get_for_world(world)
            if not native:
                finish(False, 'Native simulation subsystem missing in game world')
                return
            chunks = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamTerrainChunk)
            if len(chunks) != 1:
                finish(False, 'Expected one native terrain presentation actor')
                return
            state['native'] = native
            state['chunk'] = chunks[0]
            state['native_time_start'] = unreal.GameplayStatics.get_time_seconds(world)
            state['native_tick_start'] = native.get_simulation_tick()
            state['report']['native_grid'] = [native.get_grid_width(), native.get_grid_height()]
            state['report']['start'] = [start.x, start.y, start.z]
            state['report']['nav_data_count'] = len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh))
            state['report']['nav_mesh_settings'] = []
            for nav in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.RecastNavMesh):
                state['report']['nav_mesh_settings'].append({'runtime_generation': str(nav.get_editor_property('runtime_generation')), 'can_be_main_nav_data': nav.get_editor_property('can_be_main_nav_data')})
            # Match play.ps1's explicit startup command for this static prototype.
            state['report']['startup_navigation_command'] = 'RebuildNavigation'
            unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation', controller)
            unreal.AIHelperLibrary.simple_move_to_location(controller, goal)
            state['stage'] = 'moving'
            state['move_time'] = time.monotonic()
            return
        if state['stage'] == 'moving':
            path = unreal.AIHelperLibrary.get_current_path(controller)
            if path:
                state['report']['path_points'] = max(state['report'].get('path_points', 0), len(path.path_points))
            location = pawn.get_actor_location()
            travel = (location - state['start']).length()
            state['report']['travel_cm'] = travel
            if not path and time.monotonic() - state['move_time'] < 10:
                unreal.AIHelperLibrary.simple_move_to_location(controller, state['goal'])
            if travel > 450:
                camera = controller.player_camera_manager
                state['report']['travel_cm'] = travel
                state['report']['camera_rotation'] = str(camera.get_camera_rotation())
                native = state['native']
                chunk = state['chunk']
                before = native.get_terrain_height(16, 16)
                target = 8.0 if before < 7.0 else -8.0
                state['before_edit'] = before
                state['edit_target'] = target
                state['rebuild_before'] = chunk.get_rebuild_count()
                if state['rebuild_before'] != 1:
                    finish(False, 'Stable native patch rebuilt before any edit')
                    return
                assert native.queue_terrain_height(16, 16, target), 'Valid native edit rejected'
                assert not native.queue_terrain_height(-1, 16, target), 'Invalid native coordinate accepted'
                assert abs(native.get_terrain_height(16, 16) - before) < 0.001, 'Queue mutated height before fixed tick'
                state['stage'] = 'native_apply'
            elif time.monotonic() - state['move_time'] > 20:
                screenshot(world, controller)
                state['report']['movement_failed'] = True
                state['stage'] = 'screenshot'
                state['shot_time'] = time.monotonic()
        elif state['stage'] == 'native_apply':
            native, chunk = state['native'], state['chunk']
            if abs(native.get_terrain_height(16, 16) - state['edit_target']) < 0.001 and chunk.get_rebuild_count() > state['rebuild_before']:
                assert chunk.get_rebuild_count() == state['rebuild_before'] + 1, 'One edit caused multiple mesh rebuilds'
                state['idle_rebuilds'] = chunk.get_rebuild_count()
                state['idle_start'] = unreal.GameplayStatics.get_time_seconds(world)
                state['stage'] = 'native_idle'
        elif state['stage'] == 'native_idle':
            now = unreal.GameplayStatics.get_time_seconds(world)
            if now - state['idle_start'] >= 0.7:
                native, chunk = state['native'], state['chunk']
                assert chunk.get_rebuild_count() == state['idle_rebuilds'], 'Idle simulation rebuilt terrain'
                ticks = native.get_simulation_tick() - state['native_tick_start']
                seconds = now - state['native_time_start']
                assert abs(ticks - seconds * 10.0) <= 1.5, 'Native clock did not run at configured 10Hz'
                state['report']['native_tick_delta'] = ticks
                state['report']['native_game_seconds'] = seconds
                state['report']['native_hz_measured'] = ticks / seconds
                state['report']['deferred_edit_passed'] = True
                state['report']['idle_rebuilds'] = chunk.get_rebuild_count()
                chunk.lower_central_region()
                state['lower_target'] = state['edit_target'] - 2.0
                state['stage'] = 'native_lower'
        elif state['stage'] == 'native_lower':
            native, chunk = state['native'], state['chunk']
            if abs(native.get_terrain_height(16, 16) - state['lower_target']) < 0.001 and chunk.get_rebuild_count() > state['idle_rebuilds']:
                assert chunk.get_rebuild_count() == state['idle_rebuilds'] + 1, 'Area edit rebuilt more than once'
                state['report']['area_edit_passed'] = True
                state['report']['final_rebuilds'] = chunk.get_rebuild_count()
                state['report']['native_hud'] = controller.get_hud().get_class().get_name()
                assert state['report']['native_hud'] == 'DownstreamDebugHUD', 'Debug HUD not installed'
                chunk.toggle_debug_view()
                state['stage'] = 'native_view'
                state['view_time'] = time.monotonic()
        elif state['stage'] == 'native_view' and time.monotonic() - state['view_time'] > 1:
            state['report']['debug_view_position'] = str(pawn.get_actor_location())
            camera = controller.player_camera_manager
            rotation = camera.get_camera_rotation()
            assert abs(rotation.pitch + 55) < 0.1 and abs(rotation.yaw - 45) < 0.1, 'Debug view changed fixed camera angle'
            screenshot(world, controller)
            state['stage'] = 'screenshot'
            state['shot_time'] = time.monotonic()
        elif state['stage'] == 'screenshot' and time.monotonic() - state['shot_time'] > 6:
            state['report']['screenshot_exists'] = (root / 'build' / 'Downstream_Unreal_Preview.png').exists()
            finish(not state['report'].get('movement_failed', False), 'Pawn did not move' if state['report'].get('movement_failed') else 'Movement, native fixed tick, queued edits, dirty mesh and HUD passed')
    except Exception as exc:
        finish(False, repr(exc))


state['handle'] = unreal.register_slate_post_tick_callback(tick)

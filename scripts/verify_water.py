"""Isolated real-world Drowned Vault acceptance: flood, wading, digger, canal, drain, vault, reload."""
import json
import re
import time
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve().parents[1]
command = unreal.SystemLibrary.get_command_line()
mode = re.search(r'-DownstreamWaterMode=(\w+)', command).group(1)
save_match = re.search(r'-DownstreamSaveDir=(?:"([^"]+)"|(\S+))', command)
save_dir = Path(save_match.group(1) or save_match.group(2))
save_file = save_dir / 'prototype-save.json'
initial_bytes = save_file.read_bytes()
initial = json.loads(initial_bytes)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert level.load_level('/Game/Downstream/Maps/M1_Expedition')
state = {'stage': 'begin', 'start': time.monotonic(), 'stage_time': time.monotonic(), 'report': {'mode': mode}, 'handle': None}
VAULT = unreal.IntPoint(15, 9)
CULVERT = unreal.IntPoint(7, 0)
TIMEOUT = 280


def stage(name):
    state['stage'] = name
    state['stage_time'] = time.monotonic()
    unreal.log('DOWNSTREAM_WATER_STAGE ' + mode + ' ' + name)


def finish(passed, message):
    state['report'].update(passed=passed, message=message, stage=state['stage'])
    (root / 'build' / ('water-' + mode + '-verification.json')).write_text(json.dumps(state['report'], indent=2))
    unreal.log('DOWNSTREAM_WATER_VERIFIED ' + json.dumps(state['report']))
    unreal.unregister_slate_post_tick_callback(state['handle'])
    level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()


def place(actor, location):
    actor.set_actor_location(location, False, True)
    movement = actor.get_component_by_class(unreal.CharacterMovementComponent)
    if movement:
        movement.stop_movement_immediately()


def tick(delta):
    try:
        assert time.monotonic() - state['start'] < TIMEOUT, 'Timeout in ' + state['stage']
        if state['stage'] == 'begin':
            level.editor_request_begin_play()
            stage('spawn')
            return
        world = editor.get_game_world()
        if not world:
            return
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not pawn:
            return
        economy = unreal.DownstreamExpeditionSubsystem.get_for_world(world)
        water = unreal.DownstreamWaterSubsystem.get_for_world(world)
        manager = unreal.DownstreamSettlementManager.get_for_world(world)
        elapsed = time.monotonic() - state['stage_time']
        home = unreal.Vector(-1450, -850, 110)
        if state['stage'] == 'spawn':
            if elapsed < 3:
                return
            assert economy and water and manager, 'World adapters absent'
            assert water.is_site_active(), 'Water site did not activate'
            sites = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamWaterSite)
            assert len(sites) == 1, 'Expected one water site actor'
            assert sites[0].get_terrain_rebuild_count() >= 1 and sites[0].get_water_rebuild_count() >= 1, 'Site meshes not built'
            assert not unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamTerrainChunk), 'M0 debug patch still in M1'
            for enemy in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamEnemy):
                enemy.set_actor_tick_enabled(False)
                enemy.get_controller().stop_movement()
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            unreal.SystemLibrary.execute_console_command(world, 'RebuildNavigation', pc)
            state['report'].update(vault_depth_start=water.get_vault_depth(), water_start=water.get_total_water())
            if mode == 'legacy':
                assert initial['version'] == 1 and economy.get_banked_salvage() == 40, 'Legacy save not loaded'
                assert not economy.is_digger_built() and not economy.is_vault_opened()
                assert water.get_vault_depth() > 1.0, 'Fresh site not flooded'
                assert not pawn.plan_canal(VAULT, CULVERT), 'Canal accepted without a digger'
                assert water.get_queued_canal_count() == 0
                assert save_file.read_bytes() == initial_bytes, 'Rejected canal rewrote save'
                finish(True, 'Schema1 loads into an untouched flooded site; planning requires a digger')
                return
            if mode == 'respawn':
                gm = unreal.GameplayStatics.get_game_mode(world)
                assert gm.is_field_respawn_enabled(), 'Field respawn disabled'
                assert gm.get_tracked_enemy_count() == 9 and gm.get_tracked_cache_count() == 3, 'Field not registered (%d/%d)' % (gm.get_tracked_enemy_count(), gm.get_tracked_cache_count())
                tuning = unreal.load_asset('/Game/Downstream/Data/DA_PrototypeTuning')
                tuning.set_editor_property('enemy_respawn_seconds', 2.0)
                tuning.set_editor_property('cache_respawn_seconds', 2.0)
                # The farthest enemy from home, so it is outside the no-respawn radius around the player.
                victim = max(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamEnemy), key=lambda e: (e.get_actor_location() - home).length())
                victim.receive_combat_damage(5000, pawn)
                assert victim.is_dead(), 'Enemy did not die'
                cache = [p for p in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamSalvagePickup) if p.get_editor_property('amount') == 4][0]
                place(pawn, cache.get_actor_location() + unreal.Vector(0, 0, 60))
                assert economy.try_interact(pawn), 'Cache collection failed'
                place(pawn, home)
                state['respawns_before'] = gm.get_field_respawn_count()
                stage('respawn_wait')
                return
            if mode == 'reload':
                assert initial['version'] == 3 and initial['earthworks']['vault_opened'], 'Reload fixture lacks earthworks'
                assert economy.is_digger_built() and economy.is_vault_opened(), 'Earthworks flags not restored'
                assert economy.get_saved_terrain_delta_count() > 0, 'Dug terrain not restored'
                assert water.is_vault_accessible(), 'Restored canal did not keep the vault drained (depth %.2f)' % water.get_vault_depth()
                state['report'].update(restored_deltas=economy.get_saved_terrain_delta_count(), vault_depth=water.get_vault_depth())
                stage('reload_digger')
                return
            assert water.get_vault_depth() > 1.0, 'Vault should start flooded, depth %.2f' % water.get_vault_depth()
            # Wading: stand on the basin slope.
            wade_cell = unreal.IntPoint(11, 9)
            place(pawn, water.cell_to_world(wade_cell) + unreal.Vector(0, 0, 100))
            state['wade_depth'] = water.get_cell_water(wade_cell)
            stage('wading')
        elif state['stage'] == 'wading':
            if elapsed < 1.5:
                return
            movement = pawn.get_component_by_class(unreal.CharacterMovementComponent)
            depth = pawn.get_water_depth()
            assert depth > 0.2, 'Player not reported as wading (depth %.2f)' % depth
            assert movement.get_editor_property('max_walk_speed') < 500, 'Wading did not slow the player'
            state['report'].update(wade_depth=depth, wade_speed=movement.get_editor_property('max_walk_speed'))
            place(pawn, home)
            stage('build')
        elif state['stage'] == 'build':
            if elapsed < 1:
                return
            assert not pawn.plan_canal(VAULT, CULVERT), 'Canal accepted before digger'
            bank = economy.get_banked_salvage()
            pawn.toggle_build_menu()
            assert pawn.is_build_menu_open(), 'Builder did not open at home'
            pawn.construct_digger()
            assert economy.is_digger_built(), 'Digger not assembled'
            assert economy.get_banked_salvage() == bank - economy.get_digger_cost(), 'Digger cost not spent'
            assert not economy.try_construct_digger(pawn), 'Second digger accepted'
            pawn.toggle_build_menu()
            saved = json.loads(save_file.read_text())
            assert saved['version'] == 3 and saved['earthworks']['digger_built'], 'Digger not saved'
            # Test pacing only: faster dig steps. Every step still goes through the same subsystem path.
            tuning = unreal.load_asset('/Game/Downstream/Data/DA_PrototypeTuning')
            tuning.set_editor_property('dig_action_seconds', 0.12)
            state['report'].update(digger_cost=economy.get_digger_cost(), bank_after_digger=economy.get_banked_salvage(), test_dig_action_seconds=0.12)
            stage('digger_spawn')
        elif state['stage'] == 'digger_spawn':
            digger = manager.get_digger()
            if not digger:
                assert elapsed < 10, 'Digger did not spawn'
                return
            state['digger'] = digger
            assert abs(water.get_canal_depth() - 1.0) < 1e-4, 'Default canal depth should be 1 m'
            pawn.toggle_dig_planner()
            assert pawn.is_dig_planner_open(), 'Planner did not open with a digger'
            pawn.deepen_canals()
            assert abs(water.get_canal_depth() - 1.25) < 1e-4, 'Shift+wheel up did not deepen'
            pawn.shallow_canals()
            assert abs(water.get_canal_depth() - 1.0) < 1e-4, 'Shift+wheel down did not restore depth'
            pawn.toggle_dig_planner()
            assert not pawn.plan_canal(unreal.IntPoint(-3, 0), CULVERT), 'Off-site canal accepted'
            assert pawn.plan_canal(VAULT, CULVERT), 'Valid canal rejected'
            assert water.get_queued_canal_count() == 1
            state['report']['canal_cells'] = len(water.preview_canal(VAULT, CULVERT))
            stage('digging')
        elif state['stage'] == 'digging':
            digger = state['digger']
            if water.get_completed_canal_count() >= 1:
                assert digger.get_dig_actions() > 0, 'Canal completed without the digger working'
                state['report'].update(dig_actions=digger.get_dig_actions(), dig_seconds=round(elapsed, 1),
                                       digger_used_direct_steering=digger.is_using_direct_steering(), digger_hops=digger.get_hop_count())
                stage('draining')
            elif elapsed > 170:
                raise AssertionError('Digging stalled: %s at %s, progress %.2f, hops %d' % (digger.get_status(), digger.get_actor_location(), water.get_active_canal_progress(), digger.get_hop_count()))
        elif state['stage'] == 'draining':
            water.simulate_seconds(150)
            assert water.is_vault_accessible(), 'Canal did not drain vault (depth %.2f)' % water.get_vault_depth()
            assert water.get_drained_volume() > 10, 'Culvert drained too little'
            state['report'].update(vault_depth_after=water.get_vault_depth(), drained_m3=water.get_drained_volume())
            place(pawn, water.get_vault_location() + unreal.Vector(-150, 0, 120))
            stage('open')
        elif state['stage'] == 'open':
            if elapsed < 1.5:
                return
            before = len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamSalvagePickup))
            pawn.interact()
            assert economy.is_vault_opened(), 'Vault did not open: ' + str(economy.get_status_message())
            pickups = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamSalvagePickup)
            near = [p for p in pickups if (p.get_actor_location() - water.get_vault_location()).length() < 400]
            reward = sum(p.get_editor_property('amount') for p in near)
            assert len(pickups) > before and reward >= 16, 'Vault reward missing (%d)' % reward
            pawn.interact()
            assert len(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamSalvagePickup)) <= len(pickups), 'Vault paid twice'
            assert water.save_terrain_now(), 'Terrain save failed'
            saved = json.loads(save_file.read_text())
            assert saved['earthworks']['vault_opened'] and len(saved['earthworks']['terrain_mm']) > 0, 'Earthworks not saved'
            state['report'].update(vault_reward=reward, saved_terrain_cells=len(saved['earthworks']['terrain_mm']))
            finish(True, 'Flooded vault, wading slowdown, digger assembly, real robot canal, drain, vault reward and v3 save passed')
        elif state['stage'] == 'respawn_wait':
            gm = unreal.GameplayStatics.get_game_mode(world)
            living = [e for e in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamEnemy) if not e.is_dead()]
            caches = [p for p in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamSalvagePickup) if p.get_editor_property('amount') == 4 and not p.get_editor_property('recovery_bag')]
            if gm.get_field_respawn_count() - state['respawns_before'] >= 2 and len(living) == 9 and len(caches) >= 3:
                victim = living[0]
                victim.receive_combat_damage(5000, pawn)
                before = gm.get_field_respawn_count()
                spawned = gm.respawn_field_now()
                assert spawned >= 1 and gm.get_field_respawn_count() > before, 'F10 respawn did not respawn the fresh kill'
                state['report'].update(timed_respawns=2, manual_respawns=spawned, living_enemies=9, caches=len(caches))
                finish(True, 'Killed enemy and looted cache respawned away from the player; manual field respawn works')
            elif elapsed > 20:
                raise AssertionError('Respawn stalled: respawns=%d living=%d caches=%d' % (gm.get_field_respawn_count(), len(living), len(caches)))
        elif state['stage'] == 'reload_digger':
            if manager.get_digger():
                finish(True, 'Schema3 earthworks restored: digger, opened vault and drained canal persist')
            elif elapsed > 10:
                raise AssertionError('Saved digger did not respawn')
    except Exception as exc:
        finish(False, repr(exc))


state['handle'] = unreal.register_slate_post_tick_callback(tick)

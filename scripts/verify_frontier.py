"""Isolated frontier acceptance in PIE (scripts/verify_frontier.ps1).

Modes:
  frontier  M2_World seed N from scratch: generation, population, workshop + digger, a real
            robot canal through an outpost levee, flood damage + salvage, engineers sandbag,
            guards hunt the digger, digger wreck + rebuild, v4 save.
  reload    Same save dir: seed, razed structures and dug terrain come back.
  menu      MainMenu: preview world builds, no pawn, menu controller active.
"""
import json
import re
import time
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve().parents[1]
command = unreal.SystemLibrary.get_command_line()
mode = re.search(r'-DownstreamFrontierMode=(\w+)', command).group(1)
save_match = re.search(r'-DownstreamSaveDir=(?:"([^"]+)"|(\S+))', command)
save_dir = Path(save_match.group(1) or save_match.group(2))
save_file = save_dir / 'world-save.json'
seed_match = re.search(r'-DownstreamWorldSeed=(\d+)', command)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert level.load_level('/Game/Downstream/Maps/MainMenu' if mode == 'menu' else '/Game/Downstream/Maps/M2_World')
state = {'stage': 'begin', 'start': time.monotonic(), 'stage_time': time.monotonic(), 'report': {'mode': mode}, 'handle': None}
TIMEOUT = 400
tuning = unreal.load_asset('/Game/Downstream/Data/DA_PrototypeTuning')
restore = {}


def tune(prop, value):
    restore.setdefault(prop, tuning.get_editor_property(prop))
    tuning.set_editor_property(prop, value)


def stage(name):
    state['stage'] = name
    state['stage_time'] = time.monotonic()
    unreal.log('DOWNSTREAM_FRONTIER_STAGE ' + mode + ' ' + name)


def finish(passed, message):
    for prop, value in restore.items():
        tuning.set_editor_property(prop, value)
    state['report'].update(passed=passed, message=message, stage=state['stage'])
    (root / 'build' / ('frontier-' + mode + '-verification.json')).write_text(json.dumps(state['report'], indent=2))
    unreal.log('DOWNSTREAM_FRONTIER_VERIFIED ' + json.dumps(state['report']))
    unreal.unregister_slate_post_tick_callback(state['handle'])
    level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()


def place(actor, location):
    actor.set_actor_location(location, False, True)
    movement = actor.get_component_by_class(unreal.CharacterMovementComponent)
    if movement:
        movement.stop_movement_immediately()


def freeze(enemies, frozen=True):
    for enemy in enemies:
        if enemy.is_dead():
            continue
        enemy.set_actor_tick_enabled(not frozen)
        controller = enemy.get_controller()
        if controller and frozen:
            controller.stop_movement()


def raiders_of(world, outpost):
    return [r for r in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamRaider) if r.get_editor_property('outpost_index') == outpost]


def destroyed(water, outpost):
    return sum(1 for s in range(water.get_outpost_structure_count(outpost)) if water.is_outpost_structure_destroyed(outpost, s))


def plan_outpost_canal(pawn, water, home, skip=()):
    """Plan a digger canal from an outpost's river through its levee onto the pad."""
    order = sorted((o for o in range(water.get_outpost_count()) if o not in skip), key=lambda o: (water.get_outpost_center(o) - home).length())
    for o in order:
        river = water.get_outpost_river_cell(o)
        c = water.get_outpost_center_cell(o)
        # End on the pad two cells short of the generator, like a player aiming at the bowl.
        dx, dy = river.x - c.x, river.y - c.y
        length = max(1.0, (dx * dx + dy * dy) ** 0.5)
        end = unreal.IntPoint(int(round(c.x + 2 * dx / length)), int(round(c.y + 2 * dy / length)))
        if pawn.plan_canal(river, end):
            return o, river, end
    return None, None, None


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
        water = unreal.DownstreamWaterSubsystem.get_for_world(world)
        elapsed = time.monotonic() - state['stage_time']
        r = state['report']
        if mode == 'menu':
            if elapsed < 5:
                return
            assert water and water.is_site_active() and water.is_preview_site(), 'Preview world not active'
            sites = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamWaterSite)
            assert len(sites) == 1 and sites[0].get_chunk_count() > 1, 'Preview terrain not chunked'
            pc = unreal.GameplayStatics.get_player_controller(world, 0)
            assert isinstance(pc, unreal.DownstreamMenuController), 'Menu controller missing: %s' % pc
            assert not unreal.GameplayStatics.get_player_pawn(world, 0), 'Menu spawned a pawn'
            r.update(chunks=sites[0].get_chunk_count(), outposts=water.get_outpost_count())
            finish(True, 'Main menu preview world and menu controller')
            return
        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
        if not pawn:
            return
        economy = unreal.DownstreamExpeditionSubsystem.get_for_world(world)
        manager = unreal.DownstreamSettlementManager.get_for_world(world)
        home = economy.get_home_location()

        if state['stage'] == 'spawn':
            if elapsed < 5:
                return
            assert water.is_world_site() and economy.is_world_mode(), 'Not in world mode'
            seed = economy.get_world_seed()
            if seed_match:
                assert seed == int(seed_match.group(1)), 'Seed %d != requested' % seed
            sites = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamWaterSite)
            assert len(sites) == 1 and sites[0].get_chunk_count() >= 30, 'World terrain not chunked'
            directors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamWorldDirector)
            assert len(directors) == 1 and directors[0].is_populated(), 'Director did not populate'
            outposts = water.get_outpost_count()
            assert outposts >= 1, 'No outposts'
            assert len(directors[0].get_outposts()) == outposts, 'Outpost actors missing'
            assert water.get_vault_count() >= 1, 'No vault'
            assert (pawn.get_actor_location() - home).length() < economy.get_home_radius() + 300, 'Player not at home'
            assert water.get_water_depth_at(home) < 0.05, 'Home is wet'
            raiders = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamRaider)
            gm = unreal.GameplayStatics.get_game_mode(world)
            r.update(seed=seed, chunks=sites[0].get_chunk_count(), outposts=outposts, raiders=len(raiders),
                     vaults=water.get_vault_count(), patrols=gm.get_tracked_enemy_count(), caches=gm.get_tracked_cache_count(),
                     props=directors[0].get_spawned_props(), nav_triangles=sites[0].get_navigation_triangle_count())
            if mode == 'reload':
                saved = json.loads(save_file.read_text())
                earth = saved['earthworks']
                assert saved['version'] == 4 and earth['seed'] == seed, 'Save seed mismatch'
                assert economy.get_saved_terrain_delta_count() > 0, 'Dug terrain not restored'
                masks = {o[0]: o[1] for o in earth.get('outposts', [])}
                razed_any = False
                for o in range(outposts):
                    mask = masks.get(water.get_outpost_id(o), 0)
                    for s in range(water.get_outpost_structure_count(o)):
                        if mask & (1 << s):
                            assert water.is_outpost_structure_destroyed(o, s), 'Structure %d/%d not restored as destroyed' % (o, s)
                            razed_any = True
                assert razed_any, 'Save has no destroyed structures'
                assert manager.get_digger(), 'Digger not restored'
                finish(True, 'Frontier v4 save restored seed, terrain, destroyed structures and the digger')
                return
            assert len(raiders) >= 4 * outposts, 'Garrisons incomplete (%d)' % len(raiders)
            freeze(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamEnemy))
            assert economy.dev_grant_salvage(200)
            workshop = economy.get_build_cell_world_location(unreal.IntPoint(1, 1))
            assert economy.try_place_structure(unreal.DownstreamStructureType.WORKSHOP, workshop, pawn), 'Workshop rejected: ' + str(economy.get_status_message())
            place(pawn, home + unreal.Vector(0, 0, 30))
            stage('digger')
        elif state['stage'] == 'digger':
            if elapsed < 1:
                return
            assert economy.try_construct_digger(pawn), 'Digger rejected: ' + str(economy.get_status_message())
            tune('dig_action_seconds', 0.12)
            tune('digger_health', 5000.0)
            stage('digger_spawn')
        elif state['stage'] == 'digger_spawn':
            digger = manager.get_digger()
            if not digger:
                assert elapsed < 10, 'Digger did not spawn'
                return
            state['digger'] = digger
            o, river, end = plan_outpost_canal(pawn, water, home)
            assert o is not None, 'No outpost levee canal was valid'
            state['outpost'] = o
            r.update(outpost=o, canal_cells=len(water.preview_canal(river, end)), river=[river.x, river.y], canal_end=[end.x, end.y])
            stage('digging')
        elif state['stage'] == 'digging':
            digger = state['digger']
            if water.get_completed_canal_count() >= 1:
                r.update(dig_actions=digger.get_dig_actions(), dig_seconds=round(elapsed, 1))
                water.simulate_seconds(4)
                stage('engineers')
            elif elapsed > 200:
                raise AssertionError('Digging stalled: %s progress %.2f' % (digger.get_status(), water.get_active_canal_progress()))
        elif state['stage'] == 'dig2':
            if water.get_completed_canal_count() > state['canals_before']:
                o2 = state['flood_outpost']
                r['structures_before'] = destroyed(water, o2)
                state['pickups_before'] = {p.get_name() for p in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamSalvagePickup)}
                stage('flood')
            elif elapsed > 150:
                raise AssertionError('Second canal stalled: %s' % state['digger'].get_status())
        elif state['stage'] == 'flood':
            o = state['flood_outpost']
            peak = 0.0
            for _ in range(30):
                water.simulate_seconds(10)
                peak = max(peak, water.get_outpost_flood_depth(o))
                if destroyed(water, o) - r['structures_before'] >= 2:
                    break
            fallen = destroyed(water, o) - r['structures_before']
            r.update(flood_peak=round(peak, 2), structures_fallen=fallen)
            assert peak > 0.2, 'Outpost did not flood (peak %.2f)' % peak
            assert fallen >= 1, 'Flood destroyed nothing'
            pickups = [p for p in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamSalvagePickup)
                       if p.get_name() not in state.get('pickups_before', set()) and (p.get_actor_location() - water.get_outpost_center(o)).length() < water.get_outpost_radius_cm(o) + 600]
            assert pickups, 'Collapsed structures dropped no salvage'
            r.update(flood_peak=round(peak, 2), structures_fallen=fallen, salvage_drops=len(pickups),
                     drop_amount=sum(p.get_editor_property('amount') for p in pickups), razed=water.is_outpost_razed(o))
            stage('guards')
        elif state['stage'] == 'engineers':
            o = state['outpost']
            engineers = [e for e in raiders_of(world, o) if e.get_editor_property('raider_role') == unreal.DownstreamRaiderRole.ENGINEER and not e.is_dead()]
            if elapsed < 0.5:
                breach = water.get_outpost_breach(o)
                has_breach = breach is not None and (bool(breach[0]) if isinstance(breach, tuple) else True)
                assert engineers, 'No living engineers at the breached outpost'
                freeze(engineers, False)
                state['had_breach'] = has_breach
                return
            actions = sum(e.get_dam_actions() for e in engineers)
            if actions >= 2:
                r.update(engineer_dam_actions=actions, engineers=len(engineers))
                freeze(engineers)
                # Engineers sealed this levee; flood a second outpost whose garrison stays frozen.
                o2, river, end = plan_outpost_canal(pawn, water, home, skip=(o,))
                assert o2 is not None, 'No second outpost canal was valid'
                state['flood_outpost'] = o2
                state['canals_before'] = water.get_completed_canal_count()
                r.update(flood_outpost=o2, flood_canal_cells=len(water.preview_canal(river, end)))
                stage('dig2')
            elif not state.get('had_breach'):
                raise AssertionError('Canal through the levee left no breach')
            elif elapsed > 45:
                breach = water.get_outpost_breach(o)
                try:
                    diag = [{'loc': str(e.get_actor_location()), 'vel': str(e.get_velocity()), 'controller': str(e.get_controller()),
                             'label': str(e.get_editor_property('role_label').get_editor_property('text')), 'damming': e.is_damming(),
                             'water': e.get_water_depth(), 'mode': str(e.get_component_by_class(unreal.CharacterMovementComponent).get_editor_property('movement_mode')),
                             'move_active': e.get_component_by_class(unreal.CharacterMovementComponent).is_active(), 'tick': e.is_actor_tick_enabled(),
                             'max_speed': e.get_component_by_class(unreal.CharacterMovementComponent).get_editor_property('max_walk_speed'),
                             'overlaps': [a.get_name() for a in e.get_overlapping_actors(unreal.Actor)],
                             'move_tick': e.get_component_by_class(unreal.CharacterMovementComponent).is_component_tick_enabled()} for e in engineers]
                    guards_now = [{'name': g.get_name(), 'loc': str(g.get_actor_location()), 'mode': str(g.get_component_by_class(unreal.CharacterMovementComponent).get_editor_property('movement_mode'))} for g in raiders_of(world, o)]
                    r['raiders_diag'] = guards_now
                    probes = []
                    for e in engineers:
                        before = e.get_actor_location()
                        hit = e.add_actor_world_offset(unreal.Vector(60, 0, 0), True, False)
                        info = hit.to_tuple() if hasattr(hit, 'to_tuple') else hit
                        probes.append({'moved': (e.get_actor_location() - before).length(), 'hit': str(info)[:600]})
                    r['sweep_probe'] = probes
                except Exception as diag_error:
                    diag = repr(diag_error)

                r.update(engineer_diag=diag, breach=str(breach), flood_now=water.get_outpost_flood_depth(o))
                raise AssertionError('Engineers never sandbagged (%d alive)' % len(engineers))
        elif state['stage'] == 'guards':
            o = state['outpost']
            digger = state['digger']
            guards = [g for g in raiders_of(world, o) if g.get_editor_property('raider_role') != unreal.DownstreamRaiderRole.ENGINEER and not g.is_dead()]
            if elapsed < 0.5:
                if not guards:
                    r['guards'] = 'none alive'
                    stage('wreck')
                    return
                place(pawn, home + unreal.Vector(0, 0, 30))
                place(digger, water.get_outpost_center(o) + unreal.Vector(water.get_outpost_radius_cm(o) + 300, 0, 150))
                freeze(guards, False)
                return
            hunting = [g for g in guards if g.get_current_target() == digger]
            if hunting and 'guard_start' not in state:
                state['guard_start'] = {g.get_name(): g.get_actor_location() for g in hunting}
                state['guard_time'] = elapsed
            if 'guard_start' in state and elapsed - state['guard_time'] > 4:
                moved = max((g.get_actor_location() - state['guard_start'][g.get_name()]).length() for g in hunting if g.get_name() in state['guard_start']) if hunting else 0
                r.update(guards_hunting_digger=len(hunting), guard_moved_cm=round(moved))
                if moved <= 100:
                    diag = []
                    for g in hunting:
                        entry = {'name': g.get_name(), 'kind': str(g.get_editor_property('kind'))}
                        try:
                            c = g.get_controller()
                            entry['status'] = str(c.get_move_status())
                            entry['partial'] = c.has_partial_path()
                            path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, g.get_actor_location(), digger.get_actor_location())
                            entry['path_points'] = len(path.path_points) if path else -1
                            entry['path_partial'] = path.is_partial() if path else None
                            entry['dist'] = (g.get_actor_location() - digger.get_actor_location()).length()
                            mv = g.get_component_by_class(unreal.CharacterMovementComponent)
                            entry['accel'] = str(mv.get_current_acceleration())
                            before = g.get_actor_location()
                            g.launch_character(unreal.Vector(400, 0, 300), True, True)
                            entry['launch_from'] = str(before)
                        except Exception as diag_error:
                            entry['error'] = repr(diag_error)
                        diag.append(entry)
                    r['guard_diag'] = diag
                    state['launched'] = [g for g in hunting]
                    state['launch_from'] = {g.get_name(): g.get_actor_location() for g in hunting}
                    state['launch_time'] = elapsed
                    stage('guard_launch')
                    return
                stage('wreck')
            elif elapsed > 14 and 'guard_start' not in state:
                raise AssertionError('No guard targeted the digger (targets %s)' % [str(g.get_current_target()) for g in guards])
        elif state['stage'] == 'guard_launch':
            if elapsed < 1.5:
                return
            r['launch_moved'] = [round((g.get_actor_location() - state['launch_from'][g.get_name()]).length()) for g in state['launched']]
            raise AssertionError('Guards targeted the digger but did not move')
        elif state['stage'] == 'wreck':
            digger = state['digger']
            if elapsed < 0.5:
                freeze(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamEnemy))
                tune('digger_health', 150.0)
                tune('digger_rebuild_seconds', 2.0)
                economy.dev_grant_salvage(20)
                state['bank_before_rebuild'] = economy.get_banked_salvage()
                unreal.GameplayStatics.apply_damage(digger, 100000.0, None, pawn, unreal.DamageType.static_class())
                assert digger.is_wrecked(), 'Digger did not wreck'
                return
            if not digger.is_wrecked():
                assert digger.get_times_wrecked() == 1
                spent = state['bank_before_rebuild'] - economy.get_banked_salvage()
                assert spent == tuning.get_editor_property('digger_rebuild_cost'), 'Rebuild cost %d' % spent
                r.update(rebuild_cost=spent, rebuild_seconds=round(elapsed, 1))
                stage('save')
            elif elapsed > 15:
                raise AssertionError('Digger never rebuilt: ' + str(digger.get_status()))
        elif state['stage'] == 'save':
            assert water.save_terrain_now(), 'Terrain save failed'
            saved = json.loads(save_file.read_text())
            earth = saved['earthworks']
            assert saved['version'] == 4 and earth['seed'] == economy.get_world_seed(), 'v4 seed not saved'
            assert earth.get('outposts'), 'Outpost masks not saved'
            assert len(earth['terrain_mm']) > 0, 'Terrain not saved'
            r.update(saved_terrain_cells=len(earth['terrain_mm']), saved_outposts=earth['outposts'])
            finish(True, 'Frontier: generation, garrisons, digger canal through a levee, flood collapse + salvage, engineers, guards vs digger, wreck/rebuild, v4 save')
    except Exception as exc:
        finish(False, repr(exc))


state['handle'] = unreal.register_slate_post_tick_callback(tick)

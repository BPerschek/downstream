"""Isolated real-world settlement, migration, robot and write-failure acceptance."""
import json
import re
import time
from pathlib import Path
import unreal

root=Path(unreal.Paths.project_dir()).resolve().parents[1]
command=unreal.SystemLibrary.get_command_line()
mode=re.search(r'-DownstreamSettlementMode=(\w+)',command).group(1)
save_match=re.search(r'-DownstreamSaveDir=(?:"([^"]+)"|(\S+))',command)
save_dir=Path(save_match.group(1) or save_match.group(2))
save_file=save_dir/'prototype-save.json'
initial_bytes=save_file.read_bytes()
initial=json.loads(initial_bytes)
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert level.load_level('/Game/Downstream/Maps/M1_Expedition')
state={'stage':'begin','start':time.monotonic(),'report':{'mode':mode},'handle':None}

def stage(name):
    state['stage']=name
    state['stage_time']=time.monotonic()
    unreal.log('DOWNSTREAM_SETTLEMENT_STAGE '+mode+' '+name)

def finish(passed,message):
    state['report'].update(passed=passed,message=message,stage=state['stage'])
    (root/'build'/('settlement-'+mode+'-verification.json')).write_text(json.dumps(state['report'],indent=2))
    unreal.log('DOWNSTREAM_SETTLEMENT_VERIFIED '+json.dumps(state['report']))
    unreal.unregister_slate_post_tick_callback(state['handle'])
    level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()

def place(actor,location):
    actor.set_actor_location(location,False,False)
    movement=actor.get_component_by_class(unreal.CharacterMovementComponent)
    if movement: movement.stop_movement_immediately()

def tick(delta):
    try:
        assert time.monotonic()-state['start']<105,'Timeout in '+state['stage']
        if state['stage']=='begin':
            level.editor_request_begin_play()
            stage('spawn')
            return
        world=editor.get_game_world()
        if not world: return
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        if not pawn: return
        economy=unreal.DownstreamExpeditionSubsystem.get_for_world(world)
        manager=unreal.DownstreamSettlementManager.get_for_world(world)
        elapsed=time.monotonic()-state['stage_time']
        if state['stage']=='spawn':
            if elapsed<3: return
            assert economy and manager,'Settlement world adapters absent'
            for enemy in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DownstreamEnemy):
                enemy.set_actor_tick_enabled(False)
                enemy.get_controller().stop_movement()
            pc=unreal.GameplayStatics.get_player_controller(world,0)
            unreal.SystemLibrary.execute_console_command(world,'RebuildNavigation',pc)
            if mode=='invalid':
                assert economy.get_banked_salvage()==0,'Invalid numeric type was coerced into a bank'
                assert not economy.choose_starter_skill('steady_aim'),'Invalid save permitted writes'
                item=economy.spawn_salvage(pawn.get_actor_location(),2,False)
                assert item and economy.try_interact(pawn),'Safe gameplay collection blocked'
                assert not economy.try_interact(pawn),'Invalid save unexpectedly banked carry'
                assert economy.get_carried_salvage()==2,'Blocked save erased carried salvage'
                assert save_file.read_bytes()==initial_bytes,'Invalid save was overwritten'
                finish(True,'Malformed numeric JSON preserved; banking and skill writes blocked; carry retained')
                return
            if mode=='unfunded':
                assert economy.get_banked_salvage()==0
                assert not economy.try_place_structure(unreal.DownstreamStructureType.STORAGE,unreal.Vector(-900,-550,10),pawn)
                assert not economy.try_construct_hauler(pawn)
                assert len(economy.get_built_structures())==0 and economy.get_banked_salvage()==0
                assert save_file.read_bytes()==initial_bytes,'Rejected purchase unexpectedly rewrote save'
                finish(True,'Unaffordable placement and robot prerequisites reject without spending')
                return
            if mode=='write_failure':
                robot=manager.get_hauler()
                assert robot,'Restored robot absent'
                robot.set_actor_tick_enabled(False)
                bank=economy.get_banked_salvage()
                cargo=economy.get_hauler_cargo()
                storage=manager.get_building(unreal.DownstreamStructureType.STORAGE)
                pos=storage.get_actor_location()+unreal.Vector(140,0,60)
                place(robot,pos)
                assert not economy.try_unload_hauler_cargo(robot),'Write failure unexpectedly committed delivery'
                assert economy.get_banked_salvage()==bank and economy.get_hauler_cargo()==cargo,'Write failure changed bank/cargo'
                item=economy.spawn_salvage(robot.get_actor_location(),2,False)
                assert not economy.try_load_hauler_cargo(item,robot),'Write failure unexpectedly consumed pickup'
                assert unreal.SystemLibrary.is_valid(item) and not item.is_claimed(),'Failed cargo write did not release pickup'
                assert not economy.set_hauling_enabled(True,pawn),'Write failure committed hauling intent'
                assert not economy.is_hauling_enabled() and save_file.read_bytes()==initial_bytes
                finish(True,'Disk failure preserves bank/cargo/pickup/intent and releases claim')
                return
            if mode=='reload':
                assert initial['version']>=2 and initial['hauler']['cargo']==2,'Reload fixture did not retain cargo'
                assert len(economy.get_built_structures())==3 and manager.get_hauler(),'Placed settlement did not restore'
                assert economy.get_inventory_capacity()==20 and str(economy.get_selected_skill_id())=='steady_aim'
                assert not economy.is_hauling_enabled(),'Paused preference did not restore'
                assert economy.get_hauler_cargo()==0 and economy.get_banked_salvage()==24,'Restored cargo was lost or duplicated'
                state['report'].update(structures=3,restored_bank=24,restored_cargo_delivered_once=True,capacity=20)
                stage('reload_stable')
                return
            assert initial['version']==1 and economy.get_banked_salvage()==40,'Legacy fixture not loaded'
            assert str(economy.get_selected_skill_id())=='steady_aim' and not economy.is_hauler_constructed()
            assert not economy.try_place_structure(unreal.DownstreamStructureType.STORAGE,unreal.Vector(9000,0,10),pawn)
            assert not economy.try_place_structure(unreal.DownstreamStructureType.STORAGE,unreal.Vector(-1740,-240,10),pawn),'Shelter overlap accepted'
            assert economy.get_banked_salvage()==40,'Invalid placement spent salvage'
            pawn.toggle_build_menu()
            assert pawn.is_build_menu_open(),'Builder did not open at home'
            pawn.select_structure(unreal.DownstreamStructureType.STORAGE)
            assert pawn.is_placement_selected(),'Builder selection absent'
            pawn.cancel_placement()
            assert not pawn.is_placement_selected(),'Placement cancel failed'
            cells=[(unreal.DownstreamStructureType.STORAGE,unreal.Vector(-900,-550,10)),
                   (unreal.DownstreamStructureType.WORKSHOP,unreal.Vector(-900,-950,10)),
                   (unreal.DownstreamStructureType.CHARGER,unreal.Vector(-900,-150,10))]
            for kind,pos in cells:
                assert economy.can_place_structure(kind,pos),'Expected clear cell blocked: '+str(pos)
                assert economy.try_place_structure(kind,pos,pawn),'Structure transaction failed'
                before=economy.get_banked_salvage()
                assert not economy.try_place_structure(kind,pos,pawn),'Duplicate structure accepted'
                assert economy.get_banked_salvage()==before,'Duplicate spent bank'
            assert economy.get_banked_salvage()==26 and economy.get_inventory_capacity()==20
            assert len(economy.get_built_structures())==3
            pawn.construct_hauler()
            assert economy.is_hauler_constructed() and economy.get_banked_salvage()==18
            assert not economy.try_construct_hauler(pawn),'Second robot accepted'
            pawn.toggle_build_menu()
            assert not pawn.is_build_menu_open()
            saved=json.loads(save_file.read_text())
            assert saved['version']>=2 and len(saved['structures'])==3,'Legacy migration/new structures not saved'
            state['report'].update(legacy_migrated=True,structures=3,build_cost=22,bank_after_build=18,capacity=20,builder_controls=True)
            stage('robot_spawn')
        elif state['stage']=='robot_spawn':
            robot=manager.get_hauler()
            if not robot: return
            assert elapsed<15,'Robot spawn stalled'
            state['robot']=robot
            guard=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DownstreamEnemy)[0]
            target_pos=unreal.Vector(-450,-1200,60)
            place(guard,unreal.Vector(target_pos.x,target_pos.y,90))
            target=economy.spawn_salvage(target_pos,4,False)
            assert target
            assert economy.set_hauling_enabled(True,pawn)
            state.update(guard=guard,target=target,robot_start=robot.get_actor_location())
            stage('guarded')
        elif state['stage']=='guarded':
            if elapsed>2:
                assert unreal.SystemLibrary.is_valid(state['target']) and economy.get_hauler_cargo()==0,'Hauler collected guarded salvage'
                assert state['robot'].get_target_pickup()!=state['target'],'Hauler targeted guarded salvage'
                place(state['guard'],unreal.Vector(2800,-2000,90))
                state['report']['hostile_avoidance']=True
                stage('hauling')
        elif state['stage']=='hauling':
            robot=state['robot']
            if economy.get_hauler_cargo()>0:
                state['report']['cargo_observed']=economy.get_hauler_cargo()
            if economy.get_banked_salvage()==22:
                assert economy.get_hauler_cargo()==0,'Cargo retained after delivery'
                assert not unreal.SystemLibrary.is_valid(state['target']),'Delivered source pickup still exists'
                assert 'cargo_observed' in state['report'],'Robot never carried cargo through the world'
                state['report'].update(delivery=True,bank_after_delivery=22,battery_after_job=economy.get_hauler_battery())
                assert economy.set_hauling_enabled(False,pawn)
                economy.update_hauler_battery(-90)
                state['low_battery']=economy.get_hauler_battery()
                stage('charging')
            elif elapsed>35:
                raise AssertionError('Haul stalled: '+str(robot.get_status())+' pos='+str(robot.get_actor_location())+' cargo='+str(economy.get_hauler_cargo()))
        elif state['stage']=='charging':
            status=str(state['robot'].get_status())
            if 'Charging' in status: state['report']['charging_seen']=True
            if state['report'].get('charging_seen') and economy.get_hauler_battery()>99:
                state['report'].update(charging=True,battery_recovered=economy.get_hauler_battery())
                stage('paused_settle')
            elif elapsed>35:
                raise AssertionError('Recharge stalled: '+status+' pos='+str(state['robot'].get_actor_location()))
        elif state['stage']=='paused_settle':
            if elapsed>6:
                state['idle_location']=state['robot'].get_actor_location()
                state['idle_battery']=economy.get_hauler_battery()
                stage('paused_stable')
        elif state['stage']=='paused_stable':
            if elapsed>3:
                robot=state['robot']
                assert (robot.get_actor_location()-state['idle_location']).length()<15,'Paused robot shuttled between buildings'
                assert abs(economy.get_hauler_battery()-state['idle_battery'])<.1,'Idle robot wasted battery'
                state['report']['paused_stable']=True
                robot.set_actor_tick_enabled(False)
                item=economy.spawn_salvage(robot.get_actor_location(),2,False)
                assert economy.try_load_hauler_cargo(item,robot),'Could not save in-flight cargo fixture'
                economy.update_hauler_battery(-37)
                assert economy.save_hauler_checkpoint()
                saved=json.loads(save_file.read_text())
                assert saved['version']>=2 and saved['hauler']['cargo']==2 and not saved['hauler']['hauling_enabled']
                state['report'].update(saved_cargo=2,saved_battery=saved['hauler']['battery'],saved_bank=22)
                finish(True,'Placement, spending, v1 migration, hostile avoidance, real hauling/delivery, recharge and idle/save passed')
        elif state['stage']=='reload_stable' and elapsed>3:
            assert economy.get_banked_salvage()==24 and economy.get_hauler_cargo()==0,'Reload deposited cargo twice'
            finish(True,'Schema2 structures/capacity/skill/intent restored and saved cargo delivered exactly once')
    except Exception as exc:
        finish(False,repr(exc))

state['handle']=unreal.register_slate_post_tick_callback(tick)

"""Real PIE gameplay acceptance, with an isolated save directory supplied by wrapper."""
import json
import time
from pathlib import Path
import unreal

root = Path(unreal.Paths.project_dir()).resolve().parents[1]
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert level.load_level('/Game/Downstream/Maps/M1_Expedition')
reload_only = '-DownstreamVerifyReload' in unreal.SystemLibrary.get_command_line()
state = {'stage':'begin','started':time.monotonic(),'report':{},'handle':None}

def finish(passed, message):
    state['report'].update(passed=passed, message=message, stage=state['stage'])
    name = 'expedition-reload-verification.json' if reload_only else 'expedition-play-verification.json'
    (root/'build'/name).write_text(json.dumps(state['report'],indent=2))
    unreal.log('DOWNSTREAM_EXPEDITION_VERIFIED ' + json.dumps(state['report']))
    unreal.unregister_slate_post_tick_callback(state['handle'])
    level.editor_request_end_play()
    unreal.SystemLibrary.quit_editor()

def stage(name):
    state['stage']=name
    state['stage_time']=time.monotonic()
    unreal.log('DOWNSTREAM_GAMEPLAY_STAGE ' + name)

def place(actor, x, y, z=100):
    actor.set_actor_location(unreal.Vector(x,y,z),False,False)
    movement = actor.get_component_by_class(unreal.CharacterMovementComponent)
    if movement: movement.stop_movement_immediately()

def freeze(enemy, frozen=True):
    enemy.set_actor_tick_enabled(not frozen)
    controller=enemy.get_controller()
    if controller: controller.stop_movement()
    movement=enemy.get_component_by_class(unreal.CharacterMovementComponent)
    if movement: movement.stop_movement_immediately()

def tick(delta):
    try:
        assert time.monotonic()-state['started']<100, 'Gameplay timeout at '+state['stage']
        if state['stage']=='begin':
            level.editor_request_begin_play()
            stage('spawn')
            return
        world=editor.get_game_world()
        if not world: return
        pawn=unreal.GameplayStatics.get_player_pawn(world,0)
        if not pawn: return
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        economy=unreal.DownstreamExpeditionSubsystem.get_for_world(world)
        elapsed=time.monotonic()-state['stage_time']
        if state['stage']=='spawn':
            if elapsed<3: return
            assert isinstance(pawn,unreal.DownstreamPlayerCharacter),'Wrong default pawn'
            assert economy,'Expedition subsystem absent'
            assert pawn.get_component_by_class(unreal.SkeletalMeshComponent).get_skeletal_mesh_asset(),'Invisible player'
            assert pc.get_hud().get_class().get_name()=='DownstreamDebugHUD','Gameplay HUD absent'
            rotation=pc.player_camera_manager.get_camera_rotation()
            assert abs(rotation.pitch+55)<.1 and abs(rotation.yaw-45)<.1,'Fixed camera changed'
            enemies=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DownstreamEnemy)
            assert len(enemies)==9,'Expected nine field enemies'
            chasers=[e for e in enemies if e.get_editor_property('kind')==unreal.DownstreamEnemyKind.CHASER]
            shooters=[e for e in enemies if e.get_editor_property('kind')==unreal.DownstreamEnemyKind.SHOOTER]
            assert len(chasers)==6 and len(shooters)==3,'Enemy kind layout differs'
            assert len(unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DownstreamSalvagePickup))==3,'Field caches absent'
            for enemy in enemies: freeze(enemy)
            state.update(chasers=chasers,shooters=shooters,original_pawn=pawn)
            state['report'].update(pawn_class=pawn.get_class().get_name(),enemies=len(enemies),chasers=len(chasers),shooters=len(shooters),camera=str(rotation),hud=pc.get_hud().get_class().get_name())
            if reload_only:
                assert economy.get_banked_salvage()==2,'Stash did not survive process restart'
                assert str(economy.get_selected_skill_id())=='steady_aim','Starter skill did not survive restart'
                assert economy.get_carried_salvage()==0,'Carried salvage incorrectly persisted'
                state['report'].update(banked=2,skill='steady_aim',field_reset=True)
                finish(True,'Save reload and field reset passed')
                return
            assert economy.get_banked_salvage()==0 and economy.get_carried_salvage()==0,'Test save isolation failed'
            assert economy.choose_starter_skill('steady_aim'),'Skill choice failed'
            assert abs(economy.get_ranged_damage_multiplier()-1.15)<.001,'Skill damage bonus absent'
            assert not economy.choose_starter_skill('salvage_handling'),'Second free node incorrectly granted'
            unreal.SystemLibrary.execute_console_command(world,'RebuildNavigation',pc)
            place(pawn,-1450,-850)
            state['move_start']=pawn.get_actor_location()
            stage('movement')
        elif state['stage']=='movement':
            pawn.add_movement_input(unreal.Vector(1,0,0),1,False)
            travel=(pawn.get_actor_location()-state['move_start']).length()
            if travel>430:
                state['report']['movement_cm']=travel
                place(pawn,-100,-1100)
                target=state['shooters'][0]
                place(target,500,-1100)
                state.update(target=target,target_health=target.get_health(),last_shot=0,shot_damage_seen=False)
                stage('ranged')
        elif state['stage']=='ranged':
            target=state['target']
            if unreal.SystemLibrary.is_valid(target) and not target.is_dead():
                if target.get_health()<state['target_health']:
                    state['shot_damage_seen']=True
                    state['report']['first_projectile_damage']=state['target_health']-target.get_health()
                if elapsed-state['last_shot']>.4:
                    pawn.fire_at(target.get_actor_location())
                    state['last_shot']=elapsed
            else:
                assert state['shot_damage_seen'],'Projectile never damaged target'
                drops=[p for p in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DownstreamSalvagePickup) if p.get_editor_property('amount')==2 and not p.get_editor_property('recovery_bag')]
                assert len(drops)==1,'Enemy death did not create exactly one drop'
                pos=drops[0].get_actor_location()
                place(pawn,pos.x,pos.y)
                assert economy.try_interact(pawn),'Enemy salvage collection failed'
                assert economy.get_carried_salvage()==2,'Carried quantity incorrect'
                place(pawn,-1450,-850)
                assert economy.try_interact(pawn),'Home extraction failed'
                assert economy.get_carried_salvage()==0 and economy.get_banked_salvage()==2,'Extraction accounting incorrect'
                state['report'].update(projectile_kill=True,enemy_drop=True,banked_salvage=2,starter_skill='steady_aim')
                place(pawn,100,-1100)
                melee=state['chasers'][0]
                place(melee,200,-1100)
                pawn.set_actor_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0),False)
                state.update(melee=melee,melee_health=melee.get_health())
                state['point_last_shot']=0
                stage('point_blank')
        elif state['stage']=='point_blank':
            target=state['melee']
            if elapsed-state['point_last_shot']>.35:
                pawn.fire_at(target.get_actor_location())
                state['point_last_shot']=elapsed
            if target.get_health()<state['melee_health']:
                state['report']['point_blank_projectile_damage']=state['melee_health']-target.get_health()
                state['melee_health']=target.get_health()
                pawn.perform_melee()
                stage('melee')
            elif elapsed>10:
                raise AssertionError('Point blank shot failed: player='+str(pawn.get_actor_location())+' target='+str(target.get_actor_location()))
        elif state['stage']=='melee':
            target=state['melee']
            assert target.get_health()<state['melee_health'],'Melee did not damage nearby enemy'
            state['report']['melee_damage']=state['melee_health']-target.get_health()
            place(target,2400,1900)
            chaser=state['chasers'][1]
            place(chaser,650,-1100)
            freeze(chaser,False)
            state.update(ai_target=chaser,ai_start=chaser.get_actor_location(),health_start=pawn.get_health())
            stage('chaser')
        elif state['stage']=='chaser':
            chaser=state['ai_target']
            moved=(chaser.get_actor_location()-state['ai_start']).length()
            if pawn.get_health()<state['health_start']:
                assert moved>250,'Chaser attack bypassed path movement'
                state['report'].update(chaser_moved_cm=moved,chaser_damage=state['health_start']-pawn.get_health())
                freeze(chaser)
                place(chaser,2500,1900)
                shooter=state['shooters'][1]
                place(shooter,700,-1100)
                freeze(shooter,False)
                state.update(ai_target=shooter,health_start=pawn.get_health())
                stage('shooter')
        elif state['stage']=='shooter':
            if pawn.get_health()<state['health_start']:
                shooter=state['ai_target']
                state['report']['shooter_damage']=state['health_start']-pawn.get_health()
                freeze(shooter)
                place(pawn,-1450,-850)
                before=pawn.get_health()
                pawn.receive_combat_damage(50,shooter)
                assert pawn.get_health()==before,'Home protection failed'
                pawn.interact()
                assert pawn.get_health()==pawn.get_max_health(),'Home interaction did not heal'
                state['report'].update(home_protection=True,home_heal=True)
                place(pawn,100,-1100)
                pickup=economy.spawn_salvage(pawn.get_actor_location(),2,False)
                assert pickup and economy.try_interact(pawn),'Could not stage carried salvage'
                pawn.receive_combat_damage(500,shooter)
                assert pawn.is_dead(),'Fatal damage did not kill player'
                assert economy.get_carried_salvage()==0,'Death did not clear carried salvage'
                assert economy.get_banked_salvage()==2,'Death removed banked stash'
                stage('respawn')
        elif state['stage']=='respawn':
            if pawn!=state['original_pawn']:
                assert economy.is_at_home(pawn.get_actor_location()),'Respawn outside home'
                assert pawn.get_health()==100,'Respawn health incorrect'
                bags=[p for p in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.DownstreamSalvagePickup) if p.get_editor_property('recovery_bag')]
                assert len(bags)==1 and bags[0].get_editor_property('amount')==2,'Death recovery bag incorrect'
                pos=bags[0].get_actor_location()
                place(pawn,pos.x,pos.y)
                assert economy.try_interact(pawn),'Recovery collection failed'
                assert economy.get_carried_salvage()==2,'Recovery amount incorrect'
                state['report'].update(death_respawn=True,recovery_bag=True,recovered_salvage=2)
                place(pawn,100,-1100)
                state.update(dodge_start=pawn.get_actor_location(),dodge_health=pawn.get_health())
                pawn.dodge()
                assert pawn.is_dodging(),'Dodge never activated'
                pawn.receive_combat_damage(10,state['ai_target'])
                assert pawn.get_health()==state['dodge_health'],'Dodge protection failed'
                stage('dodge')
        elif state['stage']=='dodge':
            if not pawn.is_dodging() and elapsed>.35:
                moved=(pawn.get_actor_location()-state['dodge_start']).length()
                assert moved>100,'Dodge launch did not move player'
                pawn.dodge()
                assert not pawn.is_dodging(),'Dodge cooldown bypassed'
                pawn.receive_combat_damage(10,state['ai_target'])
                assert pawn.get_health()==state['dodge_health']-10,'Dodge protection did not expire'
                state['report'].update(dodge_cm=moved,dodge_protection=True,dodge_cooldown=True)
                finish(True,'Movement, ranged/melee, both enemy AI, extraction, home, death/recovery, dodge and save passed')
    except Exception as exc:
        finish(False,repr(exc))

state['handle']=unreal.register_slate_post_tick_callback(tick)

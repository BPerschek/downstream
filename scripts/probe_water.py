"""Diagnostic: terrain collision and navigation on the water site at runtime."""
import json, time
from pathlib import Path
import unreal
root = Path(unreal.Paths.project_dir()).resolve().parents[1]
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
assert level.load_level('/Game/Downstream/Maps/M1_Expedition')
state = {'t': time.monotonic(), 'begun': False, 'h': None}
def done(rep):
    (root / 'build' / 'water-probe.json').write_text(json.dumps(rep, indent=2, default=str))
    unreal.unregister_slate_post_tick_callback(state['h'])
    level.editor_request_end_play(); unreal.SystemLibrary.quit_editor()
def tick(dt):
    try:
        if not state['begun']:
            level.editor_request_begin_play(); state['begun'] = True; state['t'] = time.monotonic(); return
        world = editor.get_game_world()
        if not world or time.monotonic() - state['t'] < 4: return
        rep = {}
        site = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.DownstreamWaterSite)[0]
        tm = site.get_editor_property('terrain_mesh')
        rep['collision_enabled'] = str(tm.get_collision_enabled())
        rep['profile'] = str(tm.get_collision_profile_name())
        rep['sections'] = tm.get_num_sections()

        out = getattr(unreal, 'KismetProceduralMeshLibrary', None) or unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(tm, 0)
        verts, tris, norms = out[0], out[1], out[2]
        rep['vert_count'] = len(verts); rep['normal_samples'] = [str(norms[i]) for i in (40, 200, 400)]
        rep['vert_samples'] = [str(verts[i]) for i in (40, 200, 400)]
        rep['first_tri'] = [tris[0], tris[1], tris[2]]
        rep['terrain_material'] = str(tm.get_material(0).get_path_name() if tm.get_material(0) else None)
        wm = site.get_editor_property('water_mesh')
        rep['water_material'] = str(wm.get_material(0).get_path_name() if wm.get_material(0) else None)
        wout = getattr(unreal, 'KismetProceduralMeshLibrary', None) or unreal.ProceduralMeshLibrary.get_section_from_procedural_mesh(wm, 0)
        rep['water_verts'] = len(wout[0]); rep['water_sample'] = [str(wout[0][i]) for i in (9*32+15, 9*32+5)]
        traces = {}
        for name, x, y in [('plateau', 800, -1000), ('basin', 1850, -1650), ('northramp', 1750, -650)]:
            hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, 800), unreal.Vector(x, y, -300), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
            if hit:
                t = hit.to_tuple()
                traces[name] = {'hit': t[0], 'actor': str(t[9].get_name() if t[9] else None), 'z': t[4].z if hasattr(t[4],'z') else str(t[4])}
            else:
                traces[name] = None
        rep['traces'] = traces
        nav = unreal.NavigationSystemV1.get_navigation_system(world)
        for name, x, y, z in [('plateau', 800, -1000, 180), ('basin', 1850, -1650, 20)]:
            p = unreal.NavigationSystemV1.project_point_to_navigation(world, unreal.Vector(x, y, z), None, None, unreal.Vector(50, 50, 300))
            rep['nav_' + name] = str(p)
        path = unreal.NavigationSystemV1.find_path_to_location_synchronously(world, unreal.Vector(-1300, -200, 100), unreal.Vector(1850, -1650, 30))
        rep['path_valid'] = bool(path and path.is_valid()); rep['path_partial'] = bool(path and path.is_partial())
        rep['path_points'] = [str(p) for p in (path.get_editor_property('path_points') if path else [])][-4:]
        done(rep)
    except Exception as e:
        done({'error': repr(e)})
state['h'] = unreal.register_slate_post_tick_callback(tick)

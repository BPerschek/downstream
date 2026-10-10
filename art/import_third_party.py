"""Import retained CC0 third-party assets (ambientCG textures, Kenney GLB models) into /Game/Downstream/ThirdParty.

Run through the Unreal Python commandlet. Re-running replaces the imported assets.
Static meshes are renamed SM_K_<model> so native code can reference stable paths.
"""
import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve().parents[1]
THIRD = ROOT / 'third_party'
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary
report = {'textures': {}, 'meshes': {}, 'warnings': []}

def run(tasks):
    if tasks:
        tools.import_asset_tasks(tasks)

def task(filename, destination, name=None):
    t = unreal.AssetImportTask()
    t.filename = str(filename)
    t.destination_path = destination
    if name:
        t.destination_name = name
    t.automated = True
    t.replace_existing = True
    t.save = True
    return t

# ---- ambientCG textures
tex_tasks = []
for folder in sorted(p for p in (THIRD / 'ambientcg').iterdir() if p.is_dir()):
    for f in sorted(folder.glob('*.jpg')):
        kind = f.stem.split('_')[-1]
        tex_tasks.append(task(f, '/Game/Downstream/ThirdParty/AmbientCG', f'T_{folder.name}_{kind}'))
run(tex_tasks)
for t in tex_tasks:
    path = f'{t.destination_path}/{t.destination_name}'
    tex = unreal.load_asset(path)
    if not tex:
        report['warnings'].append('missing texture ' + path)
        continue
    kind = t.destination_name.split('_')[-1]
    if kind == 'NormalDX':
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('srgb', False)
    elif kind in ('Roughness', 'AmbientOcclusion'):
        tex.set_editor_property('srgb', False)
    lib.save_loaded_asset(tex, False)
    report['textures'][t.destination_name] = path

# ---- Kenney models: one folder per model so generated materials/textures do not collide.
for kit in sorted(p for p in THIRD.glob('kenney-*') if (p / 'Models').is_dir()):
    kit_name = kit.name.split('-', 1)[1].capitalize()
    for glb in sorted((kit / 'Models').glob('*.glb')):
        model = glb.stem.replace('-', '_')
        dest = f'/Game/Downstream/ThirdParty/Kenney/{kit_name}/{model}'
        if lib.does_directory_exist(dest):
            lib.delete_directory(dest)
        run([task(glb, dest)])
        meshes = [a for a in lib.list_assets(dest, recursive=True, include_folder=False)
                  if unreal.EditorAssetLibrary.find_asset_data(a).asset_class_path.asset_name == 'StaticMesh']
        if not meshes:
            report['warnings'].append(f'no static mesh from {glb.name}')
            continue
        if len(meshes) > 1:
            report['warnings'].append(f'{glb.name} produced {len(meshes)} meshes; using the first')
        target = f'{dest}/SM_K_{model}'
        source = meshes[0].split('.')[0]
        if source != target:
            lib.rename_asset(source, target)
        mesh = unreal.load_asset(target)
        box = mesh.get_bounding_box()
        size = box.max - box.min
        report['meshes'][f'{kit_name}/{model}'] = {'path': target, 'size_cm': [round(size.x, 1), round(size.y, 1), round(size.z, 1)], 'all_meshes': len(meshes)}
        lib.save_directory(dest, only_if_is_dirty=False, recursive=True)

(ROOT / 'build' / 'third-party-import.json').write_text(json.dumps(report, indent=2))
unreal.log('DOWNSTREAM_THIRD_PARTY_IMPORTED ' + json.dumps({'textures': len(report['textures']), 'meshes': len(report['meshes']), 'warnings': report['warnings']}))

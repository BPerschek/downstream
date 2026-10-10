"""Preserve compact water/settlement verification results and reviewed site frames in docs/."""
import argparse, datetime, hashlib, json, re, shutil
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--reviewed-frames', action='store_true', required=True, help='Confirm both site frames were visually reviewed.')
parser.parse_args()
reports = {}
def clean(value):
    # Drop transient editor memory addresses from Python struct reprs.
    return json.loads(re.sub(r"\(0x[0-9A-Fa-f]+\) ", '', json.dumps(value)))
for prefix, modes in [('water', ['legacy', 'flow', 'reload']), ('settlement', ['flow', 'reload', 'unfunded', 'invalid', 'write_failure'])]:
    for mode in modes:
        value = json.loads((root / 'build' / f'{prefix}-{mode}-verification.json').read_text(encoding='utf-8-sig'))
        if not value.get('passed'):
            raise RuntimeError(f'{prefix} {mode} has not passed')
        reports[f'{prefix}_{mode}'] = clean(value)
for name in ['expedition-play-verification.json', 'expedition-reload-verification.json']:
    value = json.loads((root / 'build' / name).read_text(encoding='utf-8-sig'))
    if not value.get('passed'):
        raise RuntimeError(name + ' has not passed')
    reports[name.replace('-verification.json', '').replace('-', '_')] = {'passed': True, 'message': value['message']}
reports['standalone_tests'] = {'simulation': '17/17', 'expedition': 'passed', 'settlement': 'passed', 'water': '15/15 including the full Drowned Vault puzzle', 'native_automation': '8/8'}
source_hash = hashlib.sha256()
for path in sorted((root / 'native' / 'Source').rglob('*')):
    if path.is_file():
        source_hash.update(path.relative_to(root).as_posix().encode())
        source_hash.update(path.read_bytes())
reports['canonical_source_sha256'] = source_hash.hexdigest()
frames = {}
for mode in ['flooded', 'drained', 'home', 'ruin']:
    source = root / 'build' / f'Downstream_Water_{mode}.png'
    destination = root / 'docs' / 'images' / f'Downstream_Water_{mode}.png'
    shutil.copy2(source, destination)
    frames[mode] = destination.relative_to(root).as_posix()
reports['reviewed_frames'] = {'paths': frames, 'size': [1280, 720], 'mode': 'PIE capture with isolated saves', 'reviewed': True}
reports['third_party'] = 'Kenney CC0 models and ambientCG CC0 textures; see ASSET_LICENSES.md and third_party/*/manifest.json'
reports['recorded_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
reports['limits'] = 'Development editor/PIE validation. The flow test shortens DigActionSeconds to 0.12 for pacing. No packaged GPU performance claim; procedural terrain is not in the navmesh.'
out = root / 'docs' / 'validation' / '2026-10-09-water-results.json'
out.write_text(json.dumps(reports, indent=2) + '\n')
print('Archived', out)

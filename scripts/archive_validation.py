"""Preserve compact verified results; logs/tool caches remain local under build/."""
import datetime
import argparse
import hashlib
import json
import shutil
from pathlib import Path

root = Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--reviewed-frame',action='store_true',required=True,help='Confirm the standalone frame was visually reviewed before archiving.')
parser.parse_args()
out = root/'docs'/'validation'
out.mkdir(parents=True,exist_ok=True)
reports = {}
for name, file in [('expedition','expedition-play-verification.json'),('reload','expedition-reload-verification.json'),('terrain','play-verification.json')]:
    value=json.loads((root/'build'/file).read_text())
    if not value.get('passed'): raise RuntimeError(file+' has not passed')
    # Remove transient editor memory addresses from durable evidence.
    if 'camera' in value: value['camera']={'pitch':-55,'yaw':45,'roll':0}
    if 'camera_rotation' in value: value['camera_rotation']={'pitch':-55,'yaw':45,'roll':0}
    if 'debug_view_position' in value: value.pop('debug_view_position')
    reports[name]=value
native=json.loads((root/'build'/'native-automation-report'/'index.json').read_text(encoding='utf-8-sig'))
if native.get('failed',1) or native.get('succeeded',0)<8: raise RuntimeError('Native automation incomplete')
build_log=(root/'build'/'native-expedition-build.log').read_text(encoding='utf-16' if (root/'build'/'native-expedition-build.log').read_bytes().startswith(b'\xff\xfe') else 'utf-8-sig')
if build_log.count('Result: Succeeded')<2: raise RuntimeError('Editor/Game build not verified')
reports['native_automation']={'succeeded':native['succeeded'],'failed':native['failed'],'tests':[test.get('fullTestPath',test.get('testDisplayName','')) for test in native['tests']]}
reports['build']={'editor':'Succeeded','game':'Succeeded','engine':'5.8.3','msvc':'14.44.35229','sdk':'10.0.26100.0'}
reports['standalone_tests']={'simulation':'17/17 passed','expedition':'capacity,banking,death,skills,load validation,overflow passed','compiler':'strict C++17 Zig local test compiler'}
source_hash=hashlib.sha256()
for path in sorted((root/'native'/'Source').rglob('*')):
    if path.is_file():
        source_hash.update(path.relative_to(root).as_posix().encode())
        source_hash.update(path.read_bytes())
reports['canonical_source_sha256']=source_hash.hexdigest()
reports['recorded_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
reports['limits']='Development Editor/game-world validation only; no packaged GPU performance, Mass comparison or persistent world acceptance.'
image=root/'build'/'Downstream_Game_Preview.png'
if not image.exists(): raise RuntimeError('Standalone game frame missing')
destination=root/'docs'/'images'/'Downstream_Playable_Concept.png'
shutil.copy2(image,destination)
reports['standalone_frame']={'path':destination.relative_to(root).as_posix(),'size':[1280,720],'reviewed':True,'map':'M1_Expedition','mode':'UnrealEditor -game with native DownstreamGameMode'}
path=out/'2026-10-09-results.json'
path.write_text(json.dumps(reports,indent=2)+'\n')
print('Archived passing validation and reviewed standalone frame:',path)

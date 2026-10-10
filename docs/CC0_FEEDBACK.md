# Settlement CC0 feedback audio

Three short interface sounds from **Kenney Interface Sounds 1.0** were acquired
from [Kenney's official pack page](https://kenney.nl/assets/interface-sounds) on
**2026-10-09**. That page explicitly lists Creative Commons CC0 and links the
download used here. The extracted vendor `third_party/kenney-interface/License.txt`
is retained byte-for-byte; it states Creative Commons Zero (CC0), allows
personal, educational and commercial use, and says credit is optional.
The license is [CC0 1.0 Universal](https://creativecommons.org/publicdomain/zero/1.0/).

| Intended feedback | Retained original | PCM WAV for import | Duration | Channels | Suggested volume |
| --- | --- | --- | --- | --- | --- |
| Successful settlement build / confirm | `Audio/confirmation_001.ogg` | `art/cc0_feedback/S_SettlementBuild.wav` | 0.301043 s | Mono | 0.50 |
| Rejected settlement action | `Audio/error_004.ogg` | `art/cc0_feedback/S_SettlementError.wav` | 0.116100 s | Stereo | 0.45 |
| Cargo transfer / robot docking | `Audio/switch_001.ogg` | `art/cc0_feedback/S_SettlementCargo.wav` | 0.672245 s | Stereo | 0.60 |

These are separate sound families from the pack, keeping confirmation, rejection
and docking feedback distinct. The volume multipliers are initial mix suggestions;
the game mix still needs an in-context listening check.

All WAVs are RIFF, 44,100 Hz, signed 16-bit PCM. Blender **5.2.2 LTS** bundled
`aud.Sound.write` converted their codec/container while retaining sample rate
and channel count. No gain normalization, silence trimming, mixing or pitch
change was applied. The WAV durations include decoder padding. PCM validation
reopened all files using Python `wave`, checked duration and non-silence, and
found zero clipped samples. Peak levels are -0.930, -1.252 and -0.952 dBFS
respectively. Cargo's quieter average level prompted the higher suggested
multiplier. Metadata and PCM levels were checked without playing audio through
the desktop UI.

## Provenance and reproducibility

The official archive URL is
<https://kenney.nl/media/pages/assets/interface-sounds/fa43c1dd4d-1677589452/kenney_interface-sounds.zip>.
It was 834,536 bytes with SHA256
`f2193d072726d6758a5f7871b2dcc54dcce0d5c35c6f0a62f92549b327c81232`.
Only three selected original OGGs and the unchanged vendor license remain under
`third_party/kenney-interface/`; the full archive is not retained.

`art/cc0_feedback/manifest.json` and `third_party/kenney-interface/manifest.json`
record all selected-source hashes, output hashes, license hash, measured frame
counts, levels and conversion tool. Rebuild the WAVs from the retained OGGs:

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.2\blender.exe' --background --factory-startup --python-exit-code 1 --python .\art\cc0_feedback\convert_audio.py
```

Import the WAVs with Unreal's sound factory into a dedicated settlement audio
folder, preserving the `S_SettlementBuild`, `S_SettlementError` and
`S_SettlementCargo` names. Attach them to the settlement tuning/data references
and apply the volume multipliers at playback. Asset import and runtime playback
are performed by the integrating agent; the asset preparation work does not
claim an Unreal runtime listening result.

For the repository license ledger, the creator is **Kenney**, the terms are
**CC0 1.0 Universal**, acquisition is **2026-10-09**, and source destinations
are `third_party/kenney-interface/` and `art/cc0_feedback/`. Optional attribution:
“Interface sound effects by Kenney (kenney.nl), CC0.” No endorsement is implied.

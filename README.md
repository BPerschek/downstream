# DOWNSTREAM

DOWNSTREAM is a native Unreal Engine 5.8 single-player isometric expedition prototype. The saved **M1_Expedition** level contains a home clearing, a salvage route, an industrial ruin, two enemy behaviors, combat, collection, extraction, death recovery and a persistent stash. The owner's 2026-10-09 request prioritized a working gameplay concept before the remaining M0 performance work.

## Play

Double-click **Launch_DOWNSTREAM.cmd** in the repository. It opens `/Game/Downstream/Maps/M1_Expedition` in a separate game window and rebuilds navigation at startup. Close that window to stop. From this repository root, the equivalent is:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/play.ps1
```

| Control | Action |
| --- | --- |
| WASD | Move relative to the fixed isometric camera |
| Mouse | Aim |
| Left mouse, hold | Fire projectiles; ammunition is unlimited |
| Right mouse | Nearby melee strike |
| Space | Dodge in the movement direction, or facing direction while standing |
| E | Collect nearby salvage; at home, extract and heal |
| 1 | Choose Steady Aim: +15% ranged damage |
| 2 | Choose Salvage Handling: +4 carrying slots |

Explore, fight, collect, then return to the green home area and press E. Six chasers and three shooters guard three caches containing four salvage each. Defeated enemies drop two salvage. Base carrying capacity is 12; a pickup must fit in full. Home's 340 cm radius protects the player from damage. Death leaves carried salvage in a recovery bag and respawns the player at home after 1.5 seconds. The HUD shows health, carry, bank, interaction prompts and the starter choice.

Choose one free starter node; the choice and banked salvage persist across launches. Carry, enemies, field pickups and recovery bags reset on a fresh launch. Quitting does not bank carried salvage. The default save is `unreal/Downstream/Saved/Downstream/prototype-save.json`; see [save and test contracts](tests/EXPEDITION_TESTS.md) before modifying it.

For editing, open `unreal/Downstream/Downstream.uproject`, then `/Game/Downstream/Maps/M1_Expedition`. M0_TestWorld remains a technical reference, launched with `scripts/play.ps1 -TechnicalMap`. F6 toggles the terrain station view; F7/F8 queue height edits. This debug mesh has no gameplay collision or navigation.

## Current delivery and evidence

The project now compiles native **Editor and Game** targets. Its simulation adapter runs at 10 Hz on the game thread, and DownstreamRender presents one dirty terrain patch. Canonical source is `native/Source`; the build script maintains the ignored `unreal/Downstream/Source` mirror. Edit canonical files only.

The 17 standalone simulation tests, expedition economy suite and all eight native Unreal automation cases passed. Game-world verification passed movement, projectiles, melee, both enemy attacks, home protection/healing, collection/extraction, death/respawn/recovery and dodge. A separate process restored the bank and starter choice while resetting the field. The M0 native movement/terrain regression also passed. A standalone M1 frame was reviewed at 1280x720 with the visible player and readable HUD. [Validation results](docs/validation/2026-10-09-results.json) preserve the evidence; [M0_STATUS](docs/M0_STATUS.md) records remaining technical criteria.

![M1 expedition: player at home with health, carry, bank and starter-node HUD](docs/images/Downstream_Playable_Concept.png)

This is an early gameplay slice. Settlement building, autonomous robots, water, factions, a full skill tree, persistent world entities and endgame progression remain future work. Actor/Mass comparison, worker-thread snapshots and a packaged RTX 5050 / 1080p / 60 FPS benchmark are also open.

## Build and verify

Run PowerShell from this repository root. Save and close project editors before rebuilding DLLs. Run builds, map generation and editor verification sequentially to avoid Windows package locks.

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_native.ps1
powershell -ExecutionPolicy Bypass -File scripts/test_sim.ps1
powershell -ExecutionPolicy Bypass -File scripts/test_expedition.ps1
powershell -ExecutionPolicy Bypass -File scripts/test_native.ps1
powershell -ExecutionPolicy Bypass -File scripts/verify_expedition.ps1
powershell -ExecutionPolicy Bypass -File scripts/verify_play.ps1
```

`verify_expedition.ps1` uses an isolated save directory under `build/` and a separate process to verify reload, preserving the owner's normal save. `verify_play.ps1` checks movement, camera and the M0 native terrain station. Logs and generated reports are ignored under `build/`.

On another machine, install compatible Unreal 5.8 and Visual Studio C++ tools/Windows SDK, update `scripts/project_paths.ps1`, and reassociate the project with that engine installation. Run `scripts/setup_test_compiler.ps1` once if the portable compiler for plain C++ tests is absent. This PC uses UE 5.8.3, Visual Studio Community 2026, MSVC 14.44.35229 and SDK 10.0.26100.0; [TOOLCHAIN](docs/TOOLCHAIN.md) explains compatibility and source synchronization.

## Regenerate content

Committed maps/assets can be played without regeneration. Rebuild original meshes with Blender 5.2 using `art/create_blockouts.py`, then `art/validate_blockouts.py`. `scripts/create_prototype_audio.py` generates four original PCM cues with standard Python. Build native targets before running Unreal Python scripts that use native classes.

For a full base-map rebuild, run `scripts/regenerate_world.ps1`, then `scripts/install_native_layer.py` through Unreal's Python commandlet to attach the terrain station to M0. Finally run `scripts/regenerate_expedition.ps1` to create/update M1 gameplay actors, materials and audio references, and save navigation. M1 copies M0 only on its first creation; later regeneration retains that existing environment. Both map generators replace generated actors. Keep hand-authored variants under another map name. [PLAYABLE_CONCEPT](docs/PLAYABLE_CONCEPT.md) includes the commandlet invocation and sequence.

The nine original meshes retain editable Blender source, FBX exports, palette and validation manifests in `art/`. Epic's mannequin and template resources retain their existing content mounts. [ASSET_LICENSES](ASSET_LICENSES.md) records provenance. The owner-installed AdaptivePathfindingSystem plugin is enabled locally; gameplay uses standard Recast/AIController. If that optional plugin is absent on another machine, disable its project entry before opening/building. Harvest/Ready Map payloads and the separate UE4.27 Shooter Game sample are not dependencies of this level.

## Future agent handoff

Read [AGENTS](AGENTS.md), [PLAYABLE_CONCEPT](docs/PLAYABLE_CONCEPT.md), [ARCHITECTURE_DECISIONS](docs/ARCHITECTURE_DECISIONS.md), [TOOLCHAIN](docs/TOOLCHAIN.md), [EXPEDITION_TESTS](tests/EXPEDITION_TESTS.md) and [M0_STATUS](docs/M0_STATUS.md). The playable concept records the loop, tuning seam, module ownership, generation workflow and next work. The [v0.3 design brief](docs/reference/DOWNSTREAM_Design_Doc_v0.3.md) and dated specs/plans supply design context and history.

# Playable expedition and future handoff

The owner's 2026-10-09 request prioritized a working level now, original or open-source assets as needed, and an up-to-date repository for future agents. The current slice is a single-player home-to-ruin expedition in Unreal 5.8. It advances M1 gameplay while incomplete M0 performance criteria remain open. The [v0.3 design brief](reference/DOWNSTREAM_Design_Doc_v0.3.md) is reference content; current user direction governs scope.

## Current player loop

Launch `/Game/Downstream/Maps/M1_Expedition` through `Launch_DOWNSTREAM.cmd` in the repository or `scripts/play.ps1`. The camera follows at pitch -55 degrees and yaw 45 degrees. WASD moves relative to it, the mouse aims, holding left mouse fires, right mouse strikes nearby enemies, Space dodges, and E interacts. Keys 1/2 choose one free starter node: `steady_aim` adds 15% ranged damage; `salvage_handling` adds four carry slots. A successfully saved choice cannot be exchanged through the current UI.

Six chasers pursue and strike; three shooters pursue into range and fire projectiles. Three caches contain four salvage each, and defeated enemies drop two units. Ammunition is unlimited. The HUD displays health, carried/capacity, banked salvage, starter node, status messages and collection/extraction prompts. Hit flashes, pickup lighting and four original synthesized sound cues provide feedback.

Base capacity is 12. E claims the nearest eligible pickup within 220 cm only when its whole quantity fits. At home, when no eligible pickup takes interaction priority, E banks all carry. Home interaction also restores full health. The 340 cm horizontal home radius protects the player from combat damage; dodging protects during its dodge window. Death creates a recovery bag for carried salvage and respawns at home after 1.5 seconds. The bank survives death.

The level has a finite set of enemies/pickups per fresh launch, with repeatable collection and extraction. There is no win screen, campaign, currency spending or endgame. Recovery bags remain only during the current run. A fresh launch resets carry, enemies and field pickups while restoring bank and selected starter node.

## Authoritative state and extension seams

| Module | Responsibility | Boundary to preserve |
| --- | --- | --- |
| DownstreamSim | Standard C++ terrain simulation and ExpeditionState economy | State headers have no UObject, Actor or rendering dependency; Unreal wrapper depends on Core only |
| Downstream | World simulation adapter, tuning, economy interaction, pickups, extraction presentation and JSON save commits | Owns inventory/stash; UI reads getters and submits requests |
| DownstreamCombat | Native character/controller/GameMode, GAS health, projectiles, melee, dodge, enemy AI and respawn | Depends on Downstream; never depends on DownstreamRender |
| DownstreamRender | Canvas gameplay HUD, procedural terrain mesh and developer controls | Depends on DownstreamCombat and Downstream; presents authoritative state |

Editable native source lives in `native/Source`. `scripts/build_native.ps1` stages the ignored project Source mirror and rejects independent mirror edits. Reconcile changes into canonical source before rebuilding. Build Editor and Game targets together: the launcher uses the editor executable's game mode and needs Editor modules as well as Game compilation.

`/Game/Downstream/Data/DA_PrototypeTuning` supplies health, speeds, attack damage/cooldowns, projectile values, capacity, home location/radius, respawn delay and optional sound/material references. `UDownstreamPrototypeTuning` has complete native fallback defaults. Balance through this data asset. Keep stable save IDs `steady_aim` and `salvage_handling`; changing them requires an explicit schema/migration decision.

Health is owned by GAS attributes/ability-system components. Attacks and cooldowns currently use native frame-rate behavior rather than a complete library of granted abilities/effects. Enemy movement uses Recast and AIController. The owner-installed AdaptivePathfindingSystem plugin is enabled locally, but its algorithms are not integrated into this loop.

The simulation adapter runs on the game thread at 10 Hz. One 32x32 terrain station consumes dirty metadata and updates a procedural mesh. F6 switches view; F7/F8 enqueue edits. The patch has collision and navigation disabled. Multiple patches require central dirty consumption and dispatch. Worker ownership, immutable snapshots, terrain collision and local navigation invalidation are future work.

## Save contract

Default file: `unreal/Downstream/Saved/Downstream/prototype-save.json`.

```json
{
  "version": 1,
  "banked_salvage": 27,
  "selected_skill": "steady_aim"
}
```

Only committed bank and starter node persist. File writes use `.tmp` and `.bak` siblings; memory state commits after successful writes. Failed extraction retains carry. Invalid/unsupported saves are preserved, reported and block bank/skill saves for that session. Never delete an owner's save to make a test pass. [EXPEDITION_TESTS](../tests/EXPEDITION_TESTS.md) documents validation, backup recovery, failure semantics and subsystem APIs.

`verify_expedition.ps1` supplies an absolute `-DownstreamSaveDir` under `build/expedition-test-save` and launches a separate reload process. Keep this isolation in runtime tests.

## Reproduce and modify the level

Committed M1 content is the normal starting point; generation is for deliberate updates. Install the engine/toolchain described in [TOOLCHAIN](TOOLCHAIN.md), set `scripts/project_paths.ps1`, reassociate the project engine and build native targets. If AdaptivePathfindingSystem is absent, disable its optional `.uproject` entry. Keep required engine GameplayAbilities and ProceduralMeshComponent plugins enabled. No commercial art pack is required.

For a full generated base, save and close project editors, then:

1. Generate and validate meshes with Blender 5.2 using `art/create_blockouts.py` and `art/validate_blockouts.py`. Generate cues with standard Python using `scripts/create_prototype_audio.py`.
2. Run `scripts/regenerate_world.ps1` to import base art and save M0_TestWorld with navigation.
3. Attach the compiled terrain layer to M0 through Unreal Python with the command below.
4. Run `scripts/regenerate_expedition.ps1` to save M1 gameplay actors, materials, tuning references and navigation. It clones M0 only when M1 does not already exist; later runs replace generated M1 actors over its existing environment.
5. Run the plain tests, native automation and both runtime verifiers listed in [README](../README.md), then inspect a standalone game frame.

```powershell
. .\scripts\project_paths.ps1
$nativeLayerScript = (Resolve-Path .\scripts\install_native_layer.py).Path
& $DownstreamCommandlet $DownstreamProject '-run=pythonscript' "-script=$nativeLayerScript" '-unattended' '-nop4' '-nosplash' '-NullRHI'
```

Run one build/editor automation/map mutation at a time. Python commandlets alone do not complete the editor navigation lifecycle; regeneration uses a separate hidden editor to bake navigation. The launcher explicitly rebuilds navigation at startup. Full rebuilds suit this small static map and do not establish the future local-invalidation system. Use named Unreal Python Rotator arguments (`pitch`, `yaw`, `roll`). Keep hand-authored map variants under a separate name.

Original assets mount at `/Game/Downstream/Art`; Epic's mannequin remains at `/Game/Characters`, and shared template resources at `/Game/LevelPrototyping`. Preserve mounts/dependencies. The robot is an unrigged static asset with no autonomy. Record imports in [ASSET_LICENSES](../ASSET_LICENSES.md).

## Verification and continuation

Editor/Game compilation, 17 standalone simulation tests, expedition economy tests, all eight native engine cases and M1 generation passed. The full game-world check passed player projectiles, point-blank damage, melee, both enemy attacks, home protection/healing, collection/extraction, death/respawn/recovery and dodge distance/protection/cooldown. A separate process restored two banked salvage and `steady_aim`, with nine enemies and three caches reset. The current M0 integration check also passed movement, fixed camera, native tick timing, deferred edits, dirty rebuilds, idle stability and HUD. The [standalone M1 frame](images/Downstream_Playable_Concept.png) was reviewed at 1280x720; the player and HUD were visible without overlap. [Validation results](validation/2026-10-09-results.json) retain the evidence, while [M0_STATUS](M0_STATUS.md) records the remaining acceptance criteria.

Keep the gameplay direction: combat/exploration/extraction feeds home and colony progression. Expand in small verified slices. Banked salvage now buys a settlement (storage, workshop, charger), a hauler and a digger; the digger drains the Drowned Vault water site for a one-time salvage reward ([WATER_SITE](WATER_SITE.md)). Next design decisions concern progression beyond one free node, richer encounters (enemies that use or avoid water), water structures (gates, dams, pumps), making procedural terrain navigable, and a second site where water is a weapon or a resource rather than an obstacle. Factions, broader persistent world entities, multiplayer and endgame remain unimplemented.

Full M0 needs worker/snapshot architecture, multi-chunk rendering/navigation ownership, an equivalent Actor/Mass benchmark, Windows packaging and measured RTX 5050 performance at 1080p/60 FPS. Lumen, hardware ray tracing and virtual shadow maps are currently disabled; this is configuration, not measured acceptance.

Track canonical source, configuration, saved content, original art, scripts and durable documentation. Keep mirrored Source, tools, Binaries, Intermediate, Saved, DDC and transient build evidence ignored. Update controls, schema, module boundaries and evidence as they change. Read AGENTS.md before editing, and distinguish current code, verified behavior and future requirements.

# Prototype status — 2026-10-09

DOWNSTREAM now has a saved M1 expedition level and four compiled native modules. The owner prioritized a working gameplay concept before M0 benchmarking. M0 remains open while the small M1 loop progresses. [PLAYABLE_CONCEPT](PLAYABLE_CONCEPT.md) describes behavior and future handoff.

Local environment: Unreal 5.8.3, Blender 5.2.2, Visual Studio Community 2026, MSVC 14.44.35229, Windows SDK 10.0.26100.0, i5-13600K, 32 GB RAM and RTX 5050. Native source is active; the simulation connects to game worlds through its game-thread adapter.

## Confirmed evidence

- Original mesh generation/FBX round-trip validation passed: nine meshes, 3,424 triangles total, checked scale/materials/triangle counts, editable Blender source and manifests.
- Standalone production simulation tests passed 17/17 with strict C++17 compilation. The expedition economy suite passed collection/capacity, banking, death, skill selection, restore validation and overflow behavior.
- Native Editor and Game targets compiled with the installed Visual Studio toolchain. The latest engine automation passed all eight cases: six simulation and two combat-rule cases, with zero failures or skipped tests.
- M1 generation saved `/Game/Downstream/Maps/M1_Expedition` with six chasers, three shooters, three caches, extraction, boundaries, tuning/material references and four original synthesized cues. Navigation baking completed.
- Full M1 game-world verification passed movement (432.61 cm), camera/HUD, projectile kills and salvage drops, point-blank projectiles, melee, chaser pursuit/attack, shooter attack, home protection/heal, extraction/save, death/respawn/recovery and dodge. With `steady_aim`, projectiles dealt 28.75 damage; chaser/shooter attacks dealt 12/10. Dodge moved 290.32 cm and passed protection-expiry/cooldown checks.
- Isolated cross-process reload restored two banked salvage and `steady_aim`, with nine enemies and three caches reset. The normal owner save was not used. Death recovery restored the two carried units dropped in the current-run bag.
- Current M0 native regression passed movement (452.62 cm), fixed camera/HUD, deferred queued edits, invalid edit rejection, one rebuild for each applied edit and unchanged rebuild count while idle. It recorded 19 ticks in 1.86685 game seconds, within the configured 10 Hz clock's tick-boundary tolerance; the short sample measured 10.1775 Hz.
- A real standalone M1 game window loaded DownstreamGameMode with no logged errors. The [reviewed 1280x720 frame](images/Downstream_Playable_Concept.png) shows the visible mannequin, home and health/carry/bank/starter HUD; objective, E prompt and control footer fit without overlap. This is visual/launch evidence, not GPU performance acceptance.
- Earlier M0 commandlet/movement checks established the saved base map, fixed camera, visible Epic character and navigation route. The initial standalone M0 frame is preserved at [Downstream_First_Prototype.png](images/Downstream_First_Prototype.png). This historical frame is launch evidence, not current M1 combat or performance evidence.

The implementation contains native ranged/melee/dodge combat, GAS health, two enemy behaviors, collection/extraction, respawn/recovery, a Canvas HUD and JSON bank/starter persistence. Generation and compilation establish content/code existence; runtime checks establish whether the complete flow works.

## Durable verification record

[2026-10-09 validation results](validation/2026-10-09-results.json) preserve the build/test/runtime summary and canonical source hash. The reviewed M1 screenshot is preserved under `docs/images/`. Detailed generated logs and reports remain ignored under `build/`. These checks establish the delivered gameplay slice and technical station; full M0 acceptance still requires the work below.

## Still required for full M0 acceptance

- Define immutable snapshots, command synchronization and worker ownership before moving simulation off the game thread.
- Extend the single dirty terrain station to central multi-chunk consumption/render dispatch, and validate ownership. Terrain collision and local navigation invalidation remain unimplemented.
- Run equivalent 100-unit Actor versus Mass benchmarks and record the decision. AdaptivePathfindingSystem is installed/enabled, but current enemies use Recast/AIController; no plugin performance comparison is established.
- Package Windows and profile representative gameplay with Unreal Insights at the RTX 5050 / 1080p / 60 FPS target.

The renderer updates a non-colliding 32x32 developer patch from dirty metadata. It does not provide a worker-thread world system. Maps explicitly rebuild navigation at startup; this is a prototype prerequisite, not local invalidation acceptance. Lumen, hardware ray tracing and virtual shadow maps are disabled, with no measured 60 FPS claim.

## Scope boundary

M1 contains combat/exploration/extraction, a home settlement with a hauler, and the Drowned Vault water site with a digger (schema 3 persists bank, skill, structures, robots, dug terrain and vault state). Carry, enemies, pickups and recovery bags reset on launch. Factions, full progression, broader persistent world entities, multiplayer and endgame remain unimplemented. Robots use the unrigged static salvage-bot asset.

Water (2026-10-09 evening): `WaterGrid` is a conserved virtual-pipe simulation on the game thread at 10 Hz, 640 cells, with no sleeping. It is not yet the chunked, worker-owned, active-cell system the brief targets. The site's procedural terrain has collision and rebuilds on edit (`FNavigationSystem::UpdateComponentData` is called), but it does not appear in the Recast navmesh in this build, so local navigation invalidation is still unproven. See [WATER_SITE](WATER_SITE.md).

Original meshes/audio and Epic template resources supply the level. Harvest/Ready Map packs are absent from inspected payloads; the separate UE4.27 Shooter Game sample is not imported. Missing packs do not block this prototype. The locally enabled Adaptive plugin is optional to core gameplay and must be installed or disabled on another machine.

M0 has no final sign-off, packaged acceptance or Actor/Mass decision. Current gameplay priority and remaining technical work are both retained in this handoff.

# The Frontier (design v0.4 addendum)

Status: playable, 2026-10-10. Map `/Game/Downstream/Maps/M2_World`, reached from the main menu.

The v0.3 brief asks for a world that is physically changeable (3.3), settlements that control water and fortify sites (3.7), and "weaponized water" that uses the same simulation as everything else (9.2). The Drowned Vault proved the water loop on one hand-built site. The frontier is the next step: a seeded world where cutting a canal is the main way to crack an enemy base, and the enemy fights back over the water.

## The loop

1. **Scout.** Each seed makes a 112 x 88 m frontier: two rivers running from springs to a drainage edge, your home clearing on dry high ground, up to three Dam Cult outposts, one drowned vault, ten salvage caches, six patrols and Kenney props.
2. **Gear up.** Bank salvage, build the workshop, assemble the digger (as on the training grounds).
3. **Breach.** Every outpost sits in a bowl 1.3 m *below* its river, kept dry by a 3 m earth levee. Plan a canal from the river through the levee (G, drag). The digger cuts it one cell at a time.
4. **Flood.** The river pours in. Structures standing in more than 0.25 m of water lose integrity (8 per metre above that, per second; huts have 70, the stash 110, the generator 300). Each collapse spills salvage: stash 12, generator 6, huts 3.
5. **They respond.** The alarm sounds, the light strobes. Two engineers posted on the river side of the pad run to the breach and sandbag it (+0.4 m per action, every 1.1 s, up to the rim). Guards and riflemen leave their posts to hunt the digger if it works within the outpost radius + 14 m, unless you are in their face (4.5 m). The generator pumps the bowl out (0.35 m3/s) while it stands.
6. **Hold or lose it.** Kill the engineers, or drive the canal right under the generator, and the outpost drowns. Let them seal it, and the pump wins. Destroy the generator (by water, or gunfire at 35% damage) and the outpost is razed for good: no pump, no engineers, no reinforcements. Any huts or stash still standing can be flooded or shot for their salvage.
7. **Recover.** A wrecked digger reassembles at the workshop after 25 s for 4 salvage. It heals at home. Its health bar is on the HUD.

The risk and reward is in one place: the outpost's own salvage is the prize, and the only cheap way to get it is to put your digger in front of the garrison.

## Rules worth knowing

| Rule | Value | Tuning field |
| --- | --- | --- |
| Outpost respawn (per slot, player > 15 m away) | 45 s; engineers 22 s while breached | `OutpostRespawnSeconds` |
| Engineer health | 45 | `EngineerHealth` |
| Sandbag interval | 1.1 s | `DamActionSeconds` |
| Gunfire damage to structures | 35% | `StructureGunfireScale` |
| Digger threat radius beyond the outpost | 14 m | `DiggerThreatRadius` |
| Digger health / rebuild / cost / self-repair | 150 / 25 s / 4 / 12 per s at home | `DiggerHealth`, `DiggerRebuildSeconds`, `DiggerRebuildCost`, `DiggerRepairPerSecond` |
| Flood threshold, damage per metre per second, pump rate, dam step | 0.25 m, 8, 0.35 m3/s, 0.4 m | `downstream::OutpostRules` (DownstreamSim) |
| Structure integrity: hut / stash / generator | 70 / 110 / 300 | `downstream::OutpostRules` |

Trench dodge: Space in a trench deeper than 0.5 m scrambles you up the bank instead of rolling.

## Generation

`downstream::GenerateWorld(seed)` (DownstreamSim, plain C++) builds the layout in one pass: value-noise terrain with a drainage edge, rivers carved downhill, home on a dry plateau, outposts stamped below a river bed with a levee ring, a drowned vault bowl fed by a weak spring with a spillway route, then caches, patrols and props. Placement rules keep outposts at least 17 cells from any spring and reject vaults whose spillway would cross reserved ground. A failed attempt retries with a derived terrain seed (and fewer rivers if needed), up to eight times.

`tests/world_tests.cpp` checks 80 seeds: determinism, a flat dry home, every outpost below its river and dry at rest, the vault flooded > 0.8 m, rivers reaching sinks and water volume conserved. On seeds 1 to 6 a levee breach must flood past 0.4 m, drop at least two structures, be sealed by engineers, and the pump must then lower the flood. The balance test cuts canals on seeds 1 to 8. A cut that stops two cells short on the pad: unopposed it razes 7 of 8 outposts within three minutes, but engineers arriving 12 s after the breach seal it every time. A cut driven right under the generator beats the engineers 7 times out of 8. So either aim the canal at the generator (a longer, riskier dig) or kill the engineers. Run `scripts/test_world.ps1`.

## Runtime

- `UDownstreamWaterSubsystem::ActivateWorld` generates the layout, centres it on the site actor, restores destroyed structures from the save, warms the water up for 120 s, and gives the expedition subsystem the home and build-grid locations.
- `ADownstreamWaterSite` renders 16 x 16 cell chunks; a dig rebuilds mesh, collision and navigation only for touched chunks. Invisible walls box the world in.
- `ADownstreamWorldDirector` spawns the extraction point, settlement manager, outposts, patrols (registered for field respawn), caches and props.
- `ADownstreamOutpost` owns the structures and the garrison (`ADownstreamRaider`: guard, rifleman, engineer, tinted mannequins).
- Saves: `Saved/Downstream/world-save.json`, schema 4. It adds `earthworks.seed`, `vaults_opened` and `outposts: [[id, destroyed_mask]]`. The training grounds keep their own `prototype-save.json`.

## Menus and options

`MainMenu` shows a live preview frontier under an orbiting camera. Continue, New frontier (seed box, random), Training grounds, Settings, Controls, Quit. Settings: volume, window mode, resolution, quality preset, FPS counter, HUD hints (saved in GameUserSettings, section `[Downstream.Options]`). Esc in game opens the same menu as a pause menu.

## Presentation and audio

- `M_WorldGround`: wet dirt in low ground and fresh cuts, sand plains, rocky ridges and levee rims, faint contour bands.
- `M_WaterFlow`: two panning ripple normals (generated `T_WaterRipple_N`) that strengthen and foam with flow speed. Also used on the training grounds.
- Russo One (SIL OFL 1.1) for titles.
- Six synthesized cues (`scripts/synth_frontier_audio.py`): alarm klaxon, collapse, sandbag thud, levee breach gush, running-water loop (louder the more fast, deep water is near you), digger wreck.

## Verification

`scripts/verify_frontier.ps1` runs three isolated PIE sessions with their own save folder:

- **menu**: the preview world builds and the menu controller runs without a pawn.
- **frontier** (seed 3): generation and garrisons, workshop and digger, a real robot canal from the river onto an outpost pad, its engineers reaching the breach and sandbagging it, a second canal into another outpost (garrison held still) that floods it, collapses buildings and drops salvage, guards targeting and chasing the digger, wreck and paid rebuild, then the v4 save.
- **reload**: the seed, dug terrain, destroyed structures and the digger come back.

## Known rough edges

- Outpost bowls are often navmesh islands behind their levees. Raiders that make no headway on the navmesh for 2 s "scramble" straight over the terrain for a few seconds, and engineers do the same to reach the bank beside a breach. It works, but it looks a bit like climbing.
- The digger occasionally wedges while crossing onto a fresh canal (seen once in about ten verification runs). Clearing the queue (C) and replanning frees it.

## Next

- Raider counter-play beyond sandbags: engineers cutting a relief channel toward *your* home.
- Weather: rain pulses on the springs so a sealed levee can still overtop.
- Gates and pumps the player can build, using the same pump rule as the generator.
- More outpost kinds (a dam that holds a reservoir above your home instead of below a river).

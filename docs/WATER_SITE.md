# Drowned Vault: water, earthworks and the digger

Added 2026-10-09 (evening). This is the first slice of the design brief's M3/M4 terrain and water work, wired into the existing M1 loop so it matters to play rather than being a sandbox. Per the brief, the canal is not the objective; the vault is.

## What the player does

The south-east of `M1_Expedition` is a raised plateau with a bowl-shaped basin. A burst water main (east) feeds the basin through a ditch; an east spillway holds the basin at 1.4 m, so the **sealed vault** on the basin floor sits under about 1.3 m of water. An **old culvert** on the south edge drains anything that reaches it.

1. Bank salvage, build the workshop (B, 2), then assemble a **digger** at home (B, 5; 10 salvage).
2. Press **G** for the dig planner. Drag with the left mouse from the water to where it should go (the culvert). Green markers mean the line is valid; release to queue it. RMB cancels a drag, a second RMB or G closes the planner, **C** clears the queue. Up to three canals queue.
3. The digger drives out and cuts the canal one step at a time, piling spoil into a berm on the left of travel. Water follows the new channel to the culvert.
4. When the water over the vault is 0.35 m or less, **E** opens it: 16 salvage (four pickups of 4) to carry home.

Canals are graded straight from the ground height at the first cell to the height at the last cell. A line that ends on the plateau digs nothing below the plateau, so it does not drain the basin. The designated line is the whole design: start in the water, end lower.

Standing water slows everyone the same way (player, enemies, digger): 1 m of water gives 0.45x speed (`WadeSpeedAtOneMetre`), with a floor of 0.25x. The HUD shows a wading banner. F9 toggles the developer flow overlay (depth dots and flow arrows).

## Simulation (DownstreamSim, plain C++)

`DownstreamWater.h` / `WaterGrid`: virtual-pipe shallow water on a cell grid. Each cell has ground height and water depth in metres; flux lives on cell edges, so whatever leaves one cell enters its neighbour. Volume is conserved except for explicit, counted sources, sinks, edge drainage, evaporation and direct `SetWater` edits (`TotalWater == sourced + added - sunk - drained - evaporated`). Fixed 0.1 s ticks with 4 substeps; `Advance` caps catch-up at 8 ticks per call so a frame hitch cannot stall the game thread. The constructor rejects substep sizes that would be unstable. Ground edits mark 16x16 render chunks dirty and bump a revision counter.

`DownstreamEarthworks.h`:

- `BuildDrownedVaultSite` / `DrownedVaultBaseGround` define the 32x20, 1 m-cell site. Prefill starts the basin at its flooded steady state so a fresh game needs no warm-up.
- `RasterizeCanal` returns 4-connected cells (water cannot pass a diagonal-only gap).
- `DigJob` grades a path, lowers one cell per `WorkNext` by `StepDepth`, and moves the spoil onto the side cell (left, then right; never onto the path or a sink). Spoil with nowhere to go is counted as discarded.
- `EarthworksState` holds digger ownership, vault state and terrain deltas (only cells that differ from the generated site, in millimetres).

Not implemented yet, by design for this slice: active-cell sleeping (the grid is 640 cells, about 10k cell updates per tick), gates/dams/pumps as structures, rainfall/seasons, hardness, worker-thread ownership.

## Unreal integration

| Piece | Module | Role |
| --- | --- | --- |
| `UDownstreamWaterSubsystem` | Downstream | Owns the `WaterGrid` and dig queue on the game thread; ticks at 10 Hz; world/cell conversion; canal validation and designation; dig actions; vault interaction; periodic terrain saves (every 10 s while dirty, on canal completion, best effort at shutdown) |
| `ADownstreamFloodedVault` | Downstream | Transient presentation spawned by the subsystem; label and beacon show sealed / open / empty |
| `ADownstreamWaterSite` | DownstreamRender | Placed in M1 at (300, -2600, 0). Procedural terrain (collision, vertices at cell centres plus a skirt ring down to the slab) and translucent water surface rebuilt per water tick. Canal plan and queued-cell markers. F9 overlay. Builds the base site in `OnConstruction` so the saved map shows it |
| `ADownstreamDigger` | DownstreamColony | Spawned by the settlement manager when owned. Navigates to the site edge, then drives directly over terrain to the previous (already dug) canal cell and digs the next one within `DigReach`. Hops if wedged on a wall |
| Player | DownstreamCombat | G/LMB-drag/RMB/C planner input, 5 assembles the digger in the builder, E tries the vault before normal interaction; wading slowdown lives in `ADownstreamCombatCharacter` |

Navigation note: the procedural terrain has collision, but in this build it does not contribute to the Recast navmesh (projection onto the plateau fails). The digger therefore uses navigation only to reach the site edge. Enemies do not path onto the plateau. Making the site navigable (static mobility, or exporting the terrain to nav explicitly) is the next technical item, and is the same "local navigation invalidation" work M0 lists.

Triangle winding: front faces are A -> +Y -> +X seen from above (`{A, C, B}, {B, C, D}` for a quad where B is +X and C is +Y). The opposite winding renders and traces fine but character sweeps fall through or slide off.

## Save schema 3

Schema 3 adds one object to schema 2. Schemas 1 and 2 load with an untouched site and are rewritten as 3 on the next successful progress write.

```json
"earthworks": {
  "digger_built": true,
  "vault_opened": false,
  "terrain_mm": [[15, 8, 87], [14, 8, 100]]
}
```

`terrain_mm` entries are `[x, y, height_mm]`, inside the 32x20 grid, unique, within 0..4000 mm. A digger requires the workshop. Invalid earthworks block progress writes and preserve the file, like every other invalid save. Digger construction and vault opening commit to disk before memory changes. Water depth is not saved; the site re-settles for 90 simulated seconds on load. Unfinished canal designations are not saved (dug ground is).

## Tuning (`DA_PrototypeTuning`, Earthworks category)

`DiggerCost` 10, `DiggerMoveSpeed` 380, `DigActionSeconds` 0.55, `DigStepDepth` 0.3 m, `DigReach` 230 cm, `MaxQueuedCanals` 3, `VaultReward` 16, `WadeSpeedAtOneMetre` 0.45, `WaterSiteOrigin`. Site geometry lives in `DrownedVaultSite` constants in `DownstreamEarthworks.h`; changing it changes the puzzle, so re-run `test_water.ps1`.

## Verify

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test_water.ps1     # 15 headless tests, incl. the full puzzle
powershell -ExecutionPolicy Bypass -File scripts/verify_water.ps1   # real game world, isolated saves
```

`verify_water.ps1` runs three isolated processes: `legacy` (schema 1 loads into a flooded site; planning needs a digger), `flow` (flooded start, wading slowdown, digger purchase and v3 save, real robot digs the culvert canal, water drains, vault pays 16 once, terrain saved), and `reload` (schema 3 restores digger, opened vault and the drained canal). The flow test shortens `DigActionSeconds` to 0.12 for pacing; every dig still goes through the robot and subsystem.

## Verified 2026-10-09

All suites passed on the committed map: 17 simulation, expedition, settlement, 15 water and 8 native automation tests; `verify_settlement` (5 modes, including the write-failure case), `verify_water` (3 modes), `verify_expedition` (play + reload) and `verify_play` (M0). In the flow run the digger cut the 18-cell culvert canal itself in 70 dig actions (56.5 s at test pacing, 4 wall hops), 41 m3 drained through the culvert, the vault paid 16 once and 32 terrain cells were saved; a fresh process restored them with the vault still drained. Wading in 0.45 m of water cut player speed from 550 to 413. Frames: [flooded](images/Downstream_Water_flooded.png), [drained](images/Downstream_Water_drained.png). Evidence: [validation/2026-10-09-water-results.json](validation/2026-10-09-water-results.json).

The verify_expedition combat arena moved 8 m west (same distances) because its old spot is now the site's plateau.

Rendering note: the procedural mesh's vertex colours did not reach the materials in this build (the ground rendered black), so `M_SiteGround` and `M_Water` read data from UV0 instead (ground: dug/piled metres and height; water: depth and flow speed). Both materials are generated by `create_expedition_level.py`.

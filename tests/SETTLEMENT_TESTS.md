# Settlement state and persistence handoff

Run `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test_settlement.ps1` from the repository root. The existing portable Zig compiler builds real standard C++17 state with `-Wall -Wextra -Werror -pedantic-errors`. Output is `build/settlement/settlement_tests.exe`; Unreal is not required.

The test suite compiled against interface-only implementations and produced 25 failing behavior assertions before the implementation. The same suite then passed, as did the prior expedition suite and all 17 simulation tests. An additional restore regression first failed, then passed after clearing the unsaved storage capacity bonus during a legacy expedition restore. This records observed pure-state verification; Unreal builds, JSON/file operations and world/navigation behavior require the serialized root integration verification.

Coverage includes exact spending and insufficient funds; positive prices; bounded grid placement; one structure per type and cell; storage capacity stacking with starter skill and overflow saturation; all three robot prerequisites; one robot; whole-fit positive cargo; cargo delivery without duplication or bank overflow; hauling preference; finite bounded battery updates; and restore rejection for duplicate types/cells, unknown types, out-of-grid cells, robot prerequisite inconsistencies, cargo without a robot, invalid cargo and invalid battery. Invalid restore requests preserve the existing settlement.

`downstream::SettlementState` owns structures and robot state. `ExpeditionState` owns bank/carry/skill and its derived storage capacity bonus. The engine adapter copies both candidates, commits the combined save, then updates memory. UI and Colony actors issue requests through `UDownstreamExpeditionSubsystem` and read its getters. They do not own economy state.

## Runtime contract

- `GetBuiltStructures()` returns `Type`, `GridCell` and computed `WorldLocation`; `HasStructure`, `GetStructureCost`, `GetHaulerCost` and `GetHaulerCargoCapacity` read authoritative values.
- `GetSnappedBuildLocation` and `GetBuildCellWorldLocation` use tuning's grid center/cell size. `CanPlaceStructure` checks affordability, bounds, uniqueness and world collision. `TryPlaceStructure` also requires a valid player in the protected home radius. Its 90/90/95 cm overlap box is centered 100 cm above the ground cell so the home pad/ground does not block placement; blocking props and pawns do.
- `TryConstructHauler` requires the player at home, all structures and sufficient bank. `OnSettlementChanged` is a zero-argument dynamic multicast and broadcasts successful building/robot construction to synchronize presentation actors.
- `SetHaulingEnabled` commits preference. `TryLoadHaulerCargo` requires the built robot, same-world actors, the robot's `DownstreamHauler` tag, proximity within 150 cm, whole-fit cargo and a successful pickup claim. Save failure releases the claim; success commits cargo before destroying the pickup.
- `TryUnloadHaulerCargo` requires the tagged robot within 220 cm of built storage. A saved bank/cargo transaction completes delivery; save failure retains both values.
- `UpdateHaulerBattery` accepts finite signed changes and clamps to 0..100. `SaveHaulerCheckpoint` writes only when battery changed; Colony calls it every five seconds and on normal shutdown. Other successful progress writes also checkpoint the current battery.

## Save schema 2

Default location remains `<project>/Saved/Downstream/prototype-save.json`; absolute `-DownstreamSaveDir` overrides isolate runtime verification. Do not remove owner saves. The `.tmp`/`.bak` replacement/recovery contract from `EXPEDITION_TESTS.md` remains.

```json
{
  "version": 2,
  "banked_salvage": 27,
  "selected_skill": "steady_aim",
  "structures": [
    {"type": "storage", "cell_x": -1, "cell_y": 0},
    {"type": "workshop", "cell_x": 0, "cell_y": 0},
    {"type": "charger", "cell_x": 1, "cell_y": 0}
  ],
  "hauler": {"built": true, "battery": 26.5, "cargo": 4, "hauling_enabled": true}
}
```

Fields must have their JSON types. Version, bank, grid cells and cargo are finite whole numbers in their signed 32-bit domain; bank/cargo are nonnegative. Structure names are exactly `storage`, `workshop`, `charger`, each once, with unique in-bounds cells. Battery is finite and inclusive 0..100. Robot ownership requires all three structures, cargo must fit configured capacity, and an unbuilt robot cannot hold cargo or have hauling enabled. Unknown additional fields are ignored.

Schema 1 retains its existing bank/skill contract. It loads an empty settlement and rewrites schema 2 on the next successful progress write; merely loading does not alter the file. Malformed, future, unknown or inconsistent saves leave safe defaults and block all progress writes while preserving the original. Existing skill and extraction writes include settlement data, preventing them from overwriting schema 2 progress.

Carried player salvage, enemies, field pickups and recovery bags still reset on relaunch. Saved robot cargo survives and Colony respawns the robot at home. Robot position, field depletion, offline hauling and full world persistence are outside this slice. File replacement uses Unreal file operations with a backup rather than guaranteeing power-loss atomicity.

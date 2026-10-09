# Expedition economy and persistence handoff

Run `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/test_expedition.ps1` from the repository root. This uses the existing portable `.tools/zig-x86_64-windows-0.17.0/zig.exe`, C++17, and `-Wall -Wextra -Werror -pedantic-errors`. It needs no Unreal installation. The binary is written to `build/expedition/expedition_tests.exe`.

The behavioral test suite was first compiled against an interface-only state implementation and failed on the expected collection, extraction, recovery, skill, and restore behaviors. The implementation then passed the same suite. Tests exercise real `downstream::ExpeditionState` code in `native/Source/DownstreamSim/Public/DownstreamExpeditionState.h`, without engine mocks.

Coverage:

- Positive, zero, and negative pickup quantities; exact capacity; oversized pickups; full inventory.
- All carried salvage transfers to the stash on extraction, repeated empty extraction cannot duplicate it, and later extractions accumulate.
- Death returns exactly the carried amount for a recovery bag, clears carry, preserves stash, and cannot duplicate the same death drop.
- Exactly one free starter node: `steady_aim` grants +15% ranged damage; `salvage_handling` grants +4 carry slots; invalid IDs are rejected; repeated selection of the same node is idempotent.
- Version 1 save restore, known/empty skill IDs, unsupported versions and IDs, negative/oversized counts, rejection without state mutation, and carry reset on a valid restore.
- Integer overflow when banking or adding capacity, including preservation of inventory/stash when banking is rejected.

## Runtime API

`UDownstreamExpeditionSubsystem` is a game/PIE world subsystem in module `Downstream`. The plain state owns quantities and skill effects; the engine adapter owns actor interaction and durable save commits. UI reads the getters and submits requests. It must not maintain its own inventory/stash.

| API | Contract |
| --- | --- |
| `static const UDownstreamPrototypeTuning* GetTuning(UWorld*)` | Loads optional `/Game/Downstream/Data/DA_PrototypeTuning`; defaults are always usable. No hard constructor asset references. |
| `int32 GetCarriedSalvage() const` | In-run salvage, not written to disk. |
| `int32 GetBankedSalvage() const` | Salvage saved at home. |
| `int32 GetInventoryCapacity() const` | Base tuning capacity plus selected node bonus. |
| `float GetRangedDamageMultiplier() const` | `1.15` for `steady_aim`, otherwise `1.0`. |
| `FName GetSelectedSkillId() const` | `NAME_None`, `steady_aim`, or `salvage_handling`. |
| `bool ChooseStarterSkill(FName)` | Picks and saves the one free node. A failed save leaves selection unchanged. No classes, XP, or campaign unlocks. |
| `bool IsAtHome(FVector) const` | Inclusive horizontal home radius from tuning. |
| `bool TryInteract(AActor* Player)` | Claims and collects nearest unclaimed pickup within 220 cm whose entire amount fits. Otherwise, at home, commits all carried salvage to the save/stash. No automatic extraction. |
| `void HandlePlayerDeath(FVector)` | Spawns a recovery bag before clearing carry. Spawn failure keeps carry. Stash survives death. |
| `ADownstreamSalvagePickup* SpawnSalvage(FVector,int32,bool=false)` | Positive salvage quantities only; final flag marks a current-run recovery bag. |
| `FText GetStatusMessage() const` | Short feedback, including invalid-save and failed-write status. |

`ADownstreamSalvagePickup` exposes editable `Amount` and `bRecoveryBag`. Claim happens before state collection; success destroys the actor. Collision is disabled. Teal salvage and gold recovery labels/lights plus slow bob/rotation make interaction readable. The original `/Game/Downstream/Art/SM_Crate` and `SM_ScrapStack` meshes are used if present, with an engine cube fallback. Optional materials are `/Game/Downstream/Materials/M_PrototypeSalvage` and `M_PrototypeRecovery`.

`ADownstreamExtractionPoint` is a presentation-only home cylinder, light, and label. At BeginPlay it aligns to tuning `HomeLocation`. It never banks on overlap. Optional home material is `/Game/Downstream/Materials/M_PrototypeHome`.

`UDownstreamPrototypeTuning` exposes all M1 health, speed, damage, cooldown, projectile, enemy, respawn, inventory, and home values as editable data asset properties. `ShotSound`, `HitSound`, `PickupSound`, `ExtractionSound`, `EnemyMaterial`, and `ProjectileMaterial` are optional object references. The C++ class defaults are a complete playable fallback. Change the data asset to adjust balance without changing combat code.

## Save format and file commits

Default runtime file: `<project>/Saved/Downstream/prototype-save.json`.

```json
{
  "version": 1,
  "banked_salvage": 27,
  "selected_skill": "steady_aim"
}
```

`banked_salvage` must be an integer from 0 through 2,147,483,647. `version` must be 1. `selected_skill` must be empty, `steady_aim`, or `salvage_handling`. Malformed JSON, missing required fields, fractions, unsupported versions, and unknown skill IDs are rejected. Unrecognized additional fields are ignored.

Save commits write a `.tmp` sibling first, preserve the previous primary as `.bak`, then replace the primary. Inventory/stash or skill state is committed in memory only after file operations succeed. Failed banking keeps all carried salvage. If a previous interrupted replace left only the backup, it loads visibly from `.bak`. A malformed primary or backup is preserved, logs its path, starts with safe defaults, and disables further save commits for that session. Repair it or move it aside while the game is stopped, then restart. The code never silently deletes an invalid save.

For runtime test isolation, launch with `-DownstreamSaveDir="C:/absolute/test/directory"`. The directory must be absolute; its `prototype-save.json`, `.tmp`, and `.bak` are used together. A relative or empty override is ignored. Root gameplay verification uses `build/expedition-test-save` as an absolute directory so the owner's default save is untouched.

## Prototype limitations and verification boundary

This is a local, single-player M1 loop. The selected free node and banked stash survive relaunch. Carried salvage, recovery bags, pickups, and enemies reset on a fresh run; no inventory is silently banked by quitting. There is no multiplayer save locking, cloud sync, currency spending, skill tree, or campaign progression in this slice. Save replacement uses Unreal's file operations with a backup, not a power-loss transaction guarantee.

These standalone tests verify economy state behavior. They do not verify Unreal reflection/compilation, visual placement, sounds, actor claim/destruction, JSON parsing, file permissions, or the full playable combat-to-extraction flow. Those require the root agent's serialized Unreal build and runtime verification. Do not run concurrent editor builds against the active project. Canonical editable sources are `native/Source`; `unreal/Downstream/Source` is the staged build copy.

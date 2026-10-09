# Unreal native simulation automation

The native suite compiles and executes the production `downstream::Simulation`
inside the `DownstreamSim` Unreal module. The suite uses Core automation only;
it creates no Actors, maps, render resources, or presentation adapters.
`WITH_DEV_AUTOMATION_TESTS` excludes registration when development automation is
disabled. The tests use `EAutomationTestFlags_ApplicationContextMask` and
`EAutomationTestFlags::EngineFilter`, so they can run in the editor or its
headless command-line process.

Canonical source is
`native/Source/DownstreamSim/Private/Tests/DownstreamSimAutomationTests.cpp`.
After changing it, copy/activate the canonical source and rebuild the native
Editor target before running automation; an existing editor binary does not
discover newly added tests. The project `unreal/Downstream/Source` directory is
the generated build copy, rather than the source to edit.

Run from the repository root after a successful native Editor build:

```powershell
. .\scripts\project_paths.ps1
$NativeReport = Join-Path $DownstreamRoot 'build\native-automation'
& $DownstreamCommandlet $DownstreamProject -unattended -nop4 -nosplash -NullRHI `
    '-ExecCmds=Automation RunTests Downstream' `
    '-TestExit=Automation Test Queue Empty' `
    "-ReportExportPath=$NativeReport" -log
```

The complete Downstream filter now runs eight cases: six simulation cases below
and two DownstreamCombat rule cases. scripts/test_native.ps1 checks the exported
report for at least eight successes and zero failures. Filtering only
Downstream.Simulation runs six cases. A successful process exit alone does not establish that tests
were discovered. To run one test, replace `Downstream` in `Automation RunTests`
with its full name below.

| Test | Observable contract |
| --- | --- |
| `Downstream.Simulation.FixedTickPartitions` | Partial frame time survives; 30/60/144 FPS and jittered schedules produce 100 ticks in ten seconds; configured tick durations control completed elapsed time. |
| `Downstream.Simulation.TerrainCommands.FifoAtTick` | Commands wait for a tick, assign absolute heights in FIFO order, and are consumed once. |
| `Downstream.Simulation.TerrainCommands.Clamp` | Finite targets clamp to configured limits; an equal minimum/maximum range remains valid and produces no-op edits. |
| `Downstream.Simulation.TerrainCommands.Validation` | Invalid coordinates, non-finite heights, invalid time, tick overflow, and representative invalid configurations reject with the documented exception types; valid pending work survives rejected inputs. |
| `Downstream.Simulation.DirtyChunksOnly` | Initial partial-edge chunks are reported; edits dirty only their chunks without duplicates; consumption clears flags; exact and clamped no-op edits remain clean. |
| `Downstream.Simulation.SeedReproduction` | Same seeds reproduce every cell independently of another world; different seeds and coordinates affect terrain; generated heights stay bounded and finite, including extreme finite limits. |
| `Downstream.Combat.CooldownBoundary` | Repeated attacks respect exact cooldown boundaries; invalid intervals do not consume attacks. |
| `Downstream.Combat.FactionAndProtection` | Opposing living factions can receive damage; same factions, dead actors and protected targets cannot. |

Tick counts, target heights, and dirty chunk sets use explicit expected values.
Frame schedules and seed reproduction compare separate simulation instances to
check their public invariance contracts. No expected terrain values are
calculated with the generator under test, and no source text or mocks are
asserted.

These native tests complement the portable suite in `tests/README.md`. They
verify the pure module under Unreal's toolchain and automation runner, but do
not cover the world subsystem lifecycle, camera, terrain mesh generation,
navigation invalidation, water, save/load, or packaged GPU performance.

scripts/verify_expedition.ps1 separately exercises real combat Actors, movement,
both AI attacks, projectiles (including point blank), melee, dodge, home
protection/healing, collection/extraction, death/respawn/recovery and save reload.
scripts/verify_play.ps1 checks the world simulation/dirty terrain presentation.
These are development checks; packaged performance acceptance remains open.

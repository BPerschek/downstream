# Portable simulation behavior tests

Run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/test_sim.ps1
```

The test runner builds and executes real `DownstreamSim` production code with a
C++17 compiler. It does not start Unreal or require the Windows Unreal C++ SDK.
The portable compiler is a local development tool, not part of the shipped game.

For another compiler, the two compilation inputs are `tests/sim_tests.cpp` and
`native/Source/DownstreamSim/Private/DownstreamSimulation.cpp`. Add
`native/Source/DownstreamSim/Public` to the include search path. Do not
compile `DownstreamSimModule.cpp` in the portable executable: that small wrapper
registers the module with Unreal. The production simulation implementation itself has no Unreal includes. Native automation wrappers also use Core; see NATIVE_TESTS.md.

Coverage includes fixed-tick boundaries and elapsed simulation time; equivalent
30/60/144 FPS and jittered frame partitions; FIFO terrain edits applied only on
ticks; clamping; immediate rejection of invalid coordinates, non-finite heights,
non-finite/negative time and invalid configurations; partial chunk edges,
deduplicated dirty consumption and no-op edits; seeded bounded reproduction; and
one-cell/flat-height grids. Configurable tick durations, extreme finite height
ranges, and bounded handling of enormous frame durations are covered too.

`ElapsedSeconds()` reports completed simulation ticks, excluding the partial
frame-time remainder. `QueueTerrainEdit()` assigns an absolute target height,
not a height delta. Coordinate errors throw `std::out_of_range`; invalid numbers
and configuration throw `std::invalid_argument`. The simulation object is
single-owner and is not thread-safe; an eventual engine bridge must serialize
commands and publish read-only snapshots when moving simulation to a worker.

M0 has no recurring per-tick systems yet, so a large valid `Advance` aggregates
the number of complete ticks and applies the existing terrain queue once at the
first boundary. It does not loop through millions of empty ticks. Durations that
would overflow the integer tick counter throw `std::overflow_error` before
changing state. This is not an offline catch-up implementation; adding recurring
systems will require a separate bounded stepping/catch-up policy.

Seeded height generation is intentionally a temporary independent-cell M0
fixture, not the final region/biome/ruin generator. Its clean standalone function
can move behind `DownstreamWorld` orchestration later. The suite does not prove
Unreal-native compilation, runtime integration, terrain rendering, Actor/Mass
performance, water behavior, save/load, or packaged GPU frame rate.

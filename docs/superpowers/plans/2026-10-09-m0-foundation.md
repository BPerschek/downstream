# DOWNSTREAM M0 Foundation Implementation Plan

**Goal:** Begin DOWNSTREAM with an inspectable Unreal test world, original art, and tested simulation code.

**Architecture:** Preserve the runnable Blueprint project while preparing native simulation and engine boundaries. Build the test map using editor Python automation and original Blender meshes.

**Tech Stack:** Unreal 5.8.3, Blender 5.2.2, standard C++17, Python, PowerShell.

**Spec:** ../specs/2026-10-09-m0-foundation-design.md

## Constraints and review focus

- Fixed isometric camera, low-poly 3D, Windows, RTX 5050 baseline.
- No invented campaign, multiplayer, full water or colony subsystem.
- Missing native compiler must not make the active project unopenable.
- Verify coordinate/unit conversion and template package dependencies.
- Preserve unsaved editor work and existing configurations.
- Test accumulated time under different frame schedules and invalid commands.
- Do not mistake metadata for available asset payloads or a smoke test for GPU profiling.

## Tasks

- [x] Preserve configuration; add cache ignores, asset provenance, and decision log.
- [x] Author art/create_blockouts.py; save art/Downstream_Blockouts.blend and art/exports/SM_*.fbx. Export -Y forward, +Z up; verify dimensions and face counts in a JSON manifest.
- [x] Write native simulation behavior tests first, run against an intentionally incomplete implementation, then implement fixed-tick/command/grid/seeded contracts and run the headless executable.
- [x] Prepare DownstreamSim and Downstream native module/target files under native/Source outside the runnable project, without registering them in the active descriptor until an Unreal compiler is installed. Reserve World/Render boundaries for actual engine integration rather than empty modules. Clear stale generated native target metadata when returning to Blueprint.
- [x] Copy required installed Epic Blueprint template assets with Characters, LevelPrototyping and Cursor mount folders, then run scripts/create_test_world.py through UnrealEditor-Cmd -run=pythonscript. Import props; create low-cost materials, ground, clearing, ruin, lighting, player start and navigation volume; save /Game/Downstream/Maps/M0_TestWorld. Build/save navigation in a separate hidden editor and provide the prototype's runtime startup rebuild in the launcher.
- [x] Run a commandlet map/dependency verification and a game-mode smoke run using the documented prototype startup navigation command. Capture a rendered preview where possible.
- [x] Record test evidence, unresolved M0 tasks, launch/regeneration/native activation instructions, and review all authored changes.

## Simulation interface

Namespace `downstream`. `Config` contains Width, Height, ChunkSize, TickSeconds, MinimumHeight and MaximumHeight. `Simulation(Config, uint32_t seed)` exposes `Advance(double)`, `QueueTerrainEdit(int x, int y, float height)`, `TickCount()`, `ElapsedSeconds()`, `HeightAt(int x, int y)`, `ConsumeDirtyChunks()` and `Seed()`. `ChunkCoord` contains X and Y. Seeded heights belong to a separate generator function. Tests use real production code, not source-string assertions or mocks.

## Verification commands

Portable tests: `powershell -ExecutionPolicy Bypass -File scripts/test_sim.ps1`.

Map generation: `powershell -ExecutionPolicy Bypass -File scripts/regenerate_world.ps1`.

Game launch: `powershell -ExecutionPolicy Bypass -File scripts/play.ps1`.

Native build: `powershell -ExecutionPolicy Bypass -File scripts/build_native.ps1`; preflight checks the missing toolchain and fails without modifying the descriptor when unavailable.

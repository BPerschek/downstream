# M0 native integration continuation

The owner installed Visual Studio and requested continued development despite difficulty adding asset packs. This continues the technical foundation; the original game's combat/extraction direction remains unchanged.

## Deliverable

Compile and activate the existing native simulation, expose its live fixed tick through a small debug HUD, and add one 32x32 procedural mesh spike driven by queued height commands. The patch is a developer exhibit with collision and navigation disabled. It does not implement the terrain/water gameplay milestone or redefine the game around digging.

## Boundaries

- DownstreamSim remains standard C++ data with no Actor/render dependency.
- Downstream lifecycle adapter owns a game-thread simulation, translates validated commands and provides same-thread read-only presentation access.
- DownstreamRender owns the procedural mesh, debug controls and HUD; rendering reads authoritative data and queues edits.
- A single spike actor consumes dirty metadata. Full chunk-manager/snapshot ownership and worker execution remain future M0 work.
- The height display uses an explicit visual scale; these are test fixture values, not final terrain units or balance.

## Work and evidence

- [x] Verify installed compiler/SDK and compile existing Editor/Game targets.
- [x] Add six Unreal automation cases exercising actual simulation behavior.
- [x] Prepare renderer/HUD and dirty-only terrain spike source.
- [ ] Compile the combined native modules and run all Downstream Unreal automation tests headlessly.
- [ ] Create vertex-color material and place the native debug actor in the saved map.
- [ ] Verify native tick rate against game time, deferred height application, rejection of invalid edits, one dirty rebuild and stable idle rebuild count.
- [ ] Verify gameplay movement/camera still work and inspect a rendered frame.
- [ ] Record current status and asset availability accurately.

## Asset setup

Project folder was outside Launcher discovery roots; add the parent with a backup. AdaptivePathfindingSystem is an engine plugin, so install and project activation are separate. The Shooter Game sample is UE4.27 and must be inspected/migrated selectively before use. Do not copy an entire legacy gameplay framework into the native project. Missing art packs do not block this native technical work.

## Native source workflow

Canonical source remains native/Source. Build activation synchronizes it into the active project Source, protects separately edited active files using the last-copy hashes, registers native modules only for a working build, and preserves the prior descriptor on failure. Source copying is part of the build workflow, not a second independently edited source tree.

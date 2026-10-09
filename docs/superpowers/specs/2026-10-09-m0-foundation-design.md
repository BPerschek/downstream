# DOWNSTREAM initial technical foundation

The owner requested that development begin using installed Unreal and Blender, with the supplied v0.3 design as the game brief. This deliverable starts M0. It does not claim completion of the entire game or of M0's performance acceptance criteria.

## Intended result

A runnable fixed-camera Unreal test map containing a small home clearing, a route, and an industrial ruin. A player can move through it. Original low-poly Blender props establish an initial visual language. The authored story remains undecided; map labels describe functions only.

## Environment constraints

Unreal 5.8.3 and Blender 5.2.2 are installed. Baseline hardware is an RTX 5050 with approximately 8 GB VRAM and 32 GB system RAM. Visual Studio C++ tools and a Windows SDK are absent. Keep the existing Blueprint project runnable while preparing native modules; never register unavailable native binaries in the active descriptor.

Use Epic's installed Top Down Blueprint template for the runnable movement/camera prototype. Verify dependencies through Unreal. Native simulation is plain C++ and can be tested using a local portable compiler without requiring administrative installation. Do not imply the portable compiler can build Unreal's Windows targets.

## Architecture and first contracts

`DownstreamSim` uses standard C++ only, with no UObject, Actor or rendering dependency. `Simulation` owns a tick counter, accumulated real time, a terrain grid and a FIFO terrain command queue. `Advance(double seconds)` executes fixed 0.1-second ticks; equivalent elapsed time under different frame schedules must produce identical ticks and state. Negative/non-finite input is rejected. Terrain edits execute at tick boundaries, validate coordinates and finite heights, clamp heights to configured limits, and mark only the edited chunk dirty. Consuming dirty chunks must clear them.

`DownstreamWorld` is the future owner of seeded test-world generation; the first independent-cell seed fixture lives in DownstreamSim until it has meaningful world orchestration. A seed reproduces the same grid. `DownstreamRender` will read snapshots and construct mesh chunks; presentation cannot directly mutate simulation. The `Downstream` runtime shell will bridge engine lifecycle/input to commands. Native modules are prepared in native/Source outside the runnable Blueprint project; activation is conditional on a working Unreal compiler.

Bots, Combat, Colony, Settlements and UI module boundaries are reserved for later work rather than empty speculative implementations. Actor vs Mass comparison, native integration, terrain rendering, and packaged GPU profiling remain open M0 work if prerequisites prevent them here.

## Content and source control

Save editable Blender sources and reproducible exporters with Unreal-friendly FBX coordinates and consistent metre-to-centimetre conversion. Record Epic template content and any imported pack in ASSET_LICENSES.md. Library metadata alone does not establish downloaded content. Avoid replacing the active editor's unsaved work; create a new map through a separate commandlet process.

Ignore engine caches, Intermediate, Saved and build products. Preserve existing configuration before changing graphics defaults. Disable ray tracing and Lumen for the initial low-cost baseline; profile later instead of promising 60 FPS.

## Acceptance for this first delivery

1. Saved map and player/controller dependencies load without missing-package errors.
2. Unreal runs the prototype in game mode; input/camera and path navigation are tested where automation allows.
3. Original .blend and FBX assets exist and successfully import.
4. Native headless tests cover fixed-tick partition invariance, queued edits, invalid input, bounds, dirty chunk tracking and seeded generation.
5. Start, regenerate, test and future native build instructions are recorded with accurate limitations.

Full M0 sign-off requires working Unreal native builds, meaningful Actor/Mass comparison and packaged profiling. M1 combat/extraction is the next milestone, not part of this delivery.

# Playable expedition implementation

1. Establish tested pure inventory/stash/skill state and versioned JSON persistence in Downstream. Use an isolated save directory for integration tests.
2. Add DownstreamCombat: native input/controller/character, GAS health, ranged projectiles, melee, dodge, two AI behaviors and respawn. Keep its dependency on Downstream one-way.
3. Generate M1 from the saved M0 environment; add enemies, caches, extraction, invisible boundaries, tuning and original feedback audio. Build navigation in a separate editor sequentially after map saves.
4. Integrate a readable Canvas gameplay HUD in DownstreamRender. Preserve technical terrain controls and diagnostics.
5. Build both native targets and run plain tests, native automation and isolated game-world checks. Fix failures from observed evidence; inspect a real standalone frame.
6. Refresh README, toolchain/status, asset provenance and AGENTS handoff. Track canonical source and content, ignore mirrored source/caches. Commit the verified deliverable and synchronize the existing repository branch.

Canonical files are native/Source; build_native.ps1 protects independently changed mirror files. Agents own separate source folders. Only the integrating agent launches builds and map mutation to avoid Windows file locks.

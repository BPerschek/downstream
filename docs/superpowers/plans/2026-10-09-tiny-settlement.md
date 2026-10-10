# Tiny settlement implementation plan

**Goal:** Turn expedition salvage into a placed home settlement and one persistent hauling robot.

**Architecture:** Pure settlement state and transactional schema2 persistence extend Downstream. DownstreamColony owns world actors and navigation; Combat handles inputs and Render handles HUD. Existing one-way dependencies stay intact.

**Tech Stack:** UE5.8 C++, standard C++17 tests, Unreal Python map/runtime automation, existing original art and licensed CC0 feedback.

**Spec:** docs/superpowers/specs/2026-10-09-tiny-settlement-design.md

## Global constraints

- Canonical source remains native/Source; build/Unreal Source mirrors stay ignored.
- Build/editor/map operations run sequentially; preserve owner saves and editor work.
- Default price/capacity values: storage4/+8, workshop6, charger4, hauler8/cargo4.
- Snap200cm to center(-1500,-550,10), cellsX[-4,4],Y[-3,3]. One of each structure and one bot.
- Colony must not depend Combat; living hostiles use DownstreamHostile tags.
- JSON schema1 migrates; schema2 validates all fields and commits after successful disk writes.

## Review focus

- Failure to write must never spend bank or destroy pickups.
- Schema1 and malformed/future schemas must preserve owner files and progress.
- Placement must exclude ground contact from overlap validation while rejecting real props/player/buildings.
- Bot claims, saved cargo and delivery must not duplicate or erase salvage.
- Battery reserve, blocked paths and hostile targets must not create an invisible permanent stalled job.

## Tasks

- [x] Write/run failing pure settlement/spending tests, then implement bounded placement, cost/prerequisite, cargo and battery state; extend transactional persistence and schema migration in Downstream.
- [x] Implement Colony manager, preview/building actors and one hauler with navigation, safe-target selection, delivery, battery reserve and recharge. Integrate exact state APIs before compiling.
- [x] Add B/1-4/LMB/RMB/J input routing, readable build/bot HUD, hostiles tags, map manager and any licensed feedback assets.
- [x] Build Editor/Game, run pure/native/M1 regression and isolated settlement runtime/migration/reload checks. Repair observed failures and review code/frame.
- [x] Update README/AGENTS/save contracts/status/provenance; archive accurate evidence and commit/push the verified branch.

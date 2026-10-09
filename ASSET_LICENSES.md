# Asset and dependency provenance

| Content | Creator / source | Terms | Acquired / present | Destination |
| --- | --- | --- | --- | --- |
| DOWNSTREAM original blockout meshes, palette and map | Authored in this workspace for the owner | Original project work; no imported third-party models | 2026-10-09 | art/, Content/Downstream/ |
| Four synthesized prototype sound cues and generation script | Authored in this workspace; deterministic sine/noise PCM synthesis | Original project work; no downloaded recordings | 2026-10-09 | art/audio/, Content/Downstream/Audio/ |
| GameplayAbilities plugin | Epic Games, installed engine | Unreal Engine licensed technology; same EULA | Existing engine; enabled for prototype health | Engine plugin reference; DownstreamCombat dependency |
| Unreal 5.8 Top Down Blueprint template, mannequin and required shared template resources | Epic Games, installed UE_5.8/Templates | Unreal Engine licensed technology / Examples; see https://www.unrealengine.com/eula/unreal | Existing installed engine; reused 2026-10-09 | Content/TopDown/, Content/Characters/, required shared resources |
| Unreal built-in basic shapes and engine materials | Epic Games, installed engine | Unreal Engine licensed technology; same EULA | Existing engine | References to /Engine/ assets |
| ProceduralMeshComponent plugin | Epic Games, installed engine | Unreal Engine licensed technology; same EULA | Existing engine; enabled 2026-10-09 | Engine plugin reference; DownstreamRender dependency |
| AdaptivePathfindingSystem: AI Navigation Plugin 2.06 | NordVader Inc.; https://www.fab.com/listings/e57ff036-fb84-4c86-af7d-8c2a75965c73 | Installed by owner through Fab. Exact acquired license tier/terms are not present in inspected local metadata; no acquisition date is invented. Local project activation only; plugin source is not copied into this repository. | Verified installed 2026-10-09 | Engine/Plugins/Marketplace/Adaptive64768b7cb9a5V14; project enable flag |

The owner identified Stylized Harvest Farm Environment, Adaptive Pathfinding System AI Navigation, Ready Map Vol2 Endless Random Worlds / European Beach, and Shooter Game. Adaptive is now present as an installed engine plugin; its activation is recorded above. Shooter Game exists as a separate UE4.27 sample and is not imported. Harvest and Ready Map payloads were absent from inspected locations. Record actual publisher/source/license and destination when importing content; do not invent purchase dates or entitlement evidence.

Development tools are not shipped as game content. A locally downloaded portable Zig compiler is used only for isolated standard C++ tests; source: https://ziglang.org/download/ ; upstream license: https://github.com/ziglang/zig/blob/master/LICENSE . Compiler binaries and caches are ignored by Git.

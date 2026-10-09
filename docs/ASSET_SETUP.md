# Owned asset setup

Verified locally on 2026-10-09. Active project:
`C:\Users\persc\Desktop\Downstream\downstream\unreal\Downstream\Downstream.uproject`.
Installed engine: `C:\Program Files\Epic Games\UE_5.8` (5.8.3).

## Launcher project discovery

The Unreal editor already recorded Downstream's parent directory in its
`EditorSettings.ini`. The Epic Games Launcher had only
`C:/Users/persc/Documents/Unreal Projects` in its discovery paths, so the active
Downstream project lived outside the launcher's configured roots.

Added one line to the existing `[Launcher]` section of:

`C:\Users\persc\AppData\Local\EpicGamesLauncher\Saved\Config\WindowsEditor\GameUserSettings.ini`

```ini
CreatedProjectPaths=C:/Users/persc/Documents/Unreal Projects
CreatedProjectPaths=C:/Users/persc/Desktop/Downstream/downstream/unreal
```

The first line was retained; the second line was added. The new path is the
parent of the `Downstream` project folder. All other settings and original
encoding/newlines were preserved and verified against a byte-exact backup:

`C:\Users\persc\AppData\Local\EpicGamesLauncher\Saved\Config\WindowsEditor\GameUserSettings.ini.downstream-backup-20261009-140230`

No editor or launcher process was closed or restarted. The existing Downstream
editor session remained open. The configuration is verified; appearance in the
launcher's project picker still needs confirmation after it refreshes its
project list.

The project's engine association GUID at inspection was
`{37CE65AB-49F7-8464-0DAB-DCA269588518}`. Its value in
`HKCU\Software\Epic Games\Unreal Engine\Builds` resolves to the installed
`C:/Program Files/Epic Games/UE_5.8` directory. There was no broken engine path.
This investigation did not change the project descriptor or registry.

## Local payload inventory

| Product | Verified local payload | Project action |
| --- | --- | --- |
| AdaptivePathfindingSystem: AI Navigation Plugin v2.06 | Installed engine plugin at `C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Marketplace\Adaptive64768b7cb9a5V14\AdaptivePathfindingSystem.uplugin` | Enable `AdaptivePathfindingSystem` for Downstream before using its classes. |
| Shooter Game | Complete UE 4.27 sample cache at `C:\ProgramData\Epic\EpicGamesLauncher\VaultCache\ShooterGame_4.27\data`; created project at `C:\Users\persc\Documents\Unreal Projects\ShooterGame\ShooterGame.uproject` | Use a separate converted sample copy to inspect and migrate selected assets into Downstream. |
| Stylized Harvest Farm Environment | No matching payload found in the inspected VaultCache, engine Marketplace plugins, or Downloads entries | Add the acquired content pack to Downstream when it is available through the Launcher/Fab integration. |
| Ready Map Vol2 Endless Random Worlds / European Beach | No matching payload found in those inspected locations | Confirm the exact product and delivery type in Fab, then add its content or create its supplied project as appropriate. |
| Fab integration | `C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Fab\Fab.uplugin`, version 0.0.19, `EnabledByDefault=true` | In the open project, use **Window > Fab** to access the acquired library. |

This inventory is about local files. It does not assert that absent packs are
unowned, or that the inspected locations contain every download on the machine.

### Adaptive Pathfinding

The descriptor identifies the creator as **NordVader Inc.**, version **2.06**,
engine version **5.8.0**, `Installed=true`, and a Win64 runtime module named
`AdaptivePathfindingSystem`. The plugin's compiled editor module and the
installed engine both have `BuildId` **55116800**. There are source and binaries
but no `Content` directory or `.uasset` files in this installed plugin.

An engine plugin is installed for that engine and enabled independently in each
project. An "installed in all environments" message can therefore indicate that
installation is already complete; it does not mean content must be added again.
In Downstream, use **Edit > Plugins**, search **AdaptivePathfindingSystem**, and
enable it if required. Save work before an editor restart requested by Unreal.
No plugin activation or project descriptor edit was performed by this asset
discovery investigation.

Publisher documentation recorded in the installed descriptor:
<https://ubiquitous-petroleum-1ed.notion.site/Documentation-16031447dcc8804e9ae3f6072d492b6c>.

The local Fab database's `catalog` record matches the installed product title,
catalog item `8fcea2442f31433aa896c4953d1c1d12`, and namespace
`89efe5924d3d467c839449ab6ab52e7f`. Its description links the publisher's current
documentation at
<https://nordvader.com/documentation/assets/adaptive-pathfinding-system/>.
The [official Fab listing](https://www.fab.com/listings/e57ff036-fb84-4c86-af7d-8c2a75965c73)
has the same `e57ff036` identifier prefix as the local manifest cache and
identifies the product and publisher. That page currently shows a **Personal**
license selection, but its rendered **License terms** value is `-`; neither that
selection nor the installation records prove the owner's acquired license tier.

### Shooter Game

Both the cache and created sample contain **899 `.uasset` files and 10 `.umap`
files**. The created sample still declares `EngineAssociation=4.27` and has
`ShooterGame` and `ShooterGameLoadingScreen` native modules. It is a separate
sample project, so installing it does not import those assets into Downstream.

For selected reusable art, open a separate sample copy with a compatible
conversion workflow, choose assets in the Content Browser, use **Asset Actions
> Migrate**, keep their dependencies in the Asset Report, and choose:

`C:\Users\persc\Desktop\Downstream\downstream\unreal\Downstream\Content`

Conversion and migration have not been performed here. Existing content and the
legacy Shooter Game C++ modules were not copied into Downstream. Version and
native API compatibility must be checked in the sample copy before depending on
its Blueprints or code.

## Provenance and remaining verification

The Fab local cache database currently records Shooter Game and its UE download
path; the Launcher installed-manifest metadata separately records the Adaptive
plugin installation. The installed `.item` manifests under
`C:\ProgramData\Epic\EpicGamesLauncher\Data\Manifests` confirm both the Adaptive
plugin and `UE_5.8` installation, even though `LauncherInstalled.dat` is empty.
The Fab database has a cached Adaptive catalog record but no Adaptive entry in
its `local_listing` or acquisition table, and its inspected table schema has no
license fields. Those records and payloads do not establish purchase dates or
the exact acquired license tier. Record the applicable receipt/listing terms in
`ASSET_LICENSES.md` when the relevant content is incorporated. Do not infer
license terms from an installation marker alone.

**Window > Fab** in the current Downstream editor session can access applicable
acquired products for that open project without relying on the launcher's
project picker.

The Launcher project picker has not been inspected after the discovery change.
If a content pack's picker filters out Downstream by engine support, inspect its
supported versions separately from the discovery-root issue. Do not assume that
an older content pack or native plugin is compatible with 5.8 merely because the
project is discoverable.

Official workflow references:

- [Working with Plugins](https://dev.epicgames.com/documentation/en-us/unreal-engine/working-with-plugins-in-unreal-engine)
- [Migrating Assets](https://dev.epicgames.com/documentation/en-us/unreal-engine/migrating-assets-in-unreal-engine)
- [Purchasing and Downloading Assets in Fab](https://dev.epicgames.com/documentation/en-us/fab/purchasing-and-downloading-assets-in-fab)

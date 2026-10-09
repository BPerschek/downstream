# Native Windows build prerequisite

Verified on 2026-10-09: Unreal5.8.3, Blender5.2.2LTS, Visual Studio Community2026, MSVC14.44.35229 from the14.44.35207 installation directory, and Windows SDK10.0.26100.0. Editor and Game targets now build successfully. The owner installed Visual Studio after the initial Blueprint-only delivery.

Install Visual Studio 2022 Build Tools 17.14 or a compatible newer version with **Desktop development with C++**, the x64/x86 MSVC tools, and Windows 11 SDK 10.0.22621 or newer. Microsoft's installer may require an administrator approval; no system-wide compiler installation was attempted by this task.

Unreal's local `Engine/Config/Windows/Windows_SDK.json` accepts preferred MSVC 14.44.35211+ or 14.50.35723+ and rejects several earlier compiler revisions. The installed engine's configuration is the authority for compatibility.

Microsoft installation guidance: https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-build-tools

After installing, close the existing Unreal editor so DLLs are not locked, then run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_native.ps1
```

Canonical source lives under native/Source. The build script creates the ignored unreal/Downstream/Source mirror and records hashes under build/. It refuses to overwrite independently edited mirror files; reconcile those into canonical source first. Both Editor and Game targets build before activation is retained. Initial failed-activation rollback preserves a Blueprint project; the current delivery uses native modules and the simulation is connected to the game.

On another machine update scripts/project_paths.ps1 and reassociate the project with that installation's engine through Unreal's normal version selector. This PC's existing EngineAssociation GUID is valid. The optional portable Zig compiler is only for standalone C++17 tests, not Unreal builds. Use setup_test_compiler.ps1 on a fresh checkout. Tools and caches stay ignored.

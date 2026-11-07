# Building from source

Windows x64 with Microsoft Visual Studio 2022 C++ x64 tools and Windows SDK is
the supported build environment. No commercial ROM or generated game C source
belongs in this repository. Installation generates them privately on your PC.

For a source checkout, the installer can run directly from PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/install.ps1 -Destination "D:\Games\RoadRashTrilogyRecomp" -RomFolder1 "D:\My ROMs"
```

This bootstraps the pinned dependencies, verifies your ROMs and builds locally.
Use `-NoToolInstall` to fail instead of invoking Microsoft's Build Tools installer.
The offline release includes the cache used by the same script.

For an already prepared developer workspace:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildFolder build/release02
.tools/cmake/cmake-4.4.5-windows-x86_64/bin/ctest.exe --test-dir build/release02 --output-on-failure
.tools/python/python.exe -m unittest discover -s tests -p "test_*.py" -v
```

Only games with private verified manifests are built. `-Target TrilogyLauncher`
rebuilds just the launcher. CMake, the native resource version and the package
tool use version **0.50**. Dependencies and hashes live in
`tools/public-tools.lock.json`. Runtime fixes are reproducible patch scripts;
`patches/runtime-integration.patch` is a reviewable generated diff against the
pinned SDK source. Never edit generated game C.

## Local package preparation

```powershell
.tools/python/python.exe tools/package_release.py --offline --music-pack .local/music/dean-harris
```

The destination must be fresh. This creates source, standard installer and
offline installer ZIPs under `dist/0.50`, with per-package file manifests and
SHA256SUMS. The package tool uses an allowlist and rejects ROMs, private folders
and unexpected binaries. The only prebuilt application is the ROM-independent
launcher and its SDL2 DLL. Public SDK caches appear only in the offline package.
`--music-pack` verifies the three cue maps against track provenance and SHA-256,
then copies only their referenced audio into both installer variants. Source
ZIPs exclude recordings. Raw downloader metadata and unmapped files are excluded.
Omit the option for source-only/developer packages without local recordings.

## Repository model

The project follows [BTr-RJ/RedBaron_Recomp](https://github.com/BTr-RJ/RedBaron_Recomp):
public integration source, patches, documentation and local-build installer;
user-owned game inputs stay on the user's computer. Here launcher and game
integration source remain in `src/launcher` and `src/game`; scripts remain in
`scripts` / `tools` to preserve existing build paths. `.github` contains source
validation and issue templates. A release is a manual owner-approved action;
the workflow never publishes releases.

See [AGENTS.md](../AGENTS.md) for the source map, engine history and continuation
instructions. Historical `rr.ps1` research commands and older bootstrap scripts
remain for development; the supported 0.50 installation path is `install.ps1`.

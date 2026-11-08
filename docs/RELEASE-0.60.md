# Version 0.60 — Public Test

Version 0.60 updates the Windows x64 public test for Road Rash, Road Rash II and
Road Rash 3. Installers still compile locally from the user's verified ROMs;
neither ROMs nor generated game code are distributed.

## Improvements over 0.50

- Automatic release checks and an **Updates** screen in the launcher, following
  the RedBaron_Recomp workflow. Downloads require the GitHub asset's SHA-256;
  compilation and runtime checks complete in a separate stage before managed
  files are replaced with persistent rollback/recovery metadata.
- More stable approach and scaling of roadside scenery, obstacles, opponents and
  traffic. Current-source fractional coverage reduces edge crawling and thin
  objects disappearing between sample positions. Source-art qualification avoids
  unnecessary changes to an already reduced bitmap during approach.
- Corrected road-relative positioning and hill profiles reduce floating objects
  and stale geometry across climbs, descents and crests. Original player poses,
  HUD, collision behavior and game speed are preserved.
- Presentation enhancements now extend to the original RR2/RR3 local split-screen
  modes, with independent camera/road state. RR1 keeps its original alternating
  player mode. Network play is not included.
- Automatic campaign checkpoints after single-player races. **F1** or Player 1
  **R2 / RT** loads the latest checkpoint, including after restarting the game.
  See [controls and checkpoint compatibility](CONTROLS.md).
- Verified bundled remake tracks loop at the musical ending and omit the common
  non-musical source outro. Original music and missing-track fallback remain.
- Shader authors' artistic parameters are preserved. Widescreen uses the same
  canvas-aspect integration as Super Monaco Duo Recomp; native 4:3 keeps its
  authored presentation. No engine color correction precedes an external shader.
- Fixed empty generated C initializers that could cause the reported MSVC 2019
  C2059 installation error. The fix is in the emitter, not generated game C.

The README now compares **original Mega Drive rendering versus 0.60**, using
synchronized captures at normal speed. The original side uses the native raster
renderer in this runtime, not a recording from physical console hardware.

## Installation and updates

Download the standard or offline Windows x64 installer and extract it completely.
Choose **Install games**, supply supported original ROMs, and select an empty
destination or an existing recognized installation. Existing control preferences
and achievements are independent of campaign checkpoints. Version 0.50 release
downloads remain available unchanged.

Checkpoints are tied to the **exact locally built executable and ROM**. States
from 0.50/development builds, or a different rebuild, are not compatible with
0.60. Keep the old installation and its state if you need that checkpoint; a new
completed race creates a checkpoint for the new build. Do not edit state headers
to bypass compatibility checks. The save write is atomic and retains the prior
file if writing fails.

## Validation and scope

Local validation covers synthetic pipeline/installation checks and native groups
for controls, presentation, achievements, motion, VDP composition, sprite geometry,
whole scenes, fractional sprite coverage, split screen, road markings, music,
generated layout emission and campaign state behavior. Fresh 0.60 captures and
installed-build checks are recorded during release preparation; public GIF
provenance binds their exact executable hashes.

Complete campaigns, every track/state, every physical controller and complete
static execution coverage are not claimed. Animation keeps its original poses
and update rate; a 59.94 Hz presentation does not create 60 authored poses per
second. Unsupported/incomplete source observations retain native rendering.
Widescreen remains experimental. The C11 emitter fix is regression-tested, but
the reported exact MSVC 2019 toolchain was unavailable; Visual Studio 2022 is the
qualified toolchain.

The bundled music recordings retain their existing credits and rights; this
release does not add a music license. See [music](../music/README.md),
[installation](INSTALLATION.md), [video](VIDEO.md) and
[third-party notices](../THIRD_PARTY_NOTICES.md).

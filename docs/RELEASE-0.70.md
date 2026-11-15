# Version 0.70

Version 0.70 updates the Windows x64 release for Road Rash, Road Rash II and
Road Rash 3. Installers still compile locally from the user's verified ROMs;
neither ROMs nor generated game code are distributed.

## What is new

- **Individual rival identity in all three games.** Each rider gets their own
  helmet, vest and suit colors instead of sharing one look: 29 riders in Road
  Rash, the approved kits in Road Rash II (shared identities are reused across
  games) and 20 kits in Road Rash 3. Police riders and the original-art cases
  (falls and poses without a bike) are left untouched. Identities are applied
  from the game's own rider tables, also in rear-view mirrors, pause and the
  race start, without changing the global palette, sprite order, transparency
  or mirroring. Identity needs the continuous-geometry option, which is on by
  default.
- **Motorcycle motion option (Road Rash 1).** The Video settings page offers
  *Classic (original look)*, the default, and *Enhanced (smoother rival lean
  changes)*. Enhanced adds reconstructed in-between rival frames only in the
  presentation layer. Your own bike always stays original; speed, physics,
  collisions, AI, input, audio and the native animation clock are never changed.
  Settings from earlier versions remain compatible.
- **More reliable installation (GitHub issue #2).** The build now isolates the
  compiler environment for the build process only: variables such as `CL`, `CC`,
  `CFLAGS`, `LIB`, `INCLUDE` and `CMAKE_*` are cleared, missing `TEMP`/`TMP`
  folders are repaired, MSVC is selected explicitly and a real C and C++ compile
  and link probe runs before the build. An incompatible CMake cache is discarded
  and a valid one is kept to avoid full rebuilds. Your global `PATH` and user
  environment are never modified. Six regression tests, including a negative
  control that reproduces the failure, are in `tests/test_msvc_detection.ps1`.
- Launcher update flow: version shown in the window and About box follows the
  package version, and updating from 0.60 to 0.70 uses the existing verified
  update path (GitHub asset SHA-256, separate staged build, rollback metadata).

## Not included

The experimental RR3 "HD Pixel Remaster" research is not part of this release.
Experimental motorcycle-motion prototypes and the native-rate option are not
offered in the launcher.

## Installation and updates

Download the standard or offline Windows x64 installer and extract it completely.
Choose **Install games**, supply supported original ROMs, and select an empty
destination or an existing recognized installation. Existing installations can
use **Updates** in the launcher. Preferences, achievements, ROM inputs, saves
and campaign checkpoints are preserved. Earlier releases remain available.

Checkpoints and compiled games require the exact executable/ROM that created them,
as in 0.60.
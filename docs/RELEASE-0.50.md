# Version 0.50 — initial public test

The owner approved publication of the reviewed launcher and updated installers
on 6 November 2026. Release target: `BTr-RJ/RoadRashTrilogy_Recomp`, tag `v0.50`.
This first public test follows the ROM-free local-build model of RedBaron_Recomp.
Windows installer ZIPs include the verified 18-track remake pack.

## Changes

- English native launcher and installer, sober dark palette, classic covers,
  improved Barlow typography, original motorcycle masthead and RR application icon.
- Two-player keyboard/controller profiles: button and axis capture, controller
  assignment, deadzone, analog steering toggle, defaults, Save and Cancel.
- Local ROM-folder installer for any provided subset of supported trilogy games,
  pinned verified dependencies, build progress, logs, stop/retry and empty-folder
  destination checks. Original ROM folders are never modified.
- ROM-free standard/offline installer and source package preparation, manifests,
  hashes, public documentation, issue templates and source validation workflow.
- Preserves the 0.4.3 road-marking fixes, with follow-up corrections for RR1
  paused road continuation and distant grass detail in all three games.
- RR1 results music uses the user's Redwood Forest remake, including older packs.
  See [the regression fixes and evidence](GRASS-PAUSE-MUSIC.md).
- Repeated scenery instances remain visible while racing and after pause;
  [native copy grouping and verification](SCENERY-COPIES.md).
- Nearby wide sprites recover source margins removed by the original viewport;
  vehicle depth stays continuous across animation changes, with original game
  timing preserved. See [wide sprites and arcade zoom](WIDE-SPRITES-ARCADE-ZOOM.md).
- Follow-up high-curve continuation above the old raster cutoff and elimination
  of near-camera scale resets; [reproduced defects and checks](CURVE-NEAR-ZOOM.md).
- Road-anchored vehicle depth on hills, continuous complete-assembly upload
  transitions and player-relative near size, replacing the extra approach gain;
  [hill projection and size](HILL-VEHICLES.md).
- Curve-relative lane projection, causal vehicle-camera prediction, verified
  far-cutoff recovery and forward/mirror separation during near passes;
  [latest vehicle correction and evidence](VEHICLE-LANES.md).
- Continuous near-camera growth beyond the player-size calibration plane and
  whole-image departure during overtakes; [current behavior, reference study
  and validation](SPRITE-APPROACH.md).
- Rival steering correction and pose/state ownership across direction changes,
  attack overlays and separate rider/bike animation; [current engine follow-up](RIVAL-TRANSITIONS.md).
- Starting-grid teleport and close-pose placement corrections in all three games,
  preserving the measured native X/Y anchor through zoom; [latest evidence](NATIVE-VEHICLE-ANCHORS.md).
- Expanded self-contained [development handoff](../AGENTS.md), including native
  data layouts, recomp architecture, zoom equations and reusable debugging lessons.

## Validation

The working evidence is kept privately under `build/validation/release050`.
Automated source tests cover synthetic ROM intake, ZIP handling, unsupported
hash rejection, original preservation, SDK archive path validation and package
exclusions. Native tests cover the controls backend and existing presentation,
geometry, achievements and music behavior. A launcher QA mode exercises queued
keyboard messages through the real dialog loop, card selection and Enter,
settings activation and real mapping draft Save/Cancel/clear/reload paths.

A clean separate installation built all three games from verified local ROMs
and cached official SDK archives, using the existing Microsoft C++ installation.
The verification report records exact results and any remaining limits.
See [VALIDATION-0.50.md](VALIDATION-0.50.md) for the completed checks.

## Limits

Only the verified USA/Europe revisions listed in [installation](INSTALLATION.md)
are supported, including RR206 for Road Rash II. Japanese ROMs, other revisions,
modified images and interleaved SMD dumps are not supported. Intake checks SHA-256.

The runtime still has execution fallback and campaign coverage gaps. Enhanced
geometry is limited to recognized single-player races; other screens retain the
original raster. Side expansion remains experimental. Network multiplayer is
not implemented. Specific physical controllers and first-time Microsoft tool
installation on a machine without C++ tools are separate hardware/machine checks.
Windows release installers include 18 Dean Harris remake tracks with source
credits and checksums. Original music remains selectable. ROMs, generated game
code, saves and account data are not packaged. The source ZIP excludes recordings.

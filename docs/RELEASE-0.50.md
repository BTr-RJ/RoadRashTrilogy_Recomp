# Version 0.50 ? initial public test

[Download the Windows x64 public test](https://github.com/BTr-RJ/RoadRashTrilogy_Recomp/releases/tag/v0.50).
The release follows the ROM-free local-build model: supply your compatible
original ROMs, and the installer builds the games on your computer.

## Included

- English native launcher and installer with classic trilogy covers.
- Keyboard/controller mapping for the two original player ports, including
  assignment, deadzone, button/axis capture, defaults, Save and Cancel.
- Verified local ROM intake, pinned dependency downloads, build progress,
  cancellation and retry. Your original ROM folders are never modified.
- Continuous sprite approach/zoom, scenery and road-marking improvements in
  recognized single-player races, preserving original game speed and poses.
- Optional experimental 16:9 expansion and pixel, linear or CRT presentation.
- Original sound, independent music/effects volume and local achievements.
- Eighteen Dean Harris remake tracks in the Windows installers, with artist
  credits, source URLs and checksums. Original music remains selectable.

Standard, offline and source ZIPs include file manifests and SHA-256 checksums.
The offline variant includes verified SDK caches; Microsoft C++ tools must
already be installed for a completely offline build. Source ZIPs contain no
recordings, ROMs or generated game code.

## Compatibility and limits

Only the three SHA-256 revisions listed in [installation](INSTALLATION.md) are
supported: Road Rash USA/Europe, Road Rash II USA/Europe RR206, and Road Rash 3
USA/Europe. Japanese ROMs, other revisions, modified images and interleaved SMD
dumps are unsupported. Renaming a file does not change compatibility.

This public test uses translated 68000 code with fallback for remaining paths.
Complete static coverage and full campaign qualification are not claimed.
The published 0.50 packages retain native menus, pause and original split-screen
rendering. Network multiplayer is not implemented. Physical-controller models
and first-time Microsoft tool installation require machine-specific testing.
See [video settings](VIDEO.md), [controls](CONTROLS.md) and [building](BUILDING.md).

Source tests cover synthetic ROM intake, archive boundaries, dependency checks,
original-file preservation and packaging. Native tests cover controls,
presentation, VDP status, geometry, road markings, audio and local progress.
The current source-tree cleanup leaves the published 0.50 tag and download
assets unchanged.

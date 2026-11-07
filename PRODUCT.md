# Road Rash Trilogy Recomp

<!-- impeccable:product-schema 1 -->

## Platform and stack

Native Windows x64 desktop application. C/C++17, Win32, GDI+, SDL2 and a pinned
GenesisRecomp runtime. Python and PowerShell perform local installation and
builds. This is not a web application. No hosted account is required.

## Users and purpose

Players who supply their own compatible Mega Drive cartridge images can install
and launch Road Rash, Road Rash II and Road Rash 3. The version 0.50 installer
builds only the provided games locally. The public distribution contains no
commercial ROMs or generated game code.

## Current capabilities

The three games run native translated 68000 code with fallback for remaining
execution paths. Single-player races offer continuous scenery/vehicle scaling,
road and marking smoothing, optional 16:9 expansion and CRT filters. Menus,
pause and original split screen preserve their original presentation. Original
audio remains available. Windows release installers include the optional
18-track Dean Harris remake pack for portable local playback.
Local achievements need no login. Network play is not implemented. Complete
static coverage and full campaign validation are not claimed.

The English launcher provides keyboard and SDL-compatible controller mapping
for two player ports, button/axis capture, deadzone, controller assignment and
3-/6-button pad selection. Player 2 follows each original game's player limits.
Physical controller models require device-specific verification beyond virtual
controller backend tests.

## Visual and interaction commitments

The user chose a sober dark background and classic trilogy covers. Preserve
native Windows navigation, visible focus, the three-cover selection and a clear
Play / Install game action. Barlow Condensed headings, Barlow body text and a
restrained original motorcycle masthead support this hierarchy. English is the
launcher and installer language. Existing technical history may be Portuguese.

## Operating context and evidence

Release 0.50 is prepared locally for owner approval before GitHub publication.
Follow the local-build installer/source model of BTr-RJ/RedBaron_Recomp.
ROM manifests, controls, logs, saves and generated outputs stay private.
See AGENTS.md for source navigation, test commands and development history;
docs/ROAD-SEAMS.md records the 0.4.3 geometry baseline; docs/RELEASE-0.50.md
records the new release scope and verification limits.

## Principles

- Show installation and errors where the player can act on them.
- Keep gameplay simulation separate from presentation smoothing.
- Preserve original audio and rendering as comparison baselines.
- Publish only audited source and ROM-free installer packages after approval.

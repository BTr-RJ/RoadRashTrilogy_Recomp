# Launcher surface contract

## Scope and authority

Native Win32 / GDI+ launcher and native utility dialogs for version 0.50.
This is an incumbent refinement. The binding visual authority is the user's
sober dark background and classic trilogy covers, the current C++ implementation,
and the reviewed rendered evidence. No approved composition image or separate
quality-bar board exists. DESIGN.md records the observed system; it does not
claim broader visual approval. Owner approval remains required before publication.

## Purpose and hierarchy

Help a player choose one of three games, understand whether it is ready, then
Play or install from their own ROM folders. The three classic covers lead.
The selected game's description and readiness support that choice. The settings
rail contains Video & CRT, Audio & music, Achievements, Controls and Install games.
The original motorcycle masthead and RR icon support recognition without taking
over the cover hierarchy. Keep all launcher and installer copy in English.

## Direction contract

- Use DESIGN.md's charcoal surfaces, warm text, yellow selection and primary
  action, Barlow Condensed Bold headings and regular Barlow body text.
- Owner refinement on 6 November 2026: group ROAD RASH TRILOGY in the header;
  put yellow RECOMP 14 logical units after its ink bounds in Barlow Condensed
  Bold Italic (34px). Center PLAY / INSTALL GAME by visible glyph bounds in
  the same accent face (32px). Body and utility typography remain unchanged.
- Retain three equal cover cards in one row. Contain each cover at its original
  aspect ratio. Selection must have a visible border, underline and text change.
- Retain native modal controls and their Windows focus, value and disabled states.
- Preserve the centered 1280 by 800 drawing canvas with uniform scaling. Minimum
  tracked outer window: 920 by 600 logical units; at 300% DPI this is 2760 by 1800
  physical pixels. These are window dimensions, not a browser breakpoint.
- Show Play only when the selected game executable and private ROM are present;
  otherwise offer Install game. Do not substitute a false success state.
- Keep controls capture, draft/cancel/save behavior and installer progress/logs
  legible. Errors should identify the next action the player can take.
- Keep local achievement language distinct from official service unlocks.

## Required interaction checks

1. Tab and Shift+Tab visit actionable controls with visible focus.
2. Left/Right changes the selected game even with a child button focused.
3. Enter on a cover activates the selected game; Enter on a settings action opens
   that action rather than launching a game. Standard button Space remains usable.
4. Controls capture supports Escape, Delete and timeout; Cancel restores the draft;
   Save controls persists it. Two enabled player ports cannot share a gamepad slot.
5. Installer selection, progress, Stop installation, retry and Open launcher remain
   understandable. Original ROM-folder inputs are never edited by the installer.
6. The three covers, status and primary action remain visible at minimum size.

## Finish status and evidence

The finish reviewer returned **ship for the scored refinement fixes** after the
keyboard-navigation issue was resolved and durable product context was persisted
in PRODUCT.md. This verdict covers the reviewed surface and fixes; it is not
publication authorization or a claim of complete game validation.

Reviewed captures under `.impeccable/review/`:

- `main-desktop.png`: cover hierarchy, masthead, settings and Play.
- `main-minimum.png`: minimum-window composition and retained controls.
- `controls-desktop.png`: keyboard/gamepad mapping, capture guidance and actions.
- `install-desktop.png`: ROM-folder inputs, destination, progress and log area.
- `video-desktop.png`: resolution, CRT and rendering options.
- `audio-desktop.png`: volume sliders, music choice and local-file guidance.
- `achievements-desktop.png`: local progress and service distinction.

Implementation sources: `src/launcher/main.cpp`, `controls.cpp`, `install.cpp`.
Recorded raster provenance coverage was 5/5, with zero missing records in the
finish scan. Cover/font URLs and hashes are in `assets/launcher/sources.json`.
The HTML anti-pattern detector was not run: the surface is native Windows rather
than HTML. The sidecar's HTML/CSS snippets are documentation previews only.

## Evidence limits and next verification

Physical controller models remain unverified; virtual-controller backend tests
do not establish device-specific usability. Review capture, disconnection,
reconnection, assignment and in-game input with actual supported devices before
making device-specific compatibility claims. The screenshots establish layout,
not complete behavior across DPI configurations or all installation outcomes.
Network play is not implemented. Complete static coverage and full campaign
validation are not claimed. Publication remains a separate owner decision.

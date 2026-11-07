# Road Rash Trilogy Recomp — development handoff

## Current task and publication boundary

The owner approved the concrete launcher preview and explicitly authorized
GitHub publication on 6 November 2026, then reiterated "pode publicar".
Publication target: `BTr-RJ/RoadRashTrilogy_Recomp`, initial public test `v0.50`.
Publish only the audited source and ROM-free packages from `dist/0.50-public/`;
never upload this working workspace wholesale. Record the verified remote
commit, release URLs, asset checksums and CI outcome in the publication report.
Use the local-build/source model of `BTr-RJ/RedBaron_Recomp`: public project
integration, patches, documentation and ROM-free standard/offline installer;
the user supplies compatible original game files and generated game code stays
private. Preserve the existing `src/launcher` / `src/game` layout and build paths.

The owner selected **a sober dark launcher with classic trilogy covers**.
Launcher and installer UI are English. This is a native Windows application,
not a website. Version is **0.50**, not 0.5 or 0.5.0 in public labels.

## Read first when resuming

Latest owner request after a PC restart (6 November 2026): small launcher
typography refinements and remake music bundled with the Windows installers.
ROAD RASH TRILOGY now forms one header title, followed closely by yellow RECOMP;
RECOMP and centered PLAY / INSTALL GAME use bundled Barlow Condensed Bold Italic.
Native glyph bounds determine spacing and centering. Keep the existing dark
classic-cover layout and English labels. New evidence:
`build/validation/launcher-audio050/`. The original grid/pose work below is
preserved; no gameplay/presentation geometry algorithm changed in this follow-up.
See `docs/LAUNCHER-MUSIC-0.50.md`: 27 Python tests, nine native groups,
queued UI QA, RR1 clean install/update in an accented path and actual decoding
of all 20 bundled cues. `tests/test_controls.cpp` now uses `_putenv_s` so its
environment flag reaches the application's MSVC CRT instead of SDL2's DLL CRT.

The owner's bundled-music request supersedes earlier private-audio packaging
instructions: Windows installer ZIPs include only the 18 mapped full tracks,
three cue maps and sanitized artist/source/hash metadata. Keep raw downloads,
unmapped audio and `.info.json` private. Source ZIPs and source commits still
exclude recordings. Use `--music-pack .local/music/dean-harris --offline` with
`tools/package_release.py`. Runtime uses installed `music/dean-harris/`;
existing private `.local/music/dean-harris/rrN.tsv` overrides per game.
Original music and missing-track fallback remain available. This request is
not a new rights/license grant. Publication was separately authorized above.

This file is the project's durable handoff and the reusable Mega Drive recomp
notebook requested by the owner. It includes the algorithms and failure modes,
not just links to other documents. Start with the current follow-up below, then
the source map and build commands. The later **Reusable Mega Drive recomp
knowledge** section explains how to apply the method to a different game.

Current owner follow-up: the RR1 recording named `193745.mp4` showed remaining
rival animation/placement inconsistencies and a teleport before the start.
The native grid/close-pose anchor was being clamped away by the host model.
Source X/Y corrections now bind to the measured complete pose and project in
world units at the current depth. Populated native world-height observations
are qualified separately from road X. Shared correction covers all three games;
native speed/physics remain unchanged. Earlier lane damping, source/SAT ownership,
near growth, mirror separation and the arcade/CD-console study remain.
Read **Starting-grid teleport and native pose anchors** below first. Code:
`sprite_geometry.cpp`, `road_surface.h`; fixtures: `test_organic_scene.cpp`.
Current checker: `tools/check_grid_start.py`; evidence:
`build/validation/grid-final/` and `grid-final-video*/`. Older sections/reports
are history and must not be relabeled with current executable hashes.

The working games used by `Jogar.cmd` are in `build/release02/bin/rrN/`.
The latest owner-review candidate is identified in **Evidence and release
handoff** below. After this work, the next external step is the owner's playtest
and explicit publication approval. Do not interpret automated success as full
campaign qualification or promise identical Super Hang-On animation.

## Quick source map

| Area | Files |
|---|---|
| Main launcher, artwork, video/audio pages, launch environment | `src/launcher/main.cpp` |
| Mapping UI and shared profile persistence | `src/launcher/controls.cpp`, `controls.h` |
| Native installation UI, progress, worker cancellation | `src/launcher/install.cpp`, `install.h` |
| Windows identity, DPI/UTF-8, resources | `src/launcher/app.manifest`, `launcher.rc.in` |
| Game adapters and observed scene capture | `src/game/game_spec.c`, `scene_capture.c` |
| GPU/CRT presentation | `src/presentation/gpu_present.cpp`, `presentation.h` |
| Short prediction / latest-frame presentation | `src/presentation/motion_bridge.*` |
| Sprite geometry, composition, zoom, scenery assets | `src/presentation/sprite_geometry.*`, `sprite_assets.*` |
| Terrain continuation and road seams | `src/presentation/road_surface.*` |
| Central traffic marking projection and ownership | `src/presentation/road_markings.*` |
| Remake music, original fallback, pause behavior | `src/music/remix_player.*` |
| Local achievements and optional service integration | `src/achievements/` |
| Exact supported ROM revisions | `games/supported-roms.json` |
| Public per-revision build metadata | `games/rr*/runtime.toml`, `runtime-observed.toml`, `discovery.toml` |
| SDK versions / hashes / destinations | `tools/public-tools.lock.json` |
| ROM verification and private staging | `tools/install_roms.py` |
| Public SDK bootstrap | `tools/bootstrap_public.py` |
| Actual install/build pipeline | `scripts/install.ps1`, `scripts/build.ps1`, `CMakeLists.txt` |
| Reproducible engine patches | `tools/apply_runtime_patches.py`, `tools/patch_*.py` |
| Reviewable SDK diff | `patches/runtime-integration.patch` |
| Public staging, ZIPs, manifests and exclusions | `tools/package_release.py` |
| Source tests | `tests/test_pipeline.py`, `tests/test_installation.py`, `tests/test_*.cpp`, `tests/test_vdp.c` |
| Artwork and fonts | `assets/launcher/`, `sources.json`, `brand-050.prompt.txt` |
| Current user/build docs | `README.md`, `docs/INSTALLATION.md`, `CONTROLS.md`, `BUILDING.md`, `RELEASE-0.50.md` |
| Design truth | `PRODUCT.md`, `DESIGN.md`, `.impeccable/design.json`, `.impeccable/surfaces/launcher.md` |

## Build and tests

Windows PowerShell, MSVC x64 and Windows SDK are required. The working workspace
is `G:\PROJETOS\Road Rash Trilogy Recomp`; scripts resolve paths relative to
themselves, so installed copies must work independently of that path.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -BuildFolder build/release02
.tools/cmake/cmake-4.4.5-windows-x86_64/bin/ctest.exe --test-dir build/release02 --output-on-failure
.tools/python/python.exe -m unittest discover -s tests -p "test_*.py" -v
```

For launcher-only changes, add `-Target TrilogyLauncher`. `Jogar.cmd` prefers
the public root executable, then development release02, then native build.
Installed games normally live at `build/native/bin/rrN/rrN.exe`.
The installer supports a subset: absent private manifests skip game preparation
and CMake game targets. A fresh source tree can be installed using
`scripts/install.ps1 -Destination ... -RomFolder1 ...`; see BUILDING.md.

Native CTest groups: controls, presentation, achievements, motion, vdp,
sprite_geometry, organic_scene, road_markings, music. `test_controls.cpp` uses
SDL virtual controllers with real backend persistence/button/axis handling.
Python tests use synthetic fixtures, never commercial ROM bytes.

## Launcher validation hooks

The native launcher supports development QA commands:

```powershell
# Actual rendered native window; does not photograph other desktop apps
build/release02/bin/TrilogyLauncher.exe --qa-capture "D:\review\main.png" --qa-page main
# Pages: main, controls, install, video, audio, achievements; --qa-size 920 600
build/release02/bin/TrilogyLauncher.exe --qa-ui-test "D:\review\ui-result.json"
```

The second mode posts queued keys through the real GetMessage/IsDialogMessage
loop. It checks same-card Right then Enter, settings activation, and mapping
capture/Save/Cancel/reserved-key/clear/reload using a separate QA profile.
Do not replace this with direct SendMessage(WM_KEYDOWN), which bypasses the
dialog manager. Native buttons request arrows/Return with WM_GETDLGCODE;
Enter on a card must launch `selected`, not reselect the focused card's ID.
QA capture/settings checks must not save the user's normal preferences.

`tools/probe_launcher.py` validates the developer launcher → game path, CRT,
geometry counters and unchanged preferences. `tools/validate_installed.py`
does the same using a clean installed root executable and writes a JSON report.
Set `RR_VIDEO_VALIDATION=build/validation/release050` for developer probes.
`--qa-launch-crt` requires `RR_QA_INPUT` and uses a hidden, non-vsynced test game.

GDI+ private font collections must be constructed **after GdiplusStartup** and
destroyed **before GdiplusShutdown**. Keep Segoe UI fallback text visible when a
font cannot load. Native panel fonts scale with each panel's fitting scale,
which can differ from the monitor DPI. Dark checkbox text uses unthemed native
checkboxes plus explicit WM_CTLCOLORBTN; do not reintroduce black labels.

## Installer and controls data flow

The installer copies an allowlisted public source package, verifies Python and
ROM SHA-256, prepares pinned SDK archives, detects MSVC, then builds locally.
The optional Microsoft C++ installer is official and Authenticode-checked.
`-NoToolInstall` prohibits this machine-wide tool step during automated QA.
`-SkipBuild` is a developer ROM-validation mode, not a completed installation.
An early `.local/installation.json` marker allows retry after a failed install.
Only recognized product folders or empty destinations can be updated. Never
delete or modify the user's original ROM folders. Installation cancellation
uses a Win32 Job Object so child build processes stop with the worker.

Keyboard/controller mappings use the SDK InputMap/app_config backend. Player 1
defaults to keyboard + gamepad; Player 2 disabled. Slots follow SDL connection
order. `controller_slot` and `analog_dpad` are reproducibly patched SDK fields.
Save persists `.local/controls.ini`; launch copies the profile into the selected
game's settings and sets `RR_CONTROLS_PROFILE=1` to disable fixed controller
snapshot/turbo shortcuts. Keep reserved runtime F-keys, Tab and Escape.

## Engine history — preserve these fixes

| Version | Behavior and diagnostic reference |
|---|---|
| Initial / 0.2.x | Three native adapters, translated 68000 with fallback; aspect ratio, 9-bit SAT/HBlank/raster fixes, CRT, audio/motor/pause fixes. `docs/VIDEO-FIX.md`, `AUDIO-FIX.md`, `SPRITE-ZOOM.md` |
| 0.4.0 | Organic continuous scenery sprites, full source art, depth-based terrain and bounded edge retention. `docs/ORGANIC-SCENE.md` |
| 0.4.1 | Player composition/priority, visible traffic, incomplete actor updates, horizon artefacts and latest-state rendering; removed five-frame visual buffer. `docs/VISUAL-REGRESSIONS.md` |
| 0.4.2 | Stable opponent identities/composition, smoother road markings, stronger vehicle approach scaling (up to 18% extra near camera). `docs/APPROACH-SMOOTHING.md` |
| 0.4.3 | Symmetric RR1/RR2 road continuation, finite sampling shared by centre/edges, unified per-row marking replacement; RR3 prior path preserved. `docs/ROAD-SEAMS.md` |
| 0.50 | English launcher, mapping, user-ROM installer, restrained classic-cover design, source/release preparation. Preserve 0.4.3 rendering algorithms. |

The user reported RR1/RR2 lower-corner/centre geometry breaks and duplicated
yellow markings; RR3 did not show that defect. Do not restore independent
left/right continuation coordinates or partial per-pixel centre-line replacement.
Original vs enhanced RAM/VRAM must stay identical for the same replay.
Recognized single-player races get geometry; menus, pause and split-screen
keep native raster. Do not promise unique 60fps poses throughout the game.
Network multiplayer and full campaign/static coverage remain unimplemented or
unqualified. Original music must stay selectable; remake packaging now follows
the latest explicit owner request above (the earlier candidates excluded audio).

## Evidence and release handoff

Historical 0.4.3 evidence: `build/validation/road043/result.json` (10 raster
replays, 6 video comparisons, 3 PCM, 3 launcher runs; RR3 48 screenshots +
RAM/VRAM matched 0.4.2). Original/enhanced GIFs:
`build/validation/road043/gifs/01-rolagem-original-mega-drive.gif` and
`02-zoom-continuo-0.4.3.gif`; `tools/make_comparison_gifs.py` rebuilds them.
`tools/check_road_regressions.py` is a historical 0.4.3 checker with explicit
version assumptions; do not confuse it with a current 0.50 release validator.

0.50 evidence: `build/validation/release050/`, native/Python tests, UI JSON,
clean-install logs and isolated launcher/game checks. UI captures for owner
review are `.impeccable/review/*-desktop.png` and `main-minimum.png`.
Impeccable finish review resolved keyboard navigation and PRODUCT.md scope;
its ship verdict applies to those scored fixes. No HTML/CSS detector applies
to this native UI. Physical controller hardware and first-time global C++ tool
installation need separate machine/hardware checks.

Run `tools/package_release.py --offline` into a fresh output directory. Public
standard/offline/source packages and SHA256SUMS are under `dist/0.50`.
Each package includes PACKAGE-MANIFEST.json. Stage source from this allowlist,
never upload the working workspace wholesale. `.github` validation does not
publish releases. Before later approved publication, use the validated package
and review source diff/manifest; do not accidentally publish private evidence.

The first `dist/0.50-approval/` candidate was superseded by the grass/pause/music
corrections. Those were subsequently superseded by the scenery-copy correction.
The scenery candidate was superseded by wide sprite recovery and actor smoothing.
That first arcade candidate was superseded by the high-curve/near-zoom correction.
The curves and hills candidates were superseded by lane-relative projection,
causal vehicle-camera prediction and separation of forward/mirror artwork.
The lane candidate was superseded by qualified near-camera growth and complete
sprite departure after the owner found that overtakes still felt too flat.
That entry candidate was superseded by the owner's RR1 gameplay follow-up.
That rival candidate was superseded by measured native grid/close-pose anchors.
Owner-reviewed output: `dist/0.50-approval-launcher-music/`.
Approved publication packages, with download links and approval status updated:
`dist/0.50-public/`. Launcher/game binaries are unchanged from owner review.
Other `dist/0.50*` directories are intermediate packaging/test outputs; do not
publish them. Run `tools/audit_release.py dist/0.50-public` to verify final ZIP
names, full contents, per-file manifests, ROM exclusion and SHA256SUMS.
The completed check summary is in `docs/VALIDATION-0.50.md`: 24 Python tests,
9 native groups, all three installed launcher paths, and RR1-only installation
with successful retry/gameplay in a directory containing accented characters.

### Later 0.50 review corrections

See `docs/GRASS-PAUSE-MUSIC.md` and private
`build/validation/grass-pause-final/result.json` for the follow-up fixes:

- RR1 pause retains current-raster finite-road continuation on a wide canvas,
  without temporal prediction, sprite scaling or terrain resampling.
- `rr_geometry_frame` scene values are now 0=native, 1=live race, 2=paused race;
  callers must not treat every nonzero value as an active organic scene.
- Distant grass blends back to authored raster detail instead of flattening to
  the dominant palette color. Do not restore the old footprint>=4 solid-color
  branch; it caused the missing grass markings in all three games.
- RR1 results cue 9 reuses Redwood Forest cue 7's local file. Older packs are
  supported at runtime; explicit cue 9 mappings take precedence. Do not alias
  unknown cues or apply this RR1-specific mapping to RR2/RR3.
- Nine native groups and 24 Python tests passed. Four off/on video comparisons
  passed (three wide games and RR1 4:3), with matched before/final guest RAM/VRAM.

### Scenery-copy follow-up

`docs/SCENERY-COPIES.md` records the next review correction: native scenery jobs
can contain multiple instances and paired images. `scenery_copies` recovers each
complete repeated SAT assembly, including its placement, mirroring and palette.
Never suppress every SAT piece while drawing only one instance of a shared job.
Unrecognized, subpixel or empty replacements preserve the native assembly.
Object traces are emitted only after atomic replacement succeeds.

`RR_SCENERY_AUDIT` enables optional per-job candidate coverage diagnostics.
It is off in normal play. New synthetic tests cover paired repeated objects,
mirroring/palettes and visibility fallback. Four replay comparisons passed:
RR1/2/3 wide and RR3 4:3. Matched pause/title/HUD and guest RAM/VRAM remain
unchanged; multiple scenery copies are present after resuming. See private
`build/validation/scenery-confirmed/result.json` and `tools/check_scenery.py`.

### Wide sprites and arcade zoom follow-up

See `docs/WIDE-SPRITES-ARCADE-ZOOM.md` and private
`build/validation/arcade-final/result.json`. The owner requested fixing wide
cuts first, then smoother Super Hang-On-like zoom with exact original speed.

- `SourcePacket` captures uncropped actor projections and committed descriptor
  parts. `early_sources` bridges culls before the actor builder. Native builder
  packets take precedence; empty packets mean actual removal. Keep bounded
  lifetimes and exclude the player. Never retain an object solely by stale SAT.
- `compose_actor` keeps native current animation pixels, learns palette/flip/
  priority, and restores missing source margins before zoom in widescreen.
  The destination remains clipped to the race viewport; do not zoom the fixed
  HUD clip around the object's anchor. Real hill clipping is still projected.
- `ActorCache::motion` follows stable actor identity across image changes in
  both aspect ratios. `source_motion` uses descriptor timestamps; `scene_sample`
  predicts inverse depth for at most six frames, presenting latest poses now.
  Texture upload holds retain complete assemblies while fresh motion advances.
- `RR_EDGE_RECOVERY=0` and `RR_ARCADE_MOTION=0` are internal A/B switches.
  `tools/check_arcade_zoom.py` checks recovery, extra actor motion, unchanged
  guest/HUD, off/on video and PCM evidence. Nine native/24 Python tests passed;
  2,880 replay state snapshots and 1,440 HUD frames match the prior build.
- Compatibility must remain explicit in public GitHub material: only the three
  SHA-256 revisions in `games/supported-roms.json`. Japanese ROMs and other
  revisions are unsupported; filename changes do not make them compatible.
  README, INSTALLATION and RELEASE-0.50 now state this explicitly.

### High-curve and near-zoom review follow-up

The owner supplied two RR1 screenshots of abrupt road edges and reported close
sprites breaking their approach. See `docs/CURVE-NEAR-ZOOM.md` and private
`build/validation/curve-zoom-final/result.json`.

- Never restore the line-72 continuation cutoff: confirmed road edges can hit
  their native clamp above it. Early rows require eight authored ground texels;
  continuation validates eight prior tangent samples. Keep both sides symmetric
  and reject non-road discontinuities. Real left/right off-road cases are saved.
- Never reset a valid actor pose to `cache.root` solely because its scale is
  twice the cached bitmap's size. This caused the reproduced RR2 car to shrink
  from 1.413 to 0.786 during approach. Near-depth prediction now saturates
  continuously before a zero crossing; X/Y/Z share the same effective alpha.
- `ActorCache::parts` can retain qualified original art for direct sampling.
  `compose_actor` qualifies the whole current assembly against native colors,
  silhouette and priority; unsupported poses keep the complete native canvas.
  Do not bypass this check or mix independently updated body parts. The
  `source_parts` traffic-trace column identifies direct-art candidates.
- Native fixtures test early/steep clamps, no collapse at the 2x threshold and
  real source detail recovered after minification. Seven replay comparisons
  preserve 2,592 guest snapshots and 1,296 HUD frames; raster corruption counters
  are zero. Four video comparisons and three byte-identical PCM checks passed.
- `tools/check_curve_near_zoom.py` verifies the actual reproduced defects.
  Probe `--wait`/`--steer` overrides reach late curves and the opposite off-road
  direction; preserve default timings for earlier regression tools.
- Private previews are `curve-zoom-final/zoom-antes-depois.gif` and
  `curva-antes-depois.png` under `build/validation`. Current packages stay ROM-free.

### Vehicle hills, upload continuity and proportions

The owner reported fast cars/riders breaking their zoom on hills and opponents
appearing too large next to the player. `docs/HILL-VEHICLES.md` describes the
replacement for the previous generic vehicle near-plane predictor.

- `SourcePacket` now captures world Z, camera Z and the native road profile.
  `vehicle_sample` recognizes native focal calibration 2.5/3, predicts relative
  depth over one observed interval and uses `road_point` for the current ground
  anchor and crest. Never extrapolate regular vehicle height about fixed y=64.
- Normalize near size at the player's depth-2.5 ground plane, preserving car
  calibration and artwork dimensions. Keep position advancing as vehicles pass.
  The 18% approach gain and `RR_TRAFFIC_ZOOM` override are retired. Do not restore
  either, or substitute a limit tied to an old bitmap's size.
- Calibrated scale remains bounded even if a hill profile is unavailable or
  fresh descriptor metadata temporarily ends. Keep the native vertical anchor
  on unqualified profiles. Do not revert a still-present complete SAT assembly
  to an older bitmap-history pose while a new builder/upload is pending.
- `native_hold` is authorized only while that complete native assembly still
  matches the actual SAT. It ends on native removal/reuse; synthetic coverage
  verifies no ghost after removal. Player input/poses remain native.
- Hill occlusion is a current screen-space crest, independent of sprite zoom.
  Direct source qualification allows one pixel of minification phase, with
  bidirectional color/priority checks and at most 2% unsupported covered samples.
  Unsupported recolors/poses keep the whole current native assembly.
- Nine native groups and 24 local Python tests passed. Six replays preserve
  1,728 guest snapshots / 864 HUD frames, with zero raster corruption counters.
  Four video comparisons and three byte-identical original-PCM pairs passed.
  `tools/check_hill_vehicles.py` and private
  `build/validation/hill-vehicle-confirmed/result.json` record that historical evidence.
- Earlier numeric near-scale expectations in `check_curve_near_zoom.py` describe
  its frozen historical captures. Current vehicles intentionally stop inflating
  at the player-plane size reference in the hills candidate. The near-camera
  follow-up below supersedes that cap; use its checker for current builds.

### Fast direction/speed changes, lanes and mirrors

- The former vehicle X predictor used `(screenX-160)/spriteScale` as lateral
  position. That included road curvature and camera steering, then incorrectly
  extrapolated them as the vehicle's own lane velocity. Vehicles now use the
  native horizontal road profile plus actor-local lateral position.
- `RoadRays` includes native world X/Y, clipped Y and horizon. Native interleaved
  X/Y tables: RR1 `$FF310A`, RR2 pointer at `$FF1CE8`, RR3 pointer at `$FF1CE0`.
  Changes to X alone must update the road snapshot, even while camera Z and
  clipped Y are stationary. Native vehicle horizon is 76 in the qualified full
  view (RR2 `$FFF2BC`, RR3 `$FF59A2`). Do not restore a guessed y=64 for vehicles.
- Vehicle-camera observations are captured at their actual hook ticks. A past
  forecast must not be re-interpolated as though it were a new measurement;
  that caused reverse corrections during braking. The shared vehicle camera is
  sampled once per presentation frame, with bounded, causal forward prediction.
  NPC world motion and the camera are predicted separately.
- Forward and rear-view mirror descriptors reuse actor IDs. Rear projection
  packets and their two HUD copies must never replace the forward canvas or
  appearance. A near vehicle completes its forward pass with the last complete
  forward art, current world motion and *unclipped* ground projection. The short
  departure is one-way until an actual forward projection returns. No legacy
  bitmap-history fallback may resurrect it on a mirror upload.
- `native_hold` still requires a matching native assembly. Separate world holds
  require the verified live-actor flag: actor byte +104 bit 6, current RAM and
  recent packet both active. The far hysteresis band is depth 28..34, freshness
  at most 12 frames. A near pass requires calibrated raw ground, depth .25..2.5,
  live state and a forward appearance no older than 32 frames. Its remaining
  complete canvas exits before that deadline. These are presentation rules;
  never change guest culling, collisions, Z, speed or input to implement them.
- Tiny calibrated actors retain native minimum-pixel sampling. Do not alternate
  their few visible pixels between direct-ROM and already-minified art. Large
  qualified source art still uses the verified complete-pose path.
- New fixtures cover stationary-camera countersteering, horizontal-only road
  changes, far cutoff crossings, live-flag removal, stale/out-of-band rejection,
  unclipped ground, actual mirror SAT uploads, braking after a pass and prevention
  of legacy re-entry. Earlier player, geometry, palette and whole-body cases remain.
- `RR_VEHICLE_COVERAGE` optionally writes actual host-actor pixels per frame.
  `traffic.csv` also records lane anchoring, road center, raw-ground use and
  near/far holds. Geometry trace presence alone does not prove that a visible
  pixel was drawn. Player occlusion, real crests and native-only fallback must
  be distinguished from unintended disappearance.
- Lane-candidate technical note: `docs/VEHICLE-LANES.md`. Historical checker:
  `tools/check_vehicle_lanes.py`; aggregate:
  `build/validation/vehicle-lanes-final/result.json`, with current executable
  SHA-256 values for that candidate. Six paired replays preserve 3,168 RAM/VRAM snapshots and
  1,584 HUD frames; raster faults are zero. Dynamics contain 7,891 / 3,964 /
  7,020 calibrated lane observations. RR1 FF24AA retains visible pixels on
  frames 3162..3166. RR3 FF19B2 completes its forward pass on 3836 and never
  re-enters on mirror uploads over 3837..3855. Four video comparisons and all
  three original-PCM pairs passed. Nine native groups and 24 Python tests pass.
  The normal-speed preview is
  `build/validation/vehicle-lanes-final/veiculos-curvas-passagem.gif`.
- Do not overlap probes for the same game when qualifying final evidence.
  A broad overlapping video batch produced unequal paired states/title/HUD;
  its results were rejected. Isolated per-game repetitions passed on the same
  final binaries. The transient cause was not established. Never waive state
  equality, rewrite a historical report's hashes or count a failed pair as proof.

### Near-camera approach and cross-platform reference study

The owner still found the lane candidate's overtakes flat, especially RR1, and
requested Super Hang-On arcade plus Road Rash 3DO/PS1/Saturn as references.
Do not treat the previously passing proportion tests as proof of the desired
approach effect. They enshrined a size plateau that contradicted the near X/Y
perspective. New behavior supersedes those numeric near-size assertions.

- Root cause: both `vehicle_sample` and draw preparation limited artwork to
  `focal / 2.5`, while ground and lateral position kept using a smaller depth.
  At depth 2.5 the proportions were correct, but subsequent approach moved the
  vehicle down at a constant size. A calibration plane is not a size ceiling.
- Qualified raw-ground vehicles now use `focal / max(depth, .25)`; X/Y and size
  agree. At depth 2.5 riders remain 1x and cars 1.2x. Distant and same-plane
  actors gain nothing. Below that plane, a physically closer actor grows.
  There is still no 18% multiplier or `RR_TRAFFIC_ZOOM` override. Unknown ground
  profiles retain the conservative `focal/2.5` cap. Road projection helpers
  accept the finite .25 near limit, and the live near-pass band extends to .25.
- Simply removing the cap initially let a car's growing roof rise back above
  the HUD during braking/grades. That intermediate attempt was rejected after
  actual pixel review. `ActorCache::departure_top` now tracks the complete
  forward canvas's top, not its feet. During a rear-world departure, compute
  extent = `(root.y-canvas.y)*pose.scale/root.scale`; advance the top one-way
  at bounded 2..12 pixels/frame, with remaining distance to line 153 accounted
  for before the existing 32-frame appearance deadline; set feet = top+extent.
  Keep depth nonincreasing in this phase. A new true forward projection can
  reset departure. Never use mirror art, and preserve native live/removal guards.
- Native animation poses and the guest's logic, speed, input, collisions, audio
  and road raster are unchanged. The near extension is host presentation. It
  does not generate missing animation poses or claim the exact timing/art of
  a different game.

External research used the Agent Reach skill's native-web adaptation. No
external implementation or assets were imported. The important distinctions:

| Reference | Verified source / lesson |
|---|---|
| Super Hang-On arcade | MAME `segaorun.cpp::shangon` selects **System 16B sprites**, not Out Run's sprite device. `sega16sp.cpp` handles zoom, full image extents and destination clip. Preserve complete art/anchors before clipping; do not label all Sega scaler devices as identical. |
| Road Rash 3DO | trapexit's functional reconstruction at commit `cc33365427a7cc193c3b9111a24948e9993bb7ea`: `project_road_world_point.c` and `render_road_projected_cel.c` use reciprocal depth for projection and artwork bounds, then CEL mapping. Portfolio CCB documentation defines origin and size vectors. This supports a shared projected rectangle/anchor; it is not original EA source or a matching decomp proof. |
| Road Rash PS1 | Randy Breen's creator interview identifies it as directly derived from 3DO. PSX-SPX documents that GPU **rectangle** commands do not scale their texture; textured **polygons** support changing projected vertices. A visually scaled sprite must not be assumed to be a hardware scalable rectangle. The game's exact PS1 packets were not traced here. |
| Road Rash Saturn | Sega VDP1 manual defines a scaled sprite's zoom/fixed point, display dimensions and two-corner mode. Anchor and full destination bounds matter. The game's exact VDP1 stream was not traced here. |

Reference links and qualification are in `docs/SPRITE-APPROACH.md`, including
the pinned reconstructed 3DO functions, original Portfolio/Sega manual pages,
MAME, PSX-SPX and the Breen interview. These were technical-source studies;
the CD-console games were not run/timed in this workspace. Do not claim identical
measured projection curves or frame-for-frame equivalence. Apply the common
principle to the verified Mega Drive data instead of copying another game's
camera constants, renderer, art or simulation.

Current verification:

- `tools/check_vehicle_entry.py`, private
  `build/validation/vehicle-entry-final/result.json` and executable SHA-256 values.
  Seven paired replays preserve 3,648 RAM/VRAM snapshots and 1,824 HUD frames.
  All audited player-corruption, missing-native-actor and blue-road faults are zero.
- 272 / 12 / 79 qualified dynamics samples show actual growth past the former
  ceiling in RR1/2/3. Qualified `scale * max(depth,.25)` matches focal within
  .00001. At/beyond depth 2.5 the proportion bound still holds. Unqualified
  profiles are tested separately and cannot inherit the near extension.
- RR1 straight-pass rider FF23AA has host pixels throughout frames 2611..2635,
  exits on 2636 and does not re-enter through 2660. RR3 car FF19B2 completes its
  whole-canvas exit on 3850 and stays gone through 3875, including braking.
  The old RR1 far-cutoff gap 3162..3166 still contains drawn pixels.
- Nine native groups and 24 local Python tests pass. New native fixtures check
  finite .25 projection, proportion at the calibration plane, continued growth,
  actual mirror uploads, whole-image monotonic departure and no re-entry.
  Four native/enhanced video comparisons and three original-PCM pairs pass.
- Private previews: `vehicle-entry-final/rr1-entrada-na-tela.gif` and
  `rr3-entrada-na-tela.gif`, normal speed, synchronized prior/current Mega Drive
  frames. They are not captures of Super Hang-On or a CD-console release.
- Final binaries: `build/release02/bin/rrN/rrN.exe`. Comparison baselines were
  preserved in `build/vehicle-pass-baseline/`. New build/test logs use the
  `pass-*`, `entry-*` and `rrN-entry-*` names. Current local package directory:
  `dist/0.50-approval-entry/`; earlier lane/hills candidates are historical.
  Next step: owner playtest of this candidate, then explicit GitHub approval.

### Owner gameplay recording: rival transitions

The owner supplied a local RR1 recording on 6 November, approximately 206.233s,
3840x2160, 30fps. Private review is under `build/validation/owner-rr1-video/`:
overview samples throughout and consecutive frames from 24..27, 31..34,
72.5..75.5 and 158.5..161.5 seconds. At about 25.8s the rider is visibly running
to the left of a separated bike. Do not classify that scene solely as a missing
mounted torso. It also exercises a genuine crash/separation animation, which
must not inherit a rigid riding model. The recording has no input log/RAM dumps;
the scripted evidence below is comparable regression coverage, not an exact
reconstruction of the owner's inputs or a claim that every video symptom has
one proven cause. Review the current candidate with the owner.

Implemented in the shared `src/presentation/sprite_geometry.cpp`:

- A last-difference lateral forecast could overshoot a reversal and snap back
  on the next native update. Use the two latest native intervals: opposite or
  zero signs suppress the unsupported lateral forecast; matching signs use the
  smaller speed. Keep world Z, camera, lane and pose offsets separate.
- `smooth_vehicle_lane` critically damps the live lane correction, evaluated
  once per presentation frame. It follows target velocity without introducing
  a historical framebuffer or delaying pose artwork. With omega=1.25/frame,
  e = displayLane-(targetLane-targetVelocity*dt), u = displayVelocity-targetVelocity,
  k = (u+omega*e)*dt, decay = exp(-omega*dt):
  nextLane = targetLane+(e+k)*decay;
  nextVelocity = targetVelocity+(u-omega*k)*decay.
  Clamp lane error to ±8*depth world units (eight screen pixels). Reset on an
  uninitialized/invalid interval, a gap >12 frames or a displacement >64*depth.
  Repeated preparation in the same frame reuses the value; do not run the spring
  again per scanline. The controlled player's pose/input path stays native.
- A cached focal calibration was previously never cleared when a later forward
  appearance used another projection convention. Requalify 2.5/3 against each
  forward appearance's scale/depth. Unrecognized measured appearances retain
  their current source X/Y/scale and invalidate riding ground/departure/lane
  state. Synthetic legacy packets with no camera still use the old generic
  predictor; they are not evidence of a qualified game-world projection.
- `rigid_vehicle` rejects a single vehicle model when components have different
  scale or substantially separate placement. Detached rider/bike cannot be
  predicted as one rigid image. Native animation/placement is retained until
  a riding appearance qualifies again. Rediscover component roles for a new
  game instead of assuming actor identity implies one rigid rendered object.
- Native SAT order can be the reverse of descriptor component order. Sort source
  parts by their matched native job's first SAT slot before qualification,
  recovered-margin composition and direct drawing. The first opaque native
  part wins. Sorting only the final draw still gives incorrect qualification
  or recovered pixels. A new overlapping-overlay fixture checks recovered
  x=324 is the front overlay's color 34, not the underlying body color.
- Prefer `ActorScope::sources` from the current builder invocation. Use the
  broader same-frame early collection only when scoped observations are absent.
  Keep genuinely culled parts from the same invocation, without mixing another
  invocation/view into the current animation pose.
- `SourcePacket::render_state` snapshots `(actor[104]<<8)|actor[110]` as a
  conservative change token. Bit 6 of byte +104 is the previously verified live
  flag; +110 is not claimed to be a universal off-bike enum. Requalify these
  fields for another game. `live_gap` bridges an empty mid-distance builder only
  with current/recent active state, identical change token, qualified raw ground,
  depth 2.5..28, packet freshness <=12 and appearance age <=12. Current crests
  still clip it. State changes, deactivation, stale data and removal do not
  authorize a hold. Do not extend stale artwork indefinitely to hide a blink.
- `RR_ACTOR_PACKETS` optionally writes per-part source packet diagnostics.
  `traffic.csv` now includes `live_gap` and `lane_target`; `lane_world` is the
  corrected lane. Geometry identity alone does not prove visible pixels; retain
  `RR_VEHICLE_COVERAGE` and actual raster/player audits.

Verification and exact continuation:

- Current checker `tools/check_rival_transitions.py`; private aggregate with
  executable/source hashes `build/validation/owner-rivals-complete/result.json`.
  Thirteen matched replays preserve 9,408 RAM/VRAM snapshots and 4,704 HUD frames.
  Two attack/steering windows, dynamics and touring run in every game, plus RR1
  near approach. All audited player corruption, missing-native-actor and blue
  road counters are zero. Earlier near perspective, far-cutoff and no-mirror
  re-entry checks remain. Four video pairs and three original-PCM pairs pass.
- Across recorded traces, lane correction >.01px occurs in 13,693 / 3,740 / 8,435
  observations in RR1/2/3. The eight-pixel bound holds (CSV rounding tolerance).
  Four live mid-gap observations occur in RR1; native fixtures cover the other
  hold safeguards. Counts include logged warmup/overlapping replay histories;
  do not describe them as unique racing frames.
- Nine native groups and 24 local Python tests pass. New pixel fixtures test
  reversal, changed focal convention, independently placed/scaled components,
  native overlay order beyond the original viewport, transient gap/state change,
  crest and stale data. The new reversal fixture was also compiled against the
  previous packaged engine and failed there; current code passes. Old comparison
  build/log: `build/rivals-old-regression/`, `build/rivals-old-regression.log`.
- `rivals` in `tools/probe_visual_regressions.py` is 480 consecutive frames:
  attack C for 30 frames every 60, left/right/neutral steering phases of 48.
  Default wait 500; `--wait 1400` gives the later window. New baseline captures
  are `owner-rivals-review/rrN/before/rivals` and `before-late/rivals`, with the
  previous binary tree at `build/owner-rivals-baseline/`. `owner-rivals-fixed`,
  `observed` and `confirmed` were intermediate runs; final evidence is **complete**.
  Preserve historical hashes/reports. Build/test logs are `rivals-*`, final probe
  logs `rrN-rivals-*`; current code must not be relabeled using an old result.
- Normal-speed previews: `owner-rivals-complete/rrN-adversarios.gif`. RR1 shows
  the combat window, RR2/3 show dynamics with actual vehicles in view. Comparisons
  use identical inputs and 50fps/20ms GIF timing; do not show a vehicle-free view
  as evidence of corrected rival animation.
- Current binaries are `build/release02/bin/rrN/rrN.exe`, used by `Jogar.cmd`.
  Package at that checkpoint: `dist/0.50-approval-rivals/`, now historical.
  The subsequent native-anchor follow-up supersedes it. Publication still needs explicit
  approval. Do not infer campaign/all-state perfection from these replays.

Reusable video-analysis setup: local PyAV 19.0.2 cp312-abi3 Windows wheel in
`.tools/video-python`, verified against the official PyPI SHA-256, and Pillow in
`.tools/gif-python`. The embedded Python has no pip; wheels can be hash-checked
and unpacked with `python -m zipfile`. AV seeks use the video's stream time base;
decode to the requested timestamp after a keyframe seek. Resize in the decoder
before making private contact sheets. A 30fps compressed CRT recording cannot
identify every individual 59.94Hz native frame or prove guest-state corruption.
Keep the owner's media local; these are QA tools, not runtime dependencies.

### Starting-grid teleport and native pose anchors

Latest owner recording: `Gravação de Tela 2026-11-06 193745.mp4`, 135.933 seconds,
3840x2160, 30fps. Private overview/detail exports are in
`build/validation/owner-grid-video/`; consecutive windows start at 2.7, 5.5,
34 and 88 seconds. The earlier 18:39 recording and its evidence remain separate.
Neither is an exact input replay. No owner video/image belongs in public ZIPs.

Reproduction and root cause:

- The nearest stationary grid rival has a real native anchor offset from the
  generic road/lane projection. `vehicle_sample` formerly clamped X residual
  to +/-8 pixels and zeroed residuals <=2. Model adoption thus teleported an
  otherwise stationary actor. This was not an AI decision or a countdown flag.
- RR1 actor `$FF232A` at camera 7.5/world Z 8 has depth 2.5, source X=102,
  source Y=138, scale=1, lane=32 and roadWorldX=-120. The generic X model gives
  124.8, a genuine -22.8px residual. Clamping to -8 yielded 116.8: a 14.8px jump.
  RR2/3 had 27.6px maximum grid error. The old raw-Y residual test <=8 could
  leave a previous pose's height bias active through a close pose transition.
- Actual RR1 capture index 194 still shows the pre-adoption frame; index 195
  reveals the old sideways jump. Don't confuse the trace's calculation tick
  with the already-produced screenshot. Direct rider-mask pixels confirm this.

Current algorithm, implemented entirely in shared host presentation:

```text
sourceDepth = 2 + sourceWorldZ - sourceCameraZ
currentDepth = max(.25, nativeDepth + predictedWorldDelta - cameraDelta)
worldOffsetX = (sourceX - modelSourceX) * sourceDepth
displayX = modelCurrentX + worldOffsetX / currentDepth
worldOffsetY = (sourceY - modelSourceY) * sourceDepth
displayY = modelCurrentY + worldOffsetY / currentDepth
```

- Source X correction is never arbitrarily clamped to +/-8 or zeroed for a
  two-pixel residual. Store its projected value as `body_offset` in the trace.
  An implausible source residual >128px rejects riding calibration and keeps
  current source X/Y/scale. The **separate lane damping** bound is still eight
  screen pixels: don't remove that filter or mistake its lag for pose offset.
- Rebind Y to each eligible current forward appearance with source Y<152,
  populated native world Y and absolute source-model Y residual <=64px.
  `ActorCache::height_bias` now contains **world units**, not screen pixels.
  Unsupported fresh anchors invalidate qualification instead of silently using
  a preceding pose's bias. Once source feet clip at >=152, retain the last
  qualified world correction to allow the near pass to continue naturally.
- `RoadRays::has_height` is independent of `has_x`: set it only when the native
  world-Y observations are populated, not an all-zero table. Include it in road
  snapshot equality. Runtime raw-ground qualification checks it; pure road math
  functions can still operate on explicit synthetic profiles without the flag.
- Current ground/hill occlusion, complete source composition, latest pose art,
  focal requalification, causal camera and lane prediction still apply. Keep
  all native fallback/live-state/mirror guards and the finite .25 near plane.
  No new guest hooks, countdown flags, ROM patches or generated-C edits.

Verification and exact continuation point:

- Build `build/release02`, log `build/grid-build.log`; nine CTest groups pass in
  `grid-native-tests.log`, and all 24 local Python tests in `grid-python-tests.log`.
- New organic pixel fixtures at ticks 450..464 verify measured stationary grid
  X/Y, repeated frames, immediate art/palette change and far-to-close Y rebinding.
  The old packaged rival engine fails the new center assertion while current
  engine passes. Isolated previous-source build/log: `build/grid-old-regression/`
  and `grid-old-regression.log`; old source is copied to use current struct headers.
- `grid` is a new replay scenario: wait 0 after normal setup, capture 480
  consecutive frames, holding accelerator. RR1 contains loading/countdown before
  real grid output; its audit includes 82,352 player pixels, so this scenario's
  minimum is 80,000. Other existing scenarios still require >100,000 pixels.
- Sixteen final paired replays: grid, early/late combat-steering, dynamics and
  touring for each title, plus RR1 near approach. 6,144 frames, 12,288 unchanged
  guest RAM/VRAM snapshots, identical HUD throughout. All audited corruption,
  missing-native-actor and blue-road counters are zero. First 16 traced grid
  frames give 112 NPC observations per game, all exact source X/Y at CSV precision.
- RR1 actual blue/yellow nearest-rider mask versus native: IoU .091 before,
  .812 current at adoption. This localized check complements trace equations;
  it does not pretend enhanced/native whole images should match.
- Existing lane-lag/projection, near-scale, far-cutoff actual pixels and no
  mirror-re-entry checks pass. Four new native/enhanced video pairs (RR1/2/3
  wide and RR2 4:3) pass, including pause/resume; original PCM remains byte-identical
  off/on in all three. No extra gameplay speed or artwork-frame synthesis.
- Checker `tools/check_grid_start.py`; private final report with executable
  SHA-256: `build/validation/grid-final/result.json`. Captures use current final
  binaries, with previous counterparts in `grid-review/rrN/before/grid` and
  `owner-rivals-complete/`. Native grid reference is `grid-review/rrN/native/grid`.
  `grid-fixed/` was intermediate. Final video/audio roots: `grid-final-video/`
  and `grid-final-video-43/`. Baseline exe tree is `build/grid-baseline/`.
- Normal-speed comparison GIFs: `grid-final/rr1-largada.gif` and
  `grid-final/rrN-animacoes.gif`. Current launcher path is `Jogar.cmd`, games
  `build/release02/bin/rrN/rrN.exe`. Current ROM-free source, standard and offline
  review packages: `dist/0.50-approval-grid/`. Technical note:
  `docs/NATIVE-VEHICLE-ANCHORS.md`. Version stays 0.50. Owner review is next;
  there has been no GitHub publication. Don't claim complete campaign coverage.

Reusable lesson: a valid road/focal model does not prove native anchor identity.
Before extrapolating, verify that projecting the **same observed state** returns
the measured pose. Native near/grid/combat paths can have legitimate offsets.
Preserve them in world units with source depth, then project with current depth.
Test cold adoption, stationary grid, pause/resume and pose changes alongside fast
motion. Check actual destination pixels; a finite trace alone is insufficient.

## Reusable Mega Drive recomp knowledge

### Architecture and limits

The project has three independent translated 68000 game targets and one native
Windows launcher. It uses a pinned SegaGenesisRecomp SDK/runtime, with execution
fallback for paths not covered by translation. This is not a claim of complete
static recompilation or full-campaign coverage. The renderer remains a VDP/raster
pipeline with presentation callbacks; it is not a replacement 3D game simulation.

The data flow is:

1. Verify the user's cartridge dump by exact SHA-256; identify a qualified game
   revision, stage a private copy and bind it to the matching observed profile.
2. Generate translated game code locally and compile it with the pinned runtime
   and project adapters. Keep generated C and game inputs private.
3. Execute original game state and evaluate original-width VDP status faithfully.
4. Observe native object builders/scalers and road data through read-only hooks.
5. Build host-side geometry, source-art caches and presentation snapshots.
6. Render the enhanced scene in a visual pass, then apply output/CRT presentation.
   Host results must not feed back into authentic SAT collision/overflow flags.

The public distribution follows `BTr-RJ/RedBaron_Recomp`'s local-build model:
integration source, patches, documentation and a ROM-free installer; the user
provides original compatible dumps and builds private game outputs locally.
Only the three revisions in `games/supported-roms.json` are supported. Japanese
releases, other revisions, modified images and interleaved SMD are unsupported.
File names, CRC-only matching and renamed dumps cannot authorize revision hooks.

### Reverse-engineering a different game safely

Do not transplant this trilogy's addresses or constants into another title.
First establish its revision hash, boot path, memory layout and native reference.
Use generated C/disassembly as *private research*, instrument confirmed PCs,
and verify the expected opcode bytes before enabling each hook. Observe the
CPU's register/RAM state at the instruction boundary; generated C line numbers
and function splits are not a stable ABI. Keep runtime changes reproducible in
the patch pipeline, rather than editing generated game C or an extracted SDK
file that the next build will replace.

Map, separately: player actor, opponent/traffic pools, object identity, builder
entry/exit, descriptor stride/fields, sprite source tables, staging RAM, SAT/VRAM
commit, camera coordinates, track geometry, visibility flags and screen state.
Record menu, race, pause, crash, combat, curves, hills, near pass and each camera
view. An actor ID is not necessarily a unique rendered instance: mirrors,
split screen and repeated scenery may reuse it or a descriptor slot.

For this project, pinned actor observation PCs are:

| Game | Builder begin | Builder end | Uncropped/pre-cull observation | Player actor |
|---|---|---|---|---|
| RR1 | `$F4CA` | `$F44C` | `$DC58` | `$FF1EAA` |
| RR2 | `$15734` | `$156B6` | `$12642` | `$FF0A62` |
| RR3 | `$1B032` | `$1AFAC` | `$17DF6` | `$FF09B2` |

See `src/game/game_spec.c` for hook order and opcode guards. Builder-end can be
a loop boundary closing the previous actor, not a conventional C function return.
Original hooks continue executing the native instruction; presentation metadata
must never overwrite CPU registers, guest descriptors, game speed or collisions.

### Sprite data and whole-pose ownership

Qualified descriptor strides are 20 / 22 / 32 bytes. Common fields used here:
X at +0, Y at +2, scale at +4 divided by 16384, clip at +6, image at +8,
flags at +10, relative 8.8 Z at +14, secondary image at +18. These meanings and
the source/scaler conventions must be rediscovered for a new game.

The RR source image format has optional signed anchor bytes when flag `$80` is
set, then width/height/palette/header and nibble pixels. Row stride is
`ceil(width/2)+1`; the extra byte is a guard, not another pixel. Unanchored art
defaults to center X / bottom Y. Native SAT palette, priority, flip and optional
extended palette data are authoritative for assembled animated vehicles.

One motorcycle can comprise multiple independently uploaded body/bike pieces.
Treat its current pose as a whole assembly. Do not redraw a generic full torso
over the animated native torso, or combine old torso/new bike parts. Exact
native headers, ownership and pixel hashes distinguish a complete upload from
a staging transition. Retain a complete pose through a qualified transition;
never suppress all native pieces unless a valid replacement actually renders.

Use stable actor identity plus component, update epoch and camera/view identity.
A descriptor address or SAT index alone is reused and can attach old scenery,
another actor or mirror data to the wrong object. Repeated scenery jobs may
contain several copies and paired images. `scenery_copies` preserves each
instance, placement, mirroring and palette; unknown/subpixel replacements keep
their native assembly. Source-art pointers live in a stable asset map; cached
source packets/appearances are values, not dangling deque-element references.

### Perspective, road geometry and timing

Perspective scale is proportional to `1/Z`; it is rational, not exponential.
Interpolate/predict depth, then project. Linear interpolation of screen scale
or unrelated X/Y easing produces the wrong approach speed. Differentiate world
motion, camera motion, lane position, banking/animation offsets and road curves.

For qualified Road Rash vehicles:

```text
depth = worldZ - cameraZ + 2
scale = focal / depth             focal = 2.5 rider, 3 car
q = depth - 2 + fraction(roadCameraZ)
roadWorldX/Y = linear samples of the native interleaved table at q
screenX = 160 + (roadWorldX + actorLateral + worldPoseOffsetX) / depth
screenY = nativeHorizon + (roadWorldY - actorHeight + worldPoseOffsetY) / depth
qualified near artwork scale = focal / max(depth, .25)
unknown-ground artwork scale = min(focal / depth, focal / 2.5)
```

The X projection does not multiply road/lane coordinates by the artwork focal
factor a second time. Actor lateral/height are signed words at +48/+50 here.
World pose offsets are measured from source-anchor residuals multiplied by that
source pose's depth. Reproduce native source X/Y before extrapolating. Keep
source placement separate from the eight-pixel lane-filter lag bound. Require
populated native world Y for runtime ground calibration; source Y>=152 is clipped
and must not overwrite the last qualified world-height correction.
The depth-2.5 calibration preserves a rider's authored scale at that plane;
cars keep their calibration and intrinsic image dimensions. Beyond it, closer
vehicles must continue growing with their position. This supersedes the hills
candidate's erroneous fixed maximum at that plane. Never restore the retired
18% enlargement or solve a
near-plane problem by reverting to an older cached bitmap's size.

Clipped road Y is a different quantity from unclipped vehicle ground height.
The RR1 clipped table is `$FFD77C`; RR2/RR3 use pointers at `$FF1CE4`/`$FF1CDC`.
Use it to invert visible terrain depth and derive the occluding crest. Use
unclipped world Y for vehicles approaching the HUD/near plane; a clipped
bottom row must not pin their feet to line 152. Hill masks belong to the current
screen raster and do not scale around a sprite anchor. The HUD boundary also
stays fixed after scaling. Keep viewports separate so mirror art at Y≈192/216
cannot replace the forward vehicle or create a canvas spanning both mirrors.

Use confirmed timestamped samples and bounded prediction, without a historical
framebuffer buffer. A prediction queried again after a later measurement may
change retrospectively: that is not a new physical sample. The vehicle camera
therefore has its own causal observations and a single presentation value per
frame. Braking must not make a previously forecast forward camera jump backwards.
Reset on real discontinuities/scene changes; distinguish those from acceleration.
Preserve the controlled player's latest native pose and input response.

### VDP, widescreen and raster pitfalls

Original-width status evaluation and expanded visual drawing are separate.
Keep authentic sprite overflow/collision results for the guest. H40 uses the
proper SAT table mask, 9-bit X/Y and the 128-pixel bias; VRAM addresses wrap at
16 bits. Horizontal fetch is a 10-bit coordinate with signed representatives.
Preserve the correct representative around half-turns; do not cross ±512 through
an ambiguous continuation. HV/HBlank timing remains an NTSC approximation,
not a claim of complete hardware-cycle validation.

The road is a finite bitmap in these games. Periodically wrapping its 1024-pixel
plane exposes the opposite edge as another road. In RR1/RR2, each plane
contributes one outward half; extend authored ground at its 8-pixel phase,
preserve sky/mountains and use the same corrected pair for road sampling,
center and markings. Continue an actual clamp using confirmed current-frame
tangents. A hard line-72 cutoff failed on higher curves; early rows now require
authored-ground confirmation. RR3 has a separately qualified legacy road path.
Do not apply a finite-bitmap patch to a genuinely repeating floor in another game.

Never interpolate old vertical scroll independently from the current road
bitmap. It can expose raster command bytes as blue stripes on hills. Current
terrain UV uses the road's inverse depth. Preserve authored distant grass detail
when world texels become subpixel; replacing it with a dominant color hid grass
markings that reappeared on pause. Central paint must be replaced as a complete
per-row set, with the same road center; partial replacement duplicated yellow lines.

### Approaches that failed here

- Optical-flow/framebuffer warping could not reconstruct road or sprite occlusion.
  `RR_SMOOTH` is retired; do not reintroduce it as an engine fix.
- A five-frame visual buffer added lag. Current presentation has no such buffer.
- Full ROM artwork over all animated actors without palette/whole-pose checks
  duplicated torsos, hid traffic or lost current combat animation.
- Native-only clipped bitmaps cannot recover missing margins after zoom. Restore
  qualified source data before presentation scaling, then clip the destination.
- One billboard per scenery job discarded repeated trees, houses and poles.
- Fixing only one road edge, delaying A/B halves separately, or replacing only
  some marking sprites produced corner seams and doubled center paint.
- Raising generic near-scale bounds avoided one shrink but permitted oversized
  actors. The reference must be physical depth/player plane, not old bitmap size.
- Fixed screen-center lateral prediction confused road steering with NPC motion.
- Mirror and forward poses sharing one actor cache produced spurious re-entry.
- A trace entry with nonzero bounds is not evidence of nonzero rendered pixels.

### Evidence workflow and reusable debugging skills

Build one reproducible native reference before changing a renderer. Use the same
ROM hash, input script, game state, aspect mode and settings on both sides.
Add the triggering case, not just the easy straightaway: countersteering,
brake/accelerator changes, road crests, a near pass, combat, pause/resume and
native culling transitions. Keep commercial captures and RAM/VRAM private.

Native status/RAM/VRAM equality proves simulation preservation; it does not prove
visual correctness. Verify HUD/menu/pause, actual player ownership/occlusion,
source replacement coverage, lane-relative position and genuine pixel continuity.
Zero pixels can be correct behind a hill or player. `RR_VEHICLE_COVERAGE` counts
host actor output only; account for native fallback before calling any absence
a blink. Do not force physically off-road NPCs onto asphalt to hide a projection bug.

Screenshots, input requests, end-of-frame dumps and upload hooks can represent
different points in the same native frame. Parse the probe log timestamps. Do
not compare a next-upload VRAM snapshot to an earlier raster and declare its
actor missing. `RR_GEOMETRY_AUDIT` observes actual raster colors/owners and avoids
that false conclusion. A green general suite does not replace reproducing the
owner's new visual report.

Current recipes (run from the repository root; create fresh output folders):

```powershell
$env:RR_VISUAL_VALIDATION='build/validation/my-repro'
.tools/python/python.exe tools/probe_visual_regressions.py rr1 --label before --scenario dynamics --wait 1000
# After rebuilding: same command with --label fixed
# RR2 wait 900, RR3 wait 1800 reproduce the current dynamics comparisons.
$env:RR_VIDEO_VALIDATION='build/validation/my-video'
.tools/python/python.exe tools/probe_geometry_video.py rr2 --frames 60 --wait 900 --wide
.tools/python/python.exe tools/probe_geometry_audio.py rr2
.tools/python/python.exe tools/check_grid_start.py
.tools/python/python.exe tools/audit_release.py dist/0.50-approval-grid
```

`dynamics` captures 480 consecutive frames, switches steering every 24 and
alternates braking/acceleration. `touring` captures 48 states 150 frames apart;
use it for a broader first-track sweep. `--native` disables enhanced geometry;
`--build` can select a separately preserved executable tree. Use UUID capture
names so simultaneous read-only probes do not overwrite each other's outputs.

Capture *current binaries*, then generate their aggregate/hash report. Freeze
historical evidence. Some old checkers calculate hashes from the current build;
rerunning them on old captures can falsely relabel old evidence with new hashes.
Version-specific checks such as the 0.4.3 road checker are historical tools.
Current reports/checkers are named in the latest follow-up section. GIF review
exports sample captured frames at 50fps/20ms to preserve normal elapsed time;
they do not invent animation frames or slow gameplay down.

### Launcher, installation, audio and delivery lessons

The 0.50 launcher is native Windows/GDI+, English, sober dark, with classic
covers and an original motorcycle masthead. Fonts/art provenance and the saved
generation prompt live under `assets/launcher/`; PRODUCT.md/DESIGN.md explain
the accepted design. Keep GDI+ resource lifetime, per-panel fitting/DPI, UTF-8
resource compilation, native dialog keyboard routing and visible checkbox labels.
Use the launcher's QA commands above rather than unrelated desktop captures.

Two original player ports have remappable keyboard/controller profiles through
SDL/InputMap. Save/Cancel, reserved runtime keys, controller axes/slots, deadzone
and reload are implemented. Virtual-controller tests validate the backend;
specific physical hardware still requires real testing. Do not overwrite normal
user settings while running QA.

The installer scans user-specified ROM folders, verifies hashes, stages private
copies, bootstraps pinned tools/SDK caches and builds locally. It supports a
subset of supplied games, stop/retry, recognized update destinations and Unicode
paths. It never modifies original ROM folders or downloads games. Standard and
offline installers differ in included tool caches; fully offline use still
requires an existing Microsoft C++ toolchain. The optional signed Microsoft
installer uses Windows administrator consent. Cancellation stops child workers.

Original music remains selectable. The latest owner follow-up bundles the
mapped remake pack in Windows installer ZIPs, with source credits and hashes.
RR1 result cue 9 falls back to Redwood Forest cue 7 for older local packs;
explicit cue 9 overrides that fallback, and RR2/RR3 must not receive the alias.
Use original PCM off/on comparisons to establish timing independently of remix
decoding. The Snoopy screenshot/audio report in the chat was explicitly unrelated.

Package using the public allowlist, file manifests and SHA-256, into a fresh
directory. Never upload this workspace wholesale. Only launcher/installer
binaries and approved SDK archives belong in public packages; ROMs, generated
game code, captures, saves and raw private audio downloads do not. Only the
explicitly selected verified remake pack enters installer ZIPs. Preserve existing upstream
PolyForm/noncommercial and third-party notices; do not invent an MIT grant or
claim distribution rights for game content. The release remains **0.50** and
awaits concrete owner approval before any GitHub push/repository/release/upload.

### Skills and future-project execution

Reusable engineering competencies acquired here include 68000 hook qualification,
VDP/raster analysis, stable actor/view identity, native/source composition,
inverse-depth projection, causal prediction, world/camera separation, visibility
hysteresis, Windows build/install engineering, audio determinism and pixel-level
regression design. Apply the method, not the trilogy's literal offsets.

Relevant optional agent skills/workflows: Impeccable for native launcher design
and review, imagegen for new original bitmap art, computer-use for actual app
interaction when a purpose-built QA command is insufficient, and agent-reach
for explicit primary-source research. Read their current SKILL.md/catalog when
applicable; do not hard-code a developer's plugin cache path or assume a skill
is loaded. Engine-only C++ work does not require a frontend/design skill. Earlier
UI review agents are historical; they do not authorize automatic delegation.

For a similar project: qualify inputs → establish native execution/status → map
real render data/views → observe without mutating the guest → implement one
presentation layer → add the actual regression → compare state, pixels and audio
→ build a ROM-free deliverable → document boundaries and limitations → obtain
publication approval. Prefer current primary hardware/SDK sources. MAME's Sega
sprite devices were a conceptual Super Hang-On/OutRun reference here; no MAME
renderer code was copied. Road Rash's art/pose cadence and logic remain its own.

For the next session, preserve this file and current source/report/package hashes.
Start by checking whether the owner tested the latest candidate and supplied a
new reproducible case. Use its game/revision, track/time, input pattern and video
mode to extend the probes. Continue locally; do not restart solved work, revert
older road/scenery/player fixes or publish an earlier candidate by mistake.

## Non-negotiable workspace boundaries

- Do not commit `.local`, `.tools`, build/dist outputs, ROMs/ZIPs, translated
  game source, saves, tokens, remake music or desktop screenshots.
- Pinned ROM hooks verify exact opcodes and hashes; reject revision mismatch.
- Modify runtime patches/integration source, never generated game C.
- Keep public SDK archives, source URLs, hashes, license texts and asset origin
  metadata. Original masthead prompt and cover origins are preserved.
- Local legacy setup/bootstrap/rr.py research tools remain available, but the
  public 0.50 installer uses `public-tools.lock.json` and `install.ps1`.
- PowerShell: use LiteralPath for file operations. Before recursive deletion or
  movement, resolve and verify the absolute target stays in the intended folder.
- Do not touch unrelated running apps/projects. A Snoopy screenshot/audio
  report in the conversation was explicitly sent to the wrong project.
- User approval is required only at the final GitHub publication boundary,
  after implementation, validation and review artifacts are concrete.

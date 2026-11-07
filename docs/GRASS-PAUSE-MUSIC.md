# 0.50 review fixes: grass detail, paused road and RR1 results music

These corrections supersede the first 0.50 approval candidate. The launcher and
installer version remains **0.50**. Publication still requires owner approval.

## Paused RR1 road

Turning off organic rendering on pause also removed the finite-road correction
and current-raster edge continuation. The expanded left side could then show a
clamped vertical edge / second road wedge. Resuming restored the correction.

Paused wide races now keep only spatial road continuation from the current
scroll table. They use no motion prediction, sprite scaling or world material
sampling. The finite ground masks also remain available in exposed margins
without a material callback. Menus and split screen do not inherit this state;
4:3 pause retains native rendering. Guest VRAM and hardware sprite status are
still evaluated through the unmodified native pass.

`rr_geometry_frame` distinguishes native screens (0), live races (1) and paused
races (2). The adapter supplies this state; the VDP integration remains a
reproducible patch against the pinned SDK.

## Distant grass in all three games

The old minification filter replaced a distant two-shade tile with its dominant
color when the world footprint became large. This erased the authored stipple
at the horizon, while pausing exposed the complete original raster texture.

World sampling now transitions back to the authored raster sample as texels
become subpixel. A fixed coverage mask blends the two sampling regimes over a
bounded footprint interval. It preserves far-field markings without point-sampled
UV shimmer or a hard row at the transition. Asphalt, HUD and visible sprites
remain outside this material replacement.

## RR1 results / victory music

RR1 results request music cue **9**, through the native calls at `$62C6` and
`$867A`. Redwood Forest's race uses cue **7**. Local manifests now map both to
the same user-provided Redwood Forest remake. For older manifests with only
cue 7, the runtime supplies cue 9 from the same file. An explicit cue 9 entry
takes precedence. Other games and missing-file original-audio fallback are
unchanged. No additional music recording is downloaded or distributed.

## Verification

Three regression cases failed before these changes: paused RR1 finite margins,
far grass minor-shade preservation and RR1 results-cue playback. All nine native
test groups passed after correction, including actual Media Foundation decoding,
older-pack alias behavior, pause/resume, and the paused spatial-only callback.
The existing 24 Python source/installer tests also passed.

Real game replays captured 24 consecutive frames, pause and resume, with geometry
off/on in all three games at 16:9 and in RR1 at 4:3. All comparisons passed:
RAM/VRAM, HUD and non-racing screens retain the required off/on invariants.
Before/final guest RAM/VRAM also match for each game, before pause and after resume.

In matched far-field patches of the first racing frame, the authored minor shade
was entirely absent before and restored after:

| Game | Detail pixels before | Detail pixels after | Native reference |
|---|---:|---:|---:|
| RR1 | 0 | 47 | 50 |
| RR2 | 0 | 48 | 52 |
| RR3 | 0 | 56 | 56 |

The RR1 paused left-margin fixture had 28 incorrect grass/edge pixels across
the sampled bottom row before; the corrected row continues asphalt throughout.
The results music path is covered by the native request signature and decoder
tests, rather than a claimed complete campaign playthrough.

Private evidence: `build/validation/grass-pause-before/`,
`grass-pause-final/`, `grass-pause-final-43/`. The final summary is
`build/validation/grass-pause-final/result.json`.
`tools/check_grass_pause.py` audits the matched captures and guest state.
Do not publish those working trees: they contain private RAM/VRAM dumps.

# Native vehicle anchors and the starting grid

0.50 local follow-up, 6 November 2026. Shared correction for RR1, RR2 and RR3,
after the owner's 19:37:45 RR1 recording. It supersedes the small native-anchor
correction limit in earlier lane/rival candidates. Their lane filter, whole-pose
ownership, near growth, mirror separation and live-state guards remain.

## Reproduced defect

The stationary starting grid exposed a projection mismatch. The native close
vehicle path includes genuine placement offsets relative to the road model.
Clamping their horizontal residual to eight pixels made an unchanged rider jump
when enhanced rendering adopted it. At the first grid observations, maximum X
error was 14.8 pixels in RR1 and 27.6 in RR2/3. A separate eight-pixel Y
qualification could retain a preceding pose's height correction instead of
binding the current unclipped pose, also affecting close animation transitions.

The supplied recording is 135.933 seconds, 3840x2160 at 30 fps. Overview samples
and consecutive windows around 2.7, 5.5, 34 and 88 seconds were reviewed. It is
a visual reference, not an exact scripted replay. Owner media and native game
captures stay private.

## Correction

For each qualified complete source pose, measure its native anchor against the
road projection **at that pose's own depth and road snapshot**:

```text
offsetWorldX = (sourceX - modelSourceX) * sourceDepth
offsetWorldY = (sourceY - modelSourceY) * sourceDepth
displayX = modelCurrentX + offsetWorldX / displayDepth
displayY = modelCurrentY + offsetWorldY / displayDepth
```

At equal source/current state the equations reproduce the measured anchor.
Changing depth projects its correction with the same perspective as the entire
pose, rather than adding a constant screen-space bias or discarding it.
Horizontal residuals are no longer clamped to eight pixels or zeroed below two.
An implausible residual above 128 pixels rejects the riding model and retains
the measured source pose. The independent eight-pixel bound on **lane filter
lag** remains; source artwork placement is a different quantity.

Bind raw Y only to a populated native world-height table and an unclipped
forward source anchor with Y < 152, with residual at most 64 pixels. Rebind
each fresh qualified source pose; invalidate unsupported fresh anchors. Store
the bias in **world units**. Once native feet clip at the HUD, retain the last
qualified world bias so the vehicle can continue growing into the camera.
`RoadRays::has_height` distinguishes populated world Y from valid road X;
an all-zero/uninitialized height table cannot qualify raw-ground zoom.

Current hill masks still clip the complete destination pose. Latest animation
artwork remains immediate. No guest AI, speed, physics, collision, input,
animation clock or audio changes were made.

## Verification and review

- Nine native test groups and 24 local Python tests pass. New rendered-pixel
  fixtures cover stationary model adoption, repeated grid frames, immediate
  artwork changes and returning from a far pose to a close native anchor.
  The preceding packaged engine fails the new grid fixture; current code passes.
- All 336 first-grid observations across the three games now reproduce source
  X and Y exactly at trace precision. The actual RR1 blue/yellow rider mask at
  adoption improves from 0.091 to 0.812 overlap with native rendering. This
  localized pixel check complements the trace; it is not full-image similarity.
- Sixteen matched grid, combat/steering, dynamics, touring and near-pass replays
  retain 12,288 identical RAM/VRAM snapshots and 6,144 identical HUD frames.
  Raster audits report zero player corruption, missing native actor pixels or
  blue-road artifacts in these runs. RR1's initial loading/countdown interval
  has fewer player observations; its audit still covers 82,352 player pixels.
- Lane lag remains bounded to eight screen pixels. Qualified scale remains
  `focal / max(depth, .25)`. Earlier far-cutoff and mirror re-entry pixel checks
  pass. Four native/enhanced video pairs pass, including pause/resume, and
  original PCM is byte-identical with geometry off/on in all three games.

Checker: `tools/check_grid_start.py`. Current private aggregate and executable
hashes: `build/validation/grid-final/result.json`; video/audio evidence:
`grid-final-video/` and `grid-final-video-43/`. Normal-speed comparisons:
`grid-final/rr1-largada.gif` and `rrN-animacoes.gif`.

Current local review package: `dist/0.50-approval-grid/`. Earlier candidates
are historical. Complete campaign/all-state qualification is not claimed;
publication remains subject to the owner's approval.

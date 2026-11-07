# High curves and near-camera zoom correction

Vehicle near-size prediction in this historical correction is superseded by
[road-anchored vehicle projection and proportions](HILL-VEHICLES.md).

0.50 owner-review follow-up, 6 November 2026. This supersedes the first arcade
zoom candidate. The owner supplied RR1 screenshots with abrupt vertical road
edges during off-road views, and reported close sprites breaking their approach.

## Road edges

`road_scroll` and `extend_clipped_edge` previously refused continuation above
scanline 72. In strong curves, the native bitmap can reach its horizontal clamp
much higher. One reproduced RR1 edge reaches its limit at line 54, leaving a
vertical strip to the HUD. Continuing only its lower portion cannot fix it.

Continuation now begins at the actual clamp, with eight preceding samples to
validate the outward tangent. Steeper valid slopes are accepted. Above the old
cutoff, eight authored grass texels must confirm that the row belongs to the
road bitmap; sky/parallax rows retain their native scroll. Both planes and
both clamp directions use this path. The existing finite-pair sampling,
shared road/marking center and current-frame vertical scroll remain intact.

Real RR1 replays reproduce left and right vertical strips. In the selected
lower-screen regions, the stray bright edge pixels went from 346 and 426 to
zero. The right-edge replay holds left steering before capture, rather than
relying on the earlier right-side off-road path.

## Near sprites

A cache guard discarded predicted actor scales above twice the last complete
bitmap's scale, then restored that older bitmap pose. RR2's approaching car
reproduced a scale sequence of `1.247, 1.413, 0.786, 0.786`: the car visibly
shrunk and moved backwards before approaching again.

Valid fresh motion no longer resets to the cached bitmap's size. Bitmap scale
and actor projection are independent. Inverse-depth extrapolation approaches a
finite bound smoothly before crossing zero; it cannot turn into an empty pose
or force native fallback at the near plane. The reproduced sequence is now
`1.247, 1.413, 1.630, 1.925`, continuing out of the viewport naturally.

Qualified wide actor assemblies can sample the original source art directly at
the new scale, avoiding a second scaling of an already reduced bitmap. This
requires the complete current source pose to agree with the native silhouette,
palette and priority (at most 2% mismatched covered pixels). Unsupported poses
keep their complete native assembly. Explicit removals, native animation changes,
player ownership, real hill clipping and the fixed HUD boundary remain active.

There is no additional historical framebuffer buffer. Simulation, input,
collisions and audio clocks retain the original timing.

## Checks and reproduction

- Nine native groups and 24 local Python tests passed. Added fixtures cover
  early left/right clamps, steep curves, monotonic near-plane growth beyond the
  old 2x guard, and recovery of source detail lost by native minification.
- Seven real-game replays: RR1/RR2 touring and close approach, RR1 late steering,
  RR1 left-side off-road/rightward road, and RR3 close approach. All 2,592
  RAM/VRAM snapshots and 1,296 HUD frames match their reference runs.
- Raster audits report zero corrupted player pixels, missing actor pixels or
  blue-road artifacts in those replays.
- Four native/enhanced 60-frame comparisons passed: RR1/2/3 wide and RR2 4:3,
  including title, pause, resumed state and HUD checks.
- Original PCM is byte-identical with geometry off/on in all three games.

Private aggregate: `build/validation/curve-zoom-final/result.json`.
`tools/check_curve_near_zoom.py` verifies the matched states, both edge regions,
the actual RR2 scale-reset sequence, raster audits, video and audio results.
`tools/probe_visual_regressions.py` now accepts `--wait` and `--steer` for focused
replays. Existing default replay timings are unchanged.

Private review assets: `curve-zoom-final/zoom-antes-depois.gif` (RR2, synchronized
before/after at normal speed) and `curva-antes-depois.png` (RR1). They remain
outside the public packages, together with all commercial game captures.

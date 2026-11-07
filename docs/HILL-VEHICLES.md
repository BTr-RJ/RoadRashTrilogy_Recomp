# Vehicle projection, hills and player-relative proportions

0.50 owner-review follow-up, 6 November 2026. Supersedes the vehicle prediction
and near-size behavior of the previous curves candidate. Road-edge fixes,
wide source recovery and native animation ownership remain in place.

Historical candidate: the [vehicle lanes follow-up](VEHICLE-LANES.md) supersedes
its camera prediction, lateral motion and transition handling. The player-plane
size bounds remain. Near ground now uses the native unclipped world Y table;
the clipped road profile continues to provide terrain and crest masks.

## Causes

The preceding renderer extrapolated vehicle height around a fixed horizon at
y=64. That approximation changes a car's or rider's ground anchor when a hill
changes shape. Its scale could also grow far past the player's size: there was
an additional 18% approach gain, and the near-plane predictor permitted several
times the latest native scale. During a staging/upload transition, fallback
could then select the older bitmap history and move the actor backwards.

## Current projection

`SourcePacket` records actor world Z, camera Z and the native 32-point road
profile. Measurements from all three verified games identify the vehicle
projection as `scale = focal / (worldZ - cameraZ + 2)`, with native calibration
2.5 for riders and 3 for cars. `vehicle_sample` recognizes that calibration and
advances relative depth over at most one observed update interval.

`road_point` projects that depth against the current hill profile. The original
pose supplies its small anchor/rounding offset. Height is no longer extrapolated
around a flat horizon. If the profile cannot be qualified, the native vertical
anchor is retained while calibrated depth and size remain active.

The player's ground plane, depth 2.5, provides the near-size reference. A rider
at that plane uses its authored 1x size; cars retain their 3/2.5 calibration and
their own artwork dimensions. Closer vehicles move past the player and out of
the viewport without inflating beyond that reference. The extra approach gain
and the legacy `RR_TRAFFIC_ZOOM` override are removed.

A hill's occluding crest remains in screen space. It does not scale around the
vehicle anchor. Complete current source art can be sampled at the new scale
when its colors, priority and silhouette qualify against the committed native
assembly. Qualification allows one pixel of scaler sampling phase, checked in
both directions, with at most 2% unsupported covered samples. Unsupported
recolors or poses preserve the complete native assembly.

An empty builder update may precede removal of the old SAT upload. While that
complete native assembly still exists, its fresh world projection continues
against the road. Once its SAT entries disappear or are reused, that transition
hold ends. It does not mix different torso/bike animation frames.

The controlled player's native pose and input response remain untouched.
There is no historical framebuffer buffer and no change to the simulation,
input, collision or audio clocks. Animation artwork retains its original poses;
the continuous presentation follows depth and the current raster.

## Verification

- Nine native groups and 24 local Python tests passed. New synthetic cases
  cover both directions of grade change, the player-plane size reference,
  stationary crest masking during zoom, native upload transitions/removal,
  source detail recovery and unsupported recolor rejection.
- Six matched replays: touring and fast approach in RR1, RR2 and RR3. All
  1,728 RAM/VRAM snapshots and 864 HUD frames are unchanged. Raster audits
  report zero player corruption, missing actor pixels or blue-road artifacts.
- Touring traces contain 12,526 / 8,528 / 10,143 calibrated vehicle observations.
  Every recorded calibrated scale stays within its player-plane reference;
  7,556 / 5,446 / 6,106 scale steps occur between native updates. Closing
  vehicles never shrink within the same source update interval.
- Four native/enhanced video comparisons passed: all three games wide and RR2
  4:3, including title, pause, resumed state and HUD.
- Original PCM is byte-identical with geometry off/on in all three games.

Private aggregate: `build/validation/hill-vehicle-confirmed/result.json`.
`tools/check_hill_vehicles.py` verifies the matched states, proportion limits,
continuous scale, hill anchoring, raster audits, video and PCM. The optional
traffic trace includes source/world depth, calibration, crest and anchor fields.
Private normal-speed preview: `hill-vehicle-confirmed/aproximacao-proporcoes.gif`.
Commercial captures and RAM/VRAM evidence remain outside public packages.

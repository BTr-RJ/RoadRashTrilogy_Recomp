# Near-camera approach and sprite-scaling references

0.50 local owner-review follow-up, 6 November 2026. Supersedes the maximum-size
rule in [the hills](HILL-VEHICLES.md) and [lanes](VEHICLE-LANES.md) candidates.
Their lane, whole-pose, mirror, live-cutoff and road fixes remain in place.
For the latest grid and close-pose anchor qualification, see
[native vehicle anchors](NATIVE-VEHICLE-ANCHORS.md).

## What made passing vehicles feel flat

The former model used native depth for X/Y but stopped artwork growth at depth
2.5: `scale = focal / max(depth, 2.5)`. A second cap enforced the same limit in
the draw preparation. Vehicles moved down and out while remaining the same size.
That calibration plane was incorrectly also a maximum-size plane.

Qualified vehicle artwork now uses `scale = focal / max(depth, 0.25)`. At depth
2.5, rider scale is still 1 and car scale 1.2. At a smaller depth the vehicle is
closer to the camera and continues growing. There is no extra traffic multiplier
and no enlargement of distant or side-by-side vehicles at the calibration plane.
Native road X, actor lane and ground height keep using the same depth. Unknown
ground profiles retain the conservative old cap; the extension requires the
verified unclipped world-ground projection.

Growing an image also moves its roof/helmet upwards relative to its feet. During
the bounded forward departure after a native rear-view transition, advancing
only the feet could therefore reveal the top again on braking or a grade change.
The departure now advances the complete canvas's top monotonically below HUD
line 152. Native depth still drives growth, while the whole-image exit completes
within the existing 32-frame appearance deadline. Mirror artwork is never used
for the forward view. Scale and projection remain finite at the .25 near plane.

## Reference study: what was verified

The references were studied through renderer code, reconstructed game code,
hardware documentation and a creator interview. These are technical references;
the three CD-console versions were not executed or timed in this workspace.
No claim of frame-for-frame visual equivalence or identical animation is made.

| Reference | Evidence | Applicable lesson |
|---|---|---|
| Super Hang-On arcade | MAME config selects System 16B sprites for Super Hang-On; that renderer controls image extent, zoom and viewport clipping. | Preserve the complete source pose and scale before destination clipping. Do not conflate it with Out Run's different sprite device. |
| Road Rash 3DO | Public functional reconstruction projects road objects through a reciprocal-depth table, derives artwork extents using that projection, then maps a CEL destination. Portfolio documents CEL origin and size vectors. | Position, artwork size, anchor and complete bounds must agree. A separate constant near-size cap breaks that agreement. |
| Road Rash PlayStation | Randy Breen identifies the PS1 release as directly derived from 3DO. GPU documentation distinguishes unscaled rectangle commands from textured polygons whose projected vertices can change size. | A sprite's appearance does not imply the console uses a dedicated scalable-rectangle command. Retain a common projected rectangle and anchor in the host renderer. The exact Road Rash PS1 packet sequence was not independently traced. |
| Road Rash Saturn | Sega's VDP1 manual specifies scaled sprites, their fixed zoom point, destination dimensions and two-corner mode. | Scale around the correct authored anchor and clip the destination after establishing full bounds. The exact Road Rash Saturn command stream was not independently traced. |

Sources consulted:

- [MAME Super Hang-On configuration](https://github.com/mamedev/mame/blob/master/src/mame/sega/segaorun.cpp)
  and [System 16B sprite renderer](https://github.com/mamedev/mame/blob/master/src/mame/sega/sega16sp.cpp).
- [3DO Road Rash projected CEL reconstruction](https://github.com/trapexit/3do-decomp-road-rash/blob/cc33365427a7cc193c3b9111a24948e9993bb7ea/src/render_road_projected_cel.c)
  and [world projection](https://github.com/trapexit/3do-decomp-road-rash/blob/cc33365427a7cc193c3b9111a24948e9993bb7ea/src/project_road_world_point.c).
  This project describes itself as a functional reconstruction, not a matching
  original EA source release. Its constants and implementation are not imported.
- [3DO Portfolio: Working With a CCB](https://ext.3dodev.com/3DO/Portfolio_2.5/OnLineDoc/DevDocs/ppgfldr/ggsfldr/gpgfldr/5gpgc.html).
- [Randy Breen interview](https://www.sega-16.com/2022/04/interview-randy-breen-road-rash-creator/).
- [PS1 rectangle commands](https://psx-spx.consoledev.net/dev/ps1/gpu/render-rectangle-commands/)
  and [polygon commands](https://psx-spx.consoledev.net/dev/ps1/gpu/render-polygon-commands/).
- [Sega VDP1 manual: zoom point and scaled-sprite command](https://www.infochunk.com/saturn/segahtml_en/hard/vdp1/hon/p06_10.htm).

The implementation is a local presentation correction using the trilogy's own
verified native data. No external renderer code, assets or runtime dependency
was added. Game speed, logic, input, collisions and native animation poses remain.

## Current validation and preview

- Nine native test groups and all 24 local Python tests pass. The near-pass
  fixture checks size at the calibration plane, continued growth, mirror-art
  exclusion, full-image monotonic departure and no re-entry under braking.
- Seven matched replays: 480 dynamics frames and 48 touring samples per game,
  plus a 240-frame RR1 straight approach. All 3,648 RAM/VRAM snapshots and 1,824
  HUD frames match. Audits report no player corruption, missing native actor
  pixels or blue-road artifacts in these runs.
- Dynamics have 272 / 12 / 79 qualified near-growth observations in RR1/2/3.
  For qualified raw-ground samples, `scale * max(depth, .25)` agrees with focal
  within .00001. At and beyond depth 2.5, the former proportion bound still holds.
- RR1's reproduced rider has uninterrupted host pixels on frames 2611..2635
  and leaves on 2636. RR3's car leaves on 3850, with its complete top moving
  downward through the braking transition. Neither re-enters in the checked
  windows. The earlier RR1 five-frame far-cutoff regression still passes.
- Four native/enhanced video pairs pass: RR1/2/3 wide and RR2 4:3, including
  title, pause/resume state and HUD. Original PCM is identical off/on in all three.

Checker: `tools/check_vehicle_entry.py`. Private aggregate and executable hashes:
`build/validation/vehicle-entry-final/result.json`. Normal-speed before/after
previews: `rr1-entrada-na-tela.gif` and `rr3-entrada-na-tela.gif` in that folder.
Captures and guest memory remain private. The preview demonstrates this change
against the preceding Mega Drive candidate; it is not a capture of a CD-console
version. Owner review remains required before GitHub publication.

# Vehicle lanes, fast steering and forward/mirror separation

0.50 local review follow-up, 6 November 2026. Supersedes the vehicle prediction
in [the hills candidate](HILL-VEHICLES.md), preserving its player-relative size
bounds and the earlier road, scenery, player and widescreen fixes.

Historical candidate: [near-camera growth](SPRITE-APPROACH.md) supersedes the
maximum-size cap and foot-only departure described below. Lane/mirror fixes and
their historical evidence remain; use the newer checker for current binaries.
The later [native-anchor correction](NATIVE-VEHICLE-ANCHORS.md) also supersedes
the small authored placement-offset bound; the independent lane filter remains bounded.

## Corrections

The old X predictor derived lateral motion from screen X around a fixed center.
That mixed curve shape and camera steering into the vehicle's own lane velocity.
All three games now use their native interleaved road-world X/Y tables and the
actor's lateral/height fields. The lane projection is
`screenX = 160 + (roadWorldX + actorLateral) / depth`, with a bounded authored
pose offset. Sprite focal length affects size; it must not magnify the lane twice.
Horizontal-only road changes update the snapshot even when camera Z is unchanged.

Vehicle and camera motion are observed separately at actual hook ticks. A single
causal camera forecast is shared by the current presentation frame. Recomputing
historical forecasts after new observations previously caused backward corrections
during braking. Prediction remains bounded to one observed update interval.

The native far-distance draw cutoff can alternate between a tiny sprite and no
builder output while the vehicle remains active. A bounded depth 28..34 band now
bridges that transition using the last complete forward appearance. Both the
recent packet and current RAM must confirm the verified live flag. Stale,
inactive or out-of-band actors do not qualify. Tiny actors keep native pixel
sampling to avoid alternating between two different minification patterns.

Forward and mirror projections reuse actor identities. The mirror's two small
HUD images must never become the forward actor canvas. Forward art and rear
packets are now separated. A verified live near vehicle finishes passing with
its last complete forward pose and unclipped world-ground projection; braking
cannot reverse that departure or resurrect a forward bitmap from mirror history.
Retention has a 32-frame appearance deadline and bounded exit motion. Actual
crests still occlude vehicles, and the HUD and mirrors retain native rendering.

The clipped road Y table remains appropriate for terrain and crest masks. It
cannot provide the near vehicle's ground height after clipping to HUD line 152.
The qualified world Y/height projection supplies that ground position instead.
Vehicle size remains `focal / max(depth, 2.5)`, with focal 2.5 for riders and 3
for cars. The simulation, original poses, collisions, input and audio clocks
are unchanged.

## Validation

- Nine native groups and 24 local Python tests passed. New fixtures cover
  stationary-camera countersteering, horizontal-only road changes, live far
  cutoff recovery, stale/inactive/out-of-band rejection, unclipped near ground,
  actual mirror SAT uploads, braking after a pass and legacy re-entry prevention.
- Six paired replays cover 480 consecutive fast steering/braking frames and
  48 touring samples per game: 3,168 RAM/VRAM snapshots and 1,584 HUD frames
  remain identical. Audits report zero player corruption, missing native actor
  pixels or blue-road artifacts in these runs.
- Dynamics traces contain 7,891 / 3,964 / 7,020 calibrated, lane-anchored
  observations. All stay within the player-plane size reference. The independent
  synthetic countersteer fixture verifies direction; trace equations also check
  curve-relative lane consistency.
- RR1's reproduced five-frame far-cutoff gap now contains actual vehicle pixels
  throughout. RR3's reproduced car pass exits without the former mirror-driven
  disappearance/re-entry. Coverage counts measure drawn host pixels, rather than
  assuming that a trace entry proves visibility. Real crest/player occlusion and
  native fallback can correctly have no host coverage.
- Native/enhanced video comparisons passed for RR1/2/3 wide and RR2 4:3,
  including title, pause/resume state and HUD. Original PCM is byte-identical
  with geometry off/on in all three games.

Run `tools/check_vehicle_lanes.py` against the private replay evidence.
The aggregate is `build/validation/vehicle-lanes-final/result.json`; it records
the executable hashes. The normal-speed before/after preview is
`build/validation/vehicle-lanes-final/veiculos-curvas-passagem.gif`.
These captures and game-memory snapshots are excluded from public packages.

[AGENTS.md](../AGENTS.md) contains the self-contained development handoff,
native data layouts, equations, failed approaches, build commands, evidence
workflow and reusable lessons for other Mega Drive recomp projects. Automated
coverage is limited to the documented fixtures/replays; owner playtesting and
explicit approval remain the next steps before GitHub publication.

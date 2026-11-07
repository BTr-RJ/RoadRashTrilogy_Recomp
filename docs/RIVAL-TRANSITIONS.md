# Rival steering, complete poses and render-state transitions

0.50 local follow-up, 6 November 2026. Shared engine correction for RR1, RR2 and
RR3, following the owner's RR1 gameplay recording. Earlier near-camera growth,
road geometry, scenery, proportions and forward/mirror separation remain.

The subsequent [native-anchor correction](NATIVE-VEHICLE-ANCHORS.md) supersedes
the small source-placement residual limit. Its checker/package is current;
the verification and package named below remain historical evidence.

## Recording review

The supplied recording is approximately 206.233 seconds, 3840x2160 at 30 fps.
Overview samples cover the recording; consecutive frames were reviewed around
24..27, 31..34, 72.5..75.5 and 158.5..161.5 seconds. Around 25.8 seconds the
adversary also crashes and separates from the bike: a running rider appears to
its left. A missing mounted torso in that scene alone is not proof of a bug.
The engine must distinguish riding, attack overlays and independently placed
rider/bike animation instead of applying stale riding assumptions to all of them.

The recording does not contain an input script or guest snapshots. It is a
visual reference, not an exact replay. Its frames, native game data and all
comparison captures stay out of public packages.

## Engine changes

- Lateral prediction now requires two native intervals to support the direction.
  A reversal no longer forecasts further along an unsupported velocity and then
  corrects backwards at the next observation.
- A critically damped correction follows the live lane target and its velocity.
  It is evaluated once per presentation frame and remains within eight screen
  pixels of the current measured/predicted lane. Real discontinuities reset it.
  Road curvature remains separate. Latest pose artwork is not buffered, and the
  controlled player's pose/input response remains native.
- Riding focal calibration is requalified for each forward appearance. A changed
  projection cannot silently retain an old rider/car calibration. Multiple parts
  with different scale or substantially different placement are not predicted as
  one rigid vehicle. Unqualified appearances retain their current source pose;
  the enhanced riding model resumes when qualified data returns.
- Complete source poses now respect native SAT drawing order. Descriptor order
  can be different, especially for attack overlays. The same ordering applies
  during source qualification, recovered widescreen margins and drawing.
- Short mid-distance empty-builder transitions can preserve a verified live
  appearance for at most 12 frames. Recent packet and current RAM must agree on
  active state and the render-state token. State changes, stale data and removal
  cannot authorize the hold; the current crest continues to mask it.
- Builder-local source observations take precedence over the broader per-frame
  pre-cull collection. This avoids attaching observations from another invocation
  to a new pose. Whole-pose/upload ownership and native fallback remain in place.

These changes affect presentation. They do not rewrite AI decisions, game speed,
collisions or native animation artwork. The common depth model and near-camera
growth from [the preceding correction](SPRITE-APPROACH.md) are preserved.

## Verification

- Nine native groups and 24 local Python tests pass. New pixel fixtures cover
  steering reversal, changed focal convention, separately placed rider/bike,
  native overlay priority in recovered margins, mid-distance upload gaps,
  render-state changes, crest masking and stale evidence. Existing removal,
  palette, player, hill, near-growth and mirror fixtures remain.
- The new reversal fixture also ran against the preceding packaged engine and
  failed there; it passes on the current engine.
- Thirteen paired replays cover two combat/steering windows, dynamics and touring
  in all three games, plus RR1 near approach: 9,408 guest RAM/VRAM snapshots and
  4,704 HUD frames remain identical. Audits report zero player corruption,
  missing native actor pixels or blue-road artifacts in these runs.
- Across the recorded traces, 13,693 / 3,740 / 8,435 lane samples receive a
  correction above .01 screen pixels in RR1/2/3. The eight-pixel bound holds.
  Four live mid-gap observations occur in RR1; the general hold safeguards are
  additionally tested with synthetic fixtures. These are logged observations,
  including pre-capture warmup, not a count of unique gameplay frames.
- The earlier far-cutoff pixel check and mirror re-entry check pass. Qualified
  near scale continues to agree with depth. Four native/enhanced video pairs
  pass (RR1/2/3 wide and RR2 4:3), including title and pause/resume. Original PCM
  is byte-identical with geometry off/on in all three games.

Current checker: `tools/check_rival_transitions.py`. Private aggregate and binary
hashes: `build/validation/owner-rivals-complete/result.json`. Normal-speed paired
GIFs in that folder are named `rr1-adversarios.gif`, `rr2-adversarios.gif` and
`rr3-adversarios.gif`. They show matched scripted inputs, not a reconstructed
version of the owner's recording. Full campaign/all-state qualification and
GitHub publication still require the owner's review.

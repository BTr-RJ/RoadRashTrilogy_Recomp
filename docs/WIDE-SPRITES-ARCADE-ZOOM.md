# Wide sprite recovery and continuous actor zoom

Vehicle prediction is now superseded by [current hill-aware projection and
player-relative proportions](HILL-VEHICLES.md). Source-margin recovery remains.

Version 0.50 review follow-up, 6 November 2026. The wide clipping correction was
implemented and checked first, followed by presentation smoothing. Publication
still requires owner approval.

## Widescreen correction

The original games can clip sprite data to their 320-pixel viewport before the
host receives a bitmap. Enlarging that clipped bitmap cannot restore its missing
edges. Some paths also discard an entire descriptor before the object has left
the expanded view.

`src/presentation/sprite_geometry.cpp` now keeps complete, bounded actor source
packets from projection and committed descriptors. It preserves current native
animation pixels and learns their palette, flip and priority. Full ROM art fills
missing source margins before presentation scaling. A fresh projection can
authorize an object beyond the original viewport; an explicit empty update
removes it. The pre-builder cull path is sealed at the next frame boundary.
Recovery is limited to the wide race view and excludes the player.

Nearby scenery also distinguishes the fixed HUD boundary from a real hill clip.
The HUD boundary stays at destination line 152 instead of being enlarged around
the object's anchor. Real hill occlusion remains in effect.

## Smoothing without changing game speed

The visual target is the coherent approach of Super Hang-On arcade sprites.
The primary hardware reference is MAME's [Super Hang-On machine configuration](https://github.com/mamedev/mame/blob/master/src/mame/sega/segaorun.cpp)
and [System 16B sprite renderer](https://github.com/mamedev/mame/blob/master/src/mame/sega/sega16sp.cpp).
These document separate sprite scaling and clipping. No MAME code was copied.
This project retains Road Rash artwork and native animation cadence; it does
not claim identical Super Hang-On hardware behavior.

Vehicle motion now belongs to a stable actor identity across animation-image
changes. Inverse projected scale supplies depth; position is projected from that
depth between game updates. The newest committed pose is shown immediately.
Prediction is limited to six frames and resets after discontinuities. A pending
texture upload can keep the last complete assembly while its position advances,
so the renderer never needs to combine a new bike with an old torso.

This motion path works in 4:3 and widescreen. Existing source scenery projection,
terrain and road-marking smoothing remain active. CPU timing, gameplay, input,
collision and audio clocks are unchanged. No historical framebuffer delay or
new animation poses are introduced.

Internal diagnostic switches: `RR_EDGE_RECOVERY=0` disables wide source recovery;
`RR_ARCADE_MOTION=0` holds the current actor source pose for comparison. Neither
is a launcher preference. Normal play enables both.

## Verification and continuation

- Nine native groups and 24 local Python tests passed. Synthetic organic-scene
  regressions cover left/right recovery, pre-builder culling, explicit removal,
  nearby scenery/HUD clipping and continuous depth across an animation change.
- Three 480-frame traffic replays preserve all 2,880 guest RAM/VRAM snapshots
  and 1,440 HUD images against the preceding scenery build.
- Native/enhanced 60-frame comparisons passed in all three wide games and RR2
  4:3, including title, pause, resume state and HUD checks.
- Original PCM is byte-identical with geometry disabled/enabled in all three
  games, using the same scripted input and original music.
- Extra actor scale changes during gaps in the clip-fixed stepped presentation:
  RR1 9,818; RR2 7,364; RR3 7,578 over the full replay traces. These are actor/frame
  observations, not extra simulation frames or a promise of unique animation poses.

Private results: `build/validation/arcade-final/result.json`. Reproduce using
`tools/probe_visual_regressions.py`, `tools/probe_geometry_video.py`,
`tools/probe_geometry_audio.py`, then `tools/check_arcade_zoom.py --help`.
Never publish the RAM/VRAM captures or private game inputs.

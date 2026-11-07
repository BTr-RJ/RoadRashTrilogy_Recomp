# Scenery disappearing after pause — 0.50 review correction

The native scaler can commit several complete scenery instances in one job,
reusing the same pattern tiles. A tree can also contain separate trunk/crown
images. The previous full-art replacement drew one instance but suppressed all
of that job's SAT pieces. Pausing restored the native pieces, revealing the
missing trees, poles, houses and other repeated scenery.

The renderer now recovers complete instance groups from committed SAT patterns,
sizes, positions and pixel hashes. Each group preserves its own anchor,
horizontal/vertical mirroring, palette and priority. Continuous projection and
zoom apply to every recovered instance. The existing native hill clip and HUD
boundary remain in force.

Unrecognized layouts and subpixel source images retain their current native
assembly. A source replacement must contain an actually drawable opaque sample;
a nominal rectangle alone no longer permits suppressing visible native pieces.
Replacement is atomic for a job, so a failed copy does not leave a mixture of
original and replacement instances. Retained offscreen scenery still uses the
bounded world projection when its complete layout is known.

## Verification

The new native regression fixture failed with the old renderer. It checks two
two-part scenery instances, a mirrored copy, different palettes and all four
visible parts. It also checks that subpixel or transparent source replacements
cannot erase a native assembly. All nine native test groups and 24 local Python
tests passed after correction.

Matched real-game replays passed in RR1, RR2 and RR3 at 16:9, plus RR3 at 4:3.
The traces confirm recovered repeated scenery during racing and after resuming.
Title and paused captures remain identical to the preceding corrected build;
HUD and guest RAM/VRAM also remain identical before pause and after resume.
The earlier grass, paused-road and RR1 music fixes remain covered by native tests.

Private evidence: `build/validation/scenery-confirmed/result.json`,
per-game captures, `objects.csv` and scenery audits; the 4:3 replay is under
`build/validation/scenery-confirmed-43`. The preceding build's matched captures
are `build/validation/grass-pause-final`. `tools/check_scenery.py` checks these
invariants. Optional `RR_SCENERY_AUDIT` writes per-job candidate coverage, native
piece headers, recovered copy count and replacement/fallback selection. It is
disabled during normal play.

The corrected review packages are in `dist/0.50-approval-scenery/`, still version
0.50 and still awaiting owner approval before GitHub publication. Earlier
approval packages are superseded. No ROM, music recording or generated game
code is distributed.

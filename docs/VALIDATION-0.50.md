# Version 0.50 validation record

Validated locally on Windows x64, 6 November 2026. This is a review candidate;
publication remains subject to owner approval.

Latest launcher/music follow-up: [lettering and portable remake pack](LAUNCHER-MUSIC-0.50.md).
Its current run passed 27 Python tests, nine native groups, queued UI QA,
an isolated Unicode-path RR1 installation/update and all 20 bundled music cues.
The original release/rendering evidence below remains historical to its run.

| Check | Result |
|---|---|
| Python source / installer tests | 24 passed, including synthetic static code generation locally |
| Native CTest groups | 9 passed in the clean installed tree |
| Real mapping UI paths | Save, Cancel, reserved F6, Delete, INI reload passed using an isolated QA profile |
| Queued native keyboard navigation | Same-card Right → Enter, Left and settings activation passed through the real message loop |
| Clean local installation | All three games generated and compiled from verified local ROMs and pinned SDK caches |
| Isolated installed launcher → RR1 / RR2 / RR3 | All exited successfully, 426×224 wide source, CRT active, continuous geometry counters and captures present |
| Subset installation / Unicode path | RR1 only installed and ran successfully in a path containing spaces and accented characters |
| Failed install retry | The Unicode resource-path defect was corrected; retry in the recognized installation completed successfully |
| Extracted standard ZIP without ROMs | Launcher opened successfully and displayed Not installed / Install game |
| Visual review | Seven rendered native captures reviewed; listed keyboard/context fixes resolved |
| Artwork provenance | Five launcher rasters scanned; zero missing provenance records |
| Source / installer audit | Per-file manifests and ZIP checksums; no ROM/private/generated game inputs included |
| Follow-up rendering/audio regressions | RR1 paused continuation, far grass in all three games and RR1 results cue 9 corrected; [evidence](GRASS-PAUSE-MUSIC.md) |
| Scenery after pause | Repeated/paired scenery instances and visibility fallback tested in all three games; [evidence](SCENERY-COPIES.md) |
| Wide clipping and arcade-style zoom | 2,880 guest snapshots, 1,440 HUD images, four native/enhanced video comparisons and three identical original-PCM comparisons; [evidence](WIDE-SPRITES-ARCADE-ZOOM.md) |
| High curves and near-camera follow-up | Left/right edge reproductions and RR2 car scale reset corrected; seven replays, 2,592 identical guest snapshots, four video comparisons and three PCM pairs; [evidence](CURVE-NEAR-ZOOM.md) |
| Vehicle hills and proportions | Six matched replays, 1,728 unchanged guest snapshots, calibrated size bounds and continuous depth, four video comparisons and three identical PCM pairs; [hill evidence](HILL-VEHICLES.md) |
| Fast steering/braking and vehicle lanes | Six replays, 3,168 unchanged guest snapshots and 1,584 HUD frames; live cutoff and mirror-pass pixel regressions, four video comparisons and three identical PCM pairs; [latest evidence](VEHICLE-LANES.md) |
| Near-camera approach and overtaking | Seven replays, 3,648 unchanged guest snapshots and 1,824 HUD frames; depth-consistent growth, full-image exit, four video pairs and three identical PCM pairs; [current evidence and reference study](SPRITE-APPROACH.md) |
| Rival direction/pose transitions | 13 replays, 9,408 unchanged guest snapshots and 4,704 HUD frames; native pixel fixtures, previous-engine failure reproduced, bounded lane correction, four video pairs and three identical PCM pairs; [latest evidence](RIVAL-TRANSITIONS.md) |
| Starting-grid/native pose anchors | 16 replays, 12,288 unchanged guest snapshots and 6,144 HUD frames; 336 grid observations with exact source anchors, actual rider-mask comparison, previous-engine fixture failure, four video pairs and three identical PCM pairs; [current evidence](NATIVE-VEHICLE-ANCHORS.md) |

The installer/resource fix uses an ASCII relative icon path in the build folder
and an explicit UTF-8 resource code page. This prevents resource compilation from
misreading accented source-directory names.

Automated game evidence covers initial menus and replayed racing sequences,
not complete campaigns. Original 0.4.3 road geometry algorithms are preserved.
Specific physical controller models and first-time Microsoft C++ installation on
a machine without those tools were not tested. Source CI skips legacy synthetic
codegen checks when their optional private development recompiler is absent;
the normal local-install build uses the pinned public SDK source.

Detailed private evidence: `build/validation/release050/validation-summary.json`,
test logs, installation logs and per-game geometry JSON / presentation captures.
Never publish the whole working evidence folder: it also contains private
installed copies and generated game outputs.

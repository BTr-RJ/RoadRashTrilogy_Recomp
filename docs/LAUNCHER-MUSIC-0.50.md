# Launcher lettering and portable remake music — 0.50

Owner follow-up on 6 November 2026, after a PC restart. This local candidate
supersedes the grid candidate for packaging and launcher review. Publication
still requires the owner's explicit approval.

## Changes

The header now groups ROAD RASH TRILOGY in the existing condensed bold face.
Yellow RECOMP follows its visible bounds by 14 logical units, using bundled
Barlow Condensed Bold Italic at 34px. PLAY / INSTALL GAME uses the same accent
face at 32px, centered horizontally and vertically using glyph bounds. Dark
surfaces, classic covers, keyboard routing and the utility typography remain.
The font's source/hash and existing OFL license travel with the package.

Both Windows installers include 18 full-length Dean Harris recordings mapped
to 20 cues. Only referenced audio, three TSV maps and sanitized artist URLs,
durations and hashes enter `music/dean-harris/`. Raw downloader metadata and
unmapped files stay private. Source ZIPs exclude the recordings. This follows
the owner's bundling instruction and does not establish a new music license.

Runtime resolves the pack from the installed root. An existing private map
overrides its game; missing or undecodable files preserve original-audio
fallback. New installations select remakes when the pack is available, while
saved soundtrack preferences continue to take precedence. Original audio is
still selectable. No game simulation or geometry algorithm changed.

## Validation

- 27 Python tests passed, including synthetic mapped-pack selection, path
  escape rejection, checksum rejection, raw-metadata exclusion and source-ZIP
  exclusion.
- Nine native CTest groups passed. Music fixtures exercise portable lookup,
  private-map precedence, actual Media Foundation decoding, pause/resume and
  original fallback. The controls fixture now sets the application's MSVC CRT
  environment directly; SDL2's separate DLL environment was insufficient for
  its quickload-suppression assertion. Production controls behavior is unchanged.
- Real queued launcher keyboard/mapping QA passed. Main desktop, minimum-size
  and audio dialog captures were inspected for alignment and clipping.
- A fresh RR1-only installation built successfully using the packaged offline
  SDK caches with `-NoToolInstall`, in a path containing spaces and accents.
  Updating the recognized installation also succeeded.
- All 20 actual bundled music cues decoded and mixed from that installation;
  no `.local/music` directory exists there. This verifies portable pack lookup,
  rather than reuse of the developer's music folder.
- The installed production launcher started RR1 with widescreen, CRT and
  continuous geometry. Launcher QA preserves normal preferences.

Private logs, captures and the aggregate report are under
`build/validation/launcher-audio050/`. Packages are prepared in
`dist/0.50-approval-launcher-music/`; run `tools/audit_release.py` against that
directory for ZIP hashes, full manifest membership, ROM exclusion and each
music cue's actual bytes and provenance.

This follow-up does not rerun or relabel historical grid/rival rendering
captures. Existing limits on full campaigns, other ROM revisions, physical
controllers and first-time global tool installation still apply. A second
physical PC was not tested in this workspace.

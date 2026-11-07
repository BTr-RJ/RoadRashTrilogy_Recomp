# Selectable soundtrack

The Windows 0.50 installers include 18 full-length Dean Harris Composer remake
tracks mapped to 20 music cues across the trilogy. Source packages exclude the
recordings. Credits, official source URLs, durations and SHA-256 hashes are
included in `music/dean-harris/sources.json` inside the installers.

Choose Original or Remake in the launcher audio settings. Original Mega Drive
music remains available; missing or undecodable remake tracks fall back to the
original audio. Pause preserves the current playback position. Music and sound
effects have separate volume controls.

The installed player reads `music/dean-harris/` relative to the application root.
Each per-game TSV map contains a cue number, filename under `source/`, and title,
separated by tabs. Private maps under `.local/music/dean-harris/` take precedence.
Media Foundation decodes the recordings using the codecs available in Windows.
Tracks loop from beginning to end; custom loop points and fades are not supported.

For release preparation, pass `--music-pack .local/music/dean-harris --offline`
to `tools/package_release.py`. Only mapped tracks, cue maps and sanitized source
credits enter installer packages. The packager verifies track hashes.

Credits: Dean Harris Composer. Official sources:
[artist Patreon](https://www.patreon.com/DeanHarrisComposer) and
[Road Rash 3 album](https://www.patreon.com/DeanHarrisComposer/posts/road-rash-3-full-53364429).
Recordings remain copyrighted by their respective owners; inclusion in an
installer does not grant a new license to the music.

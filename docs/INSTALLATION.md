# Installing version 0.60

Use Windows 10/11 x64 and a writable folder. Extract the complete Windows-x64
release ZIP before running `RoadRashTrilogyRecomp.exe`. Keep the executable,
SDL2.dll, assets, tools, scripts and source folders together.

1. Open the launcher and choose **Install games**.
2. Choose one or more folders containing your own ROM dumps. One folder may
   contain all three games; the other fields can remain empty.
3. Choose an empty installation folder, or a recognized previous installation.
   The default is your local Programs folder. Avoid Program Files and drive roots.
4. Choose **Install games**. The installer verifies the ROM revisions, prepares
   pinned SDK dependencies, generates game code locally and compiles the games.
5. Choose **Open launcher**, select a ready game and press **Play**.

BIN, MD, GEN and ZIP are supported, including ZIPs containing more than one
candidate. Names are not used to authenticate a game; SHA-256 is checked. Only
provided supported games are installed. No game files are downloaded. Your
original ROM folders are read only; private copies remain in the installation.

## Supported revisions

| Game | Cartridge revision | Bytes | CRC32 |
|---|---|---:|---|
| Road Rash | USA, Europe | 786432 | DEA53D19 |
| Road Rash II | USA, Europe, RR206 | 1048576 | 0876E992 |
| Road Rash 3 | USA, Europe | 2097152 | 15785956 |

Authoritative SHA-256 values are in [supported-roms.json](../games/supported-roms.json).
Japanese releases and all other unlisted regions/revisions, modified images and
SMD interleaved dumps are not accepted. This installer does not support every
Road Rash ROM. Renaming a file cannot change its compatibility.

## Tools and downloads

The standard package downloads exact dependencies listed in
`tools/public-tools.lock.json`; each archive is verified before extraction.
The offline package includes those verified archives. Microsoft C++ Build Tools
and a Windows SDK must already be installed for a fully offline installation.
If absent, the installer offers the official signed Microsoft installer through
Windows administrator consent. This download needs Internet access and can take
several minutes. A cancelled or failed tool installation can be retried.

No administrator permission is needed for the game folder itself. The project
uses a private Python, CMake and Ninja instead of changing your PATH.

## Settings, saves and removal

Both Windows release installers include 18 Dean Harris remake tracks under
`music/dean-harris/`. They are copied with the installation and play locally;
there is no separate music download or dependency on the developer's folders.
Choose **Audio & music** to switch between remakes and the original Mega Drive
soundtrack. Track credits and checksums are in the pack's `sources.json`.
An existing `.local/music/dean-harris/rrN.tsv` overrides the bundled map for
that game. Missing tracks fall back to original audio.

Launcher settings: `.local/launcher.ini`. Controls: `.local/controls.ini`.
Each locally built game keeps its own settings and saves beside its executable
under `build/native/bin/rr1`, `rr2` or `rr3`. Keep these when moving installations.
Private ROMs and generated inputs remain under `.local/`. Logs are under
`.local/logs/`; the installer log is `.local/install-0.60.log` in the package
you launched. **Open logs** opens that folder.

To add another game, reopen Install games and select the existing recognized
installation as destination. To remove the application, close its windows and
remove its dedicated installation folder. Back up saves first. There is no
system service, registry installation or global PATH change.

## Troubleshooting

- **No supported ROM revisions:** check the table and SHA-256 list. RR2 must be
  RR206. Pick a smaller folder containing cartridge dumps rather than an entire
  backup drive.
- **Checksum mismatch:** the downloaded/cached dependency failed verification.
  Remove only the named archive from `.tools/downloads` and retry online.
- **Destination not empty:** choose a fresh folder. The installer only updates
  folders marked as this product's installation.
- **Compilation failed:** retain the installation log and check free space and
  C++ tools. Stopping installation never changes your original ROM folder.
- **MSVC 2019 C2059 in `rr1_layout.c` (line 13):** the original v0.50 SDK
  emits an empty `level_modes` initializer that this compiler rejects. The
  corrected build pipeline emits `{ 0 }` while preserving the zero mode count.
  Apply the corrected source tools to the existing installation and rerun
  `scripts/build.ps1 -BuildFolder build/native`; the generator rebuilds and
  refreshes the game sources automatically. Editing generated C is temporary
  and will be undone by regeneration. Full VS2019 compatibility is not yet
  qualified; Visual Studio 2022 remains the supported build environment.
- **Missing files:** extract the whole ZIP again; do not run the EXE inside ZIP.

For bug reports, provide the version, Windows version, GPU, ROM SHA-256 and the
relevant log excerpt. Do not upload ROMs, generated game code, saves or account
tokens. Review logs for private paths before sharing.

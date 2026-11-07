# Third-party notices

Each component keeps its own terms. A public source repository does not change
the ownership or licensing of the original games, covers or music. No commercial
game image or translated game code is distributed in these packages.

| Component | Terms and retained notice |
|---|---|
| GenesisRecomp runtime and modifications | PolyForm Noncommercial 1.0.0; [license](docs/licenses/GenesisRecomp-PolyForm-Noncommercial.md) |
| GenesisRecomp recompiler | [Retained recompiler license](docs/licenses/GenesisRecomp-recompiler.txt) |
| Engine dependencies | [Upstream inventory](docs/licenses/GenesisRecomp-third-party.md); license texts also remain in downloaded SDK sources |
| SDL2 | zlib; [notice](docs/licenses/SDL2.txt) |
| rcheevos | MIT; [notice](docs/licenses/rcheevos.txt) |
| ymfm | BSD 3-Clause; [notice](docs/licenses/ymfm.txt) |
| clowncommon | ISC; [notice](docs/licenses/clowncommon.txt) |
| librashader | MPL 2.0; [license](docs/licenses/librashader.md), [versioned source](https://github.com/SnowflakePowered/librashader/tree/librashader-v0.12.0) |
| koko-aio | Its upstream source and per-file license notices are retained by the pinned SDK download; [source](https://github.com/kokoko3k/koko-aio-slang/tree/64f3d8338ccaf3bc4024ad01f0e6da4a10da418b) |
| Barlow / Barlow Condensed | SIL OFL 1.1; `assets/launcher/BARLOW-LICENSE.txt` and `FONT-LICENSE.txt` |
| D3DX support files | Microsoft SDK terms; [notice](docs/licenses/Microsoft-D3DX.txt) |

The standard installer obtains build tools from official distributions.
The offline package contains those same unchanged verified SDK archives and
their included notices. Python, CMake, Ninja and SDK dependency notices remain
inside their distributions. Microsoft C++ Build Tools are obtained from Microsoft
when needed, not bundled. Exact URLs and hashes are in
`tools/public-tools.lock.json`.

Project-specific source files without an explicit license do not grant additional
rights by being publicly visible. Existing upstream terms continue to apply to
derived patches and runtime components. No commercial-use permission is implied.

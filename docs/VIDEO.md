# Video and presentation

Open **Video & CRT** in the launcher to choose the output resolution, display
filter, fullscreen mode, experimental 16:9 expansion and **Continuous zoom and
smooth movement**. Save applies these preferences to subsequent launches.

Pixel and linear filtering preserve the artwork's proportions. CRT choices
use librashader / koko-aio; widescreen uses the complete expanded canvas.
Original graphics are low-resolution artwork, so increasing output resolution
does not introduce new source detail.

Version 0.60 preserves the shader authors' artistic settings,
including curvature, bezel, background and dynamic zoom. Expanded canvases use
the same display-aspect integration as Super Monaco Duo Recomp: ASPECT_X follows
the canvas proportions and ASPECT_Y is 1. Native 4:3 keeps the previous preset
values. The engine adds no color correction before an external shader.

## Continuous zoom and motion

Version 0.60 smooths scenery, vehicle movement, approach scaling,
terrain and road markings in recognized single-player races and the original
RR2/RR3 local split-screen modes. Each split view owns independent camera and
road state. Current-source fractional sprite coverage reduces edge crawling
and thin-feature popping without changing the shader's input dimensions. Presentation
targets the NTSC display cadence of approximately 59.94 Hz while preserving
the original simulation speed, physics, input and audio timing.

Animation retains the game's authored poses and native update cadence. A
smooth display does not mean every object has 60 unique animation poses per
second. Unknown or incomplete objects keep their original rendering.
Menus and paused motion retain native behavior; expanded paused views preserve
the finite road continuation. RR1 retains its original alternating-player mode.
Network multiplayer is not included.

Turn continuous zoom off for original rendering comparisons. Widescreen is a
separate experimental setting; original 4:3 remains available. Side expansion
and complete campaign coverage are not a guarantee for every scene.

The [README comparisons](../README.md#original-mega-drive-vs-continuous-zoom-060)
show synchronized original/enhanced captures at normal game speed.

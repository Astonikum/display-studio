# Mad Studio

Mad Studio is a native display UI designer and simulator. It uses the same
MadDisplay components, sizing rules and renderer as the embedded library.
Write, compile and upload device sketches in your preferred external tools.

The workspace keeps pages and a permanent component palette on the left,
the simulated display in the center, and component properties on the right.
Its neutral gray toolbar uses bundled Phosphor Icons; hover for each action.

The Controls palette contains `Label`, `Button`, `Switch`, `Checkbox`,
`RadioButton`. The Graphics palette contains `Canvas`, `Image`, `Rectangle`,
`Ellipse` and `Line`, all backed by MadDisplay components. Image imports PNG,
JPG/JPEG and self-contained SVG through the folder icon (or a path + Enter).
Its embedded original and filter settings travel inside `.mad`; no source-file
link is required after import. Contain/Cover/Stretch resize to 1–512 pixels per
side with smooth alpha-aware sampling. Brightness, contrast, gamma, saturation,
threshold, inversion, alpha cutoff and matte are adjustable. None/Bayer/Floyd–Steinberg
dithering and a strength slider adapt the baked result to the selected display:
mono bits for mono/zoned panels, the configured palette, or RGB565. Changing
display colors reprocesses from the original. Source/Result thumbnails and the
actual display preview update live; idle frames reuse cached pixels and textures.
Reset restores filters. The brush icon detaches the source for manual pixel
editing; undo restores the source and adjustments. Existing bitmap images remain
editable through Pixel editor. Canvas
provides a list of local drawing commands (pixels, lines, rectangles, ellipses),
clipped to its bounds. Shapes have intrinsic dimensions, color and fill settings.
Old Matrix nodes migrate to Image while preserving their pixel values.
The Layouts palette
contains vertical and horizontal layouts, both scrollable variants,
Box (`Container`), `Panel`, `GridLayout` and `FreeLayout`. Grid fills left to right,
then starts a new row. Columns, Row gap and Column gap are configurable;
Scroll overflow enables vertical scrolling with a visual scrollbar. Click to add controls; controls
also support drag and drop into the hierarchy.

Select a layout in the hierarchy, including Root, then use **Properties → Layout → Type**
to switch between VerticalLayout, HorizontalLayout, GridLayout, FreeLayout,
Container and Panel. Children, IDs and root sizing are retained. Switching to
FreeLayout captures the children's current arranged positions and sizes; its
coordinates can be edited under Advanced. Undo/Redo also applies to layout changes.

The toolbar exposes all MadDisplay input actions: Left, Up, Down, Right,
Previous, Next, Use and Back. Clicking a control always selects and activates
it. Arrow keys and Enter control focus; Tab / Shift+Tab map to Next / Previous
and Backspace maps to Back. Up/Down preserve the horizontal focus anchor across
uneven rows; Left/Right stay in their row. Next/Previous follow the component tree
in forward/reverse order, skip hidden/disabled controls and stop at the ends.
Default text sizing excludes the final character spacing and label line spacing,
so control padding stays symmetric. A button's Scale changes text, border, padding,
corner cuts and focus inset together. Padding uses base units before scaling.
Layout gaps remain explicit (1 pixel by default).

Each layer has an Animation section backed by the library's `Motion`:
Rise, Slide and Reveal, with duration, delay and all five library easing
curves. Switches expose their toggle duration. The Simulation menu controls
page transition duration/easing, frame rate, safe area, grid and zoom.
The pause icon (or Space) stops the entire simulation clock and control input.
Click any page, including the current page, to restart its entrance animations.
A page click while paused queues its entrance until the simulation resumes.

A single `.mad` design (version 6) contains the display profile, pages, controls,
layouts, motion and simulation settings. The display selector has 48 profiles: 36 requested physical panel variants,
explicit SPI alternatives and retained legacy/custom profiles. Names use spaces.
See [DISPLAYS.md](DISPLAYS.md) for simulation features and physical driver mappings,
and [BOARDS.md](BOARDS.md) for the 33 board specifications.
ILI9341 profiles render RGB565; hardware support is the separate MadILI9341 package.
The display and hardware dropdowns provide case-insensitive name search with
scrollable results. Opening either dropdown clears and focuses its search field.
All runtime components and animations come directly from MadDisplay.

`Ctrl+N` creates a design, `Ctrl+S` saves it, and `Ctrl+Z` / `Ctrl+Y` undo and
redo changes (Cmd also works on macOS). The toolbar path identifies the whole
design; there is no separate project or UI asset.

Session files are `.mad_studio.ini`, `.mad_display_studio.session` and
`.mad_display_studio.autosave.mad`. Old sessions and `.madui` files remain
readable. Legacy `.mad` references resolve their UI path relative to the file.
Before replacing a reference-only project with a complete design, Studio saves
its original bytes to `<path>.legacy.bak`, preserving former device settings.
A different existing backup is never overwritten. Files save through a temporary
file before replacing the original. Former generated sketches are unused.

## Build and test

The project fetches Dear ImGui at configure time and uses installed SDL2 and
OpenGL. No graph editor or embedded toolchain is needed.

From the workspace root:

```sh
cmake -S display-studio -B /tmp/mad-studio-build
cmake --build /tmp/mad-studio-build -j
ctest --test-dir /tmp/mad-studio-build --output-on-failure
/tmp/mad-studio-build/MadStudio
```

Tests cover design serialization, legacy migration and backups, core
layout/focus/animation, the showcase at three resolutions, SSD1306 byte
conversion using a driver stub, and Studio runtime parity, replay/pause,
switches, page history, scrolling, all palette types and animation persistence.

## Animated OLED showcase

`/tmp/mad-studio-build/MadDisplaySimulator` opens a standalone 128×64 showcase
using the actual MadDisplay renderer. Space / Right changes scenes, Enter
toggles the control, P pauses and Esc exits. Waves, orbital motion and spectrum
scenes also cycle automatically. `--smoke` runs 120 frames across the scenes
and exits. Physical display performance still needs validation on the board.

Phosphor Icons: https://github.com/phosphor-icons/web (MIT, bundled license in `assets/phosphor/LICENSE`).

## Liquid focus and camera

Timestamped previews use the library's elastic focus body and camera following
on both axes. Nested vertical and horizontal scroll layouts ease to the chosen
control. The camera-transformed bounds are also used for selection and clicking
controls. Click activation moves focus to that control before handling Use.

Simulation > Focus and Scroll set the library durations in milliseconds;
zero disables the associated animation. Values are persisted as `focusMs`
and `scrollMs`, with defaults of 144 and 280 ms. Arrow navigation works outside
text input. Fixed artwork pixels bypass the liquid inversion, so checkbox and
radio outlines preserve their shape; their focus regions match their intrinsic
9×9 inner artwork even when the surrounding component is expanded.

The navigation test covers all four camera directions, negative content
coordinates, interrupted motion, paused timestamps, nested scroll interpolation,
timer wraparound, hidden controls, transformed rasterization and persistence.

Control-focus tests compare timed rendering pixel-for-pixel against native checkbox,
radio and switch rendering in both states and at intrinsic/expanded sizes.

Focus uses a convex capsule without a pinched middle. Its default duration is
shared with MadDisplay (`kDefaultFocusDurationMs`, 144 ms). Switch track and
thumb outlines remain white; inversion affects their interiors only.

Scrollable content is independent of focus. Hover the display and use the mouse
wheel/trackpad; Shift+wheel scrolls horizontally. The deepest matching scroll
layout handles each axis, then bubbles to its parent at the limit. Arrow keys
also scroll when no further focus target exists, including pages with no controls.
Focus changes reveal the requested control once; subsequent frames preserve
manual scrolling. Page camera bounds include visible graphics and text.

Overflowing vertical/horizontal layouts draw a visual scrollbar on their right/
bottom edge. A 3-pixel lane keeps content clear; the proportional thumb follows
the animated offset. Scrollbars use fixed pixels, are not focusable or draggable,
and disappear when the content fits. The same behavior runs in MadDisplay.

Arrow navigation chooses the nearest row/column before comparing alignment, so
large controls cannot be skipped in favor of smaller controls farther away. Row
membership uses allocated bounds; centered checkbox/radio/switch artwork only
affects the alignment anchor. Next/Previous retain sequential tree order.

## Color displays and rendering

Source colors are RGB24, written as `#RRGGBB`; legacy `on`/`off` remain valid.
Simulation offers Monochrome, Palette (2–4 colors), Fixed color zones and RGB.
Palette entries, emission color and arbitrary emission rectangles are editable
and saved in the design. The bicolor SSD1306 preset starts with yellow top rows
and a blue bottom region; it is a configurable example, not a universal panel
assumption. Fixed physical emitters and independently selectable palette colors
have different models.

All component styles, pages, shape/Canvas commands and Images preserve color.
Images support transparent, normal and fixed pixels. Legacy Matrix/Image artwork
remains readable. Full-color image editing caches its content fingerprint between
edits instead of rehashing all pixels every UI frame.

The preview stores target-quantized RGB565 pixels and draws a single nearest-
filtered texture. Unchanged images skip texture uploads; buffers are resized only
when dimensions change. The standalone OLED simulator also uses a texture instead
of issuing a draw command for every pixel. The editor chrome stays neutral gray.

Version 4 persists color mode, palette, emission regions and colored images.
Versions 1–3 remain readable, including reference-only legacy projects. Hardware
SPI/I²C speed and physical display appearance require validation on real panels.

Native UI smoke checks can run in a disposable working directory with a saved
autosave design: `/tmp/mad-studio-build/MadStudio --smoke` renders 120 frames,
checks OpenGL errors and exits. The caller's working directory owns that test
session. See `../display-lib/PERFORMANCE.md` for the reproducible fill benchmark
and exact controller-emulator payload counts.

Code that edits model pixel vectors directly must call `invalidateImageHash()`
after changes; the editor does this for painting, conversion and resizing.

### Image import limits

Imports are desktop preparation: MadDisplay on the board draws baked pixels with
its existing ImageComponent; it does not run a JPEG/SVG decoder or the import
filters. Original files, decoded desktop caches and editor thumbnails are excluded
from modeled device RAM. Final pixel/coverage arrays remain included.

PNG/JPEG files are limited to 8 MiB, 16 megapixels and 8192 pixels per side. SVG
is rasterized to a 1024-pixel longest side, then sampled for the target. NanoSVG
supports paths, basic shapes, transforms, fills, strokes and gradients; text,
external/embedded bitmap images, use/symbol references and SVG filter effects
are rejected rather than silently dropped. Convert such artwork to paths or PNG.
JPEG pixels are imported in stored orientation (EXIF orientation is not applied).

Vendored decoder revisions and licenses live in `third_party/`; board headers and
adapters do not depend on these desktop codecs.

### Composable controls

Button, Checkbox, RadioButton and Switch accept child components. A new button
starts with a real `Label` named Content in the tree; edit text/font/color on that
Label. Delete it and add an Image to make an image button. Multiple direct children
form a horizontal content row; add an explicit Horizontal/Vertical/Grid layout to
choose a different arrangement. Controls accept drag/drop and Add child just like
other containers. The button's own Text/alignment editor has been removed.

Checkbox/Radio/Switch retain their indicator and show content beside it.
The whole control, including its content, is one focus/action target. Clicking a
child selects that child for editing and activates the owning control. Children
are not separate Tab/Next stops. Composite Scale multiplies child artwork, layout
gaps, padding and borders; the child's own Scale is independent and multiplies it.
Child colors belong to the child; focus inversion preserves fixed image pixels.

Version 1–4 designs migrate button text to Label without changing the original
button IDs, scale or padding. Version 5 stores content as child nodes. The same
components and local-to-display transforms run in MadDisplay and Studio.

## Unicode text

Labels support UTF-8, native font fallback and complex-script shaping, with
portable prepared glyph layouts shared by Studio and firmware. See
[UNICODE.md](UNICODE.md) for editing, font coverage and C++ asset export.

## Standalone repository

Clone https://github.com/Astonikum/display-studio and build from that checkout:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
./build/MadStudio
```

Install CMake, pkg-config and SDL2 first (`brew install cmake pkg-config sdl2`
on macOS). Tagged MadDisplay and adapter-support sources are fetched automatically
when sibling workspace checkouts are absent. Dear ImGui is fetched at configure time;
UI fonts, their licenses and icons are bundled. Hardware driver libraries are only
needed by the separate PlatformIO adapter compile check.

`pio run -d tests/adapter_compile` installs tagged adapters from GitHub and compiles
all catalog driver types for ESP32 without touching GPIO. See [DISPLAYS.md](DISPLAYS.md)
for exact revisions and verification limits. This does not constitute a hardware test.

# Unicode text

MadDisplay accepts UTF-8 Unicode scalar values through U+10FFFF. Invalid UTF-8
is replaced with U+FFFD by the portable decoder. Studio rejects malformed JSON
strings and correctly decodes UTF-16 surrogate pairs in JSON escapes.

## Editing in Studio

A Label has three Typeface modes:

- **Auto (pixel)** keeps the compact 5×7 font for Latin, Russian, Ukrainian and
  Belarusian text, and prepares broader Unicode with the bundled Mad Pixel font.
- **Unicode (pixel)** prepares all text, including ASCII, in the same pixel style.
- **Pixel** explicitly selects the small built-in font; unsupported text produces
  a warning and visible replacement boxes.

`System`, an empty family, and `Mad Pixel` select the pixel default. Latin and
Cyrillic use matching uppercase and lowercase 5×7 artwork. Other scripts use
Mad Pixel / GNU Unifont's 16-pixel grid, with the
same compact Latin and Cyrillic artwork in mixed strings. Use Scale to enlarge
whole pixels. Text retains its original case. Pixel glyphs, including
emoji, are monochrome and have no antialiasing.

Enter `Native System` or an installed font family to use native typography and
the font-size setting (6–96 pixels). macOS Core Text performs font fallback,
contextual shaping, ligatures, combining marks and bidirectional layout.
Native color emoji retain RGB565 colors; ordinary glyphs use the Label color.
Display color conversion remains in MadDisplay. See
[the bundled font notes](assets/pixel-font/README.md) for sources and licenses.

The TextT icon beside the text field opens a native editor for IME input, emoji
and complex scripts. The quick inline editor supports Cyrillic but does not shape
complex scripts itself. The simulated display uses the prepared layout.

No font covers every Unicode character. Missing glyphs and unavailable requested
fonts produce diagnostics. Install a suitable font when necessary. The preparation
backend currently uses macOS Core Text; other platforms can render saved layouts
but cannot prepare new ones without a native shaping backend.

## Saved designs and firmware

Design format 6 stores UTF-8 source, font settings, positioned glyphs and a
subset of their bitmaps. Repeated glyphs share bitmap storage. Saved layouts
remain usable without their original fonts. Editing text or font settings
invalidates the cached layout; older designs remain readable.

Studio and firmware both draw these layouts through `mad::Label` and
`mad::drawTextLayout`. The board needs neither Core Text nor a full font file.
Export prepared labels from a saved design:

```sh
/path/to/MadStudio --export-text design.mad labels.hpp
```

```cpp
#include "labels.hpp"

mad::Label title("");
// Keep the generated layout alive for the lifetime of the label.
// The suffix comes from the Label's node ID in the saved design.
title.setTextLayout(mad_text_assets::text_node_4);
```

The generated header puts bitmap bytes in AVR PROGMEM and uses far Flash reads
on AVR devices that support them. Metadata still consumes RAM. Export rejects
missing glyphs rather than silently producing incomplete firmware assets.
`setText()` and `setFont()` clear a prepared layout. For dynamic complex text on
the board, select another prepared layout or supply a target-side shaping backend;
a small AVR does not acquire an arbitrary runtime Unicode shaper through export.

Custom `BitmapFont` tables also use Unicode codepoints. Tables can be marked
sorted for binary lookup. A column contains `ceil(height/8)` bytes, least
significant bit first, allowing glyphs taller than eight pixels.

Preparation limits per Label: 64 KiB UTF-8 source, 512 lines, 4096 unique glyphs,
8192 placements and 4 MiB of bitmap/color data. Layout dimensions are limited to
32767 pixels. These are desktop safety limits, not a promise that a board has
that much memory; exported assets must fit the actual target.

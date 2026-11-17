# Mad Pixel

MadPixel.otf is derived from GNU Unifont 17.0.03. Its Latin, Russian,
Ukrainian and Belarusian letters and supported punctuation use the exact
MadDisplay 5x7 artwork and six-pixel advance. Other characters retain Unifont's
original pixel outlines. `unifont_upper-17.0.03.otf` is the unmodified
supplementary-plane companion. Fonts register for the Studio process only.

Source: https://unifoundry.com/pub/unifont/unifont-17.0.03/font-builds/

Copyright © 1998–2025 Roman Czyborra, Paul Hardy, Qianqian Fang, Andrew Miller,
Johnnie Weaver, David Corbett, Ælla Chiana Moskopp, Rebecca Bettencourt,
Ho-Seok Ee, et al. Mad Pixel modifications © 2026 MadDisplay contributors.
Both fonts are distributed under the SIL Open Font License 1.1 in OFL-1.1.txt;
the original copyright and license metadata are retained inside the fonts.

To regenerate MadPixel.otf, install fontTools in an authoring environment and
run `python build_font.py /path/to/unifont-17.0.03.otf`. The generator reads
the actual C++ firmware glyphs; building Studio needs neither Python nor
fontTools. Keep the generated font in sync when editing the built-in artwork.

The font grid is 16 pixels for broader Unicode coverage. Compact Latin and
Cyrillic-only strings use the built-in 5x7 renderer directly. Mixed strings
with wider scripts retain the compact letter artwork inside the larger line.
Both uppercase and lowercase follow the source text. Native shaping and
fallback remain available for contextual scripts and unsupported characters;
pixel mode disables antialiasing and color glyphs. No font covers every scalar.

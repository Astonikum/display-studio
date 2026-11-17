"""Build Mad Pixel from Unifont 17.0.03 using fontTools (authoring only).

Usage: python build_font.py /path/to/unifont-17.0.03.otf
The checked-in OTF is sufficient to build and run Studio.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

from fontTools.ttLib import TTFont
from fontTools.pens.t2CharStringPen import T2CharStringPen
from fontTools.unicodedata import script

here = Path(__file__).resolve().parent
root = here.parents[2]
font = TTFont(sys.argv[1])
cmap = font.getBestCmap()
top = font['CFF '].cff.topDictIndex[0]
unit = font['head'].unitsPerEm // 16

# Read the actual firmware artwork rather than maintaining another glyph table.
with tempfile.TemporaryDirectory() as temp:
    source = Path(temp) / 'glyphs.cpp'
    binary = Path(temp) / 'glyphs'
    source.write_text('''#include <MadDisplay/core.hpp>
#include <cstdio>
int main() {
  for (unsigned cp = 32; cp < 0x500; ++cp) {
    if (const auto* g = mad::findDefaultGlyph(cp))
      std::printf("%u %u %u %u %u %u\\n", cp,g[0],g[1],g[2],g[3],g[4]);
  }
}
''')
    subprocess.run(['c++', '-std=c++14', '-I'+str(root/'MadDisplay/include'),
                    str(source), '-o', str(binary)], check=True)
    glyphs = subprocess.check_output([str(binary)], text=True)

for line in glyphs.splitlines():
    cp, *columns = map(int, line.split())
    if cp not in cmap:
        continue
    name = cmap[cp]
    pen = T2CharStringPen(6*unit, None)
    for x, column in enumerate(columns):
        for y in range(7):
            if column & (1 << y):
                left, bottom = x*unit, (6-y)*unit
                pen.moveTo((left, bottom))
                pen.lineTo((left+unit, bottom))
                pen.lineTo((left+unit, bottom+unit))
                pen.lineTo((left, bottom+unit))
                pen.closePath()
    top.CharStrings[name] = pen.getCharString(private=top.CharStrings[name].private, globalSubrs=font['CFF '].cff.GlobalSubrs)
    font['hmtx'][name] = (6*unit, 0)

for record in font['name'].names:
    if record.nameID in (1, 3, 4, 6, 16):
        value = 'MadPixel' if record.nameID == 6 else 'Mad Pixel'
        record.string = value.encode(record.getEncoding())
top.FamilyName = 'Mad Pixel'
top.FullName = 'Mad Pixel'
font['CFF '].cff.fontNames = ['MadPixel']
# Unifont has no contextual GSUB features. Let Core Text shape these scripts
# with native fallback fonts, then rasterize them without antialiasing.
contextual = {'Arab','Syrc','Thaa','Nkoo','Deva','Beng','Guru','Gujr','Orya',
              'Taml','Telu','Knda','Mlym','Sinh','Thai','Laoo','Tibt','Mymr',
              'Khmr','Mong'}
for table in font['cmap'].tables:
    if table.isUnicode():
        for cp in list(table.cmap):
            if script(chr(cp)) in contextual:
                del table.cmap[cp]
font.save(here/'MadPixel.otf')

# -*- coding: utf-8 -*-
"""
Extract weather icons from Font Awesome 6 Solid,
remap to Unicode Private Use Area (E000-E00D).
"""
from fontTools.ttLib import TTFont

# FA6 Solid codepoint -> PUA mapping
GLYPH_MAP = {
    0xF185: 0xE000,  # sun         -> sunny
    0xF6C4: 0xE001,  # cloud-sun   -> partly cloudy
    0xF0C2: 0xE002,  # cloud       -> cloudy / overcast
    0xF75F: 0xE003,  # smog        -> fog
    0xF75E: 0xE004,  # smog(2)     -> haze
    0xF043: 0xE005,  # droplet     -> light rain / drizzle / shower
    0xF73D: 0xE006,  # cloud-rain  -> moderate rain
    0xF740: 0xE007,  # cloud-showers-heavy -> heavy rain
    0xF2DC: 0xE008,  # snowflake   -> snow
    0xF76C: 0xE009,  # cloud-bolt  -> thunderstorm
    0xF0E7: 0xE00A,  # bolt        -> hail
    0xF72E: 0xE00B,  # wind        -> wind (reserve)
    0xF186: 0xE00D,  # moon        -> clear night
}

def main():
    import os
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)
    
    print("Loading fa-solid-900.ttf ...")
    font = TTFont("fa-solid-900.ttf")
    
    cmap = font.getBestCmap()
    
    fa_to_glyph = {}
    for cp, glyph_name in cmap.items():
        if cp in GLYPH_MAP:
            fa_to_glyph[cp] = glyph_name
            print("  Found: U+%04X (%s) -> U+%04X" % (cp, glyph_name, GLYPH_MAP[cp]))
    
    for fa_cp in GLYPH_MAP:
        if fa_cp not in fa_to_glyph:
            print("  WARNING: U+%04X not found in font!" % fa_cp)
    
    # Remap cmap tables: only keep our mapped glyphs, with new PUA codes
    for table in font["cmap"].tables:
        new_subtable = {}
        for cp, glyph_name in table.cmap.items():
            if cp in GLYPH_MAP:
                new_subtable[GLYPH_MAP[cp]] = glyph_name
        table.cmap = new_subtable
    
    output_file = "weather_icons.ttf"
    print("\nSaving subset font to %s ..." % output_file)
    font.save(output_file)
    font.close()
    print("Done!")

if __name__ == "__main__":
    main()

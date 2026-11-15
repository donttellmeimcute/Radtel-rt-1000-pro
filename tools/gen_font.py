#!/usr/bin/env python3
"""Genera fuentes bitmap para el firmware a partir de DejaVu Sans Mono
(licencia Bitstream Vera / DejaVu, permisiva). No usa datos del firmware original.

Formato: cada glifo es W columnas x H filas, fila por fila, MSB primero,
ceil(W/8) bytes por fila. Rango ASCII 0x20-0x7E.
"""
import sys
from PIL import Image, ImageDraw, ImageFont

TTF = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
TTF_B = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"


def render(name, ttf, w, h, size, chars, yoff=0):
    font = ImageFont.truetype(ttf, size)
    bpr = (w + 7) // 8
    out = [f"/* Generado por tools/gen_font.py desde {ttf.split('/')[-1]} - no editar */",
           f"const unsigned char {name}_data[{len(chars)}][{bpr * h}] = {{"]
    asc, desc = font.getmetrics()
    for ch in chars:
        img = Image.new("1", (w, h), 0)
        d = ImageDraw.Draw(img)
        bw = d.textlength(ch, font=font)
        d.text(((w - bw) / 2, (h - (asc + desc)) / 2 + yoff), ch, font=font, fill=1)
        rows = []
        for y in range(h):
            for bx in range(bpr):
                v = 0
                for bit in range(8):
                    x = bx * 8 + bit
                    if x < w and img.getpixel((x, y)):
                        v |= 0x80 >> bit
                rows.append(v)
        out.append("  {" + ",".join(f"0x{v:02x}" for v in rows) + f"}}, /* '{ch if ch not in chr(92)+chr(39) else ' '}' */")
    out.append("};")
    return "\n".join(out) + "\n"


def main(dst):
    ascii_chars = "".join(chr(c) for c in range(0x20, 0x7F))
    src = ['#include "font.h"']
    src.append(render("font8x16", TTF, 8, 16, 14, ascii_chars))
    src.append(render("font12x24", TTF_B, 12, 24, 20, ascii_chars))
    digits = "0123456789.-"
    src.append(render("font16x32", TTF_B, 16, 32, 28, digits))
    src.append("""
const font_t FONT_SMALL = {8, 16, 0x20, 95, &font8x16_data[0][0]};
const font_t FONT_MEDIUM = {12, 24, 0x20, 95, &font12x24_data[0][0]};
/* Fuente grande solo digitos: el mapeo de caracteres se hace en font_glyph() */
const font_t FONT_BIG_DIGITS = {16, 32, 0, 12, &font16x32_data[0][0]};
""")
    open(dst, "w").write("\n".join(src))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "firmware/src/font_data.c")

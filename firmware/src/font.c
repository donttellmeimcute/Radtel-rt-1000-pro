#include "font.h"
const uint8_t *font_glyph(const font_t *f, char c)
{
    unsigned idx;
    int bytes = ((f->w + 7) / 8) * f->h;
    if (f->first == 0) {               /* fuente de digitos: "0123456789.-" */
        if (c >= '0' && c <= '9') idx = (unsigned)(c - '0');
        else if (c == '.') idx = 10;
        else if (c == '-') idx = 11;
        else return 0;
    } else {
        if ((uint8_t)c < f->first || (uint8_t)c >= f->first + f->count) return 0;
        idx = (uint8_t)c - f->first;
    }
    return f->data + idx * bytes;
}

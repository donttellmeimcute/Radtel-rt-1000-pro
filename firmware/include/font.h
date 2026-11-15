#pragma once
#include <stdint.h>
typedef struct {
    uint8_t w, h;          /* tamano de celda en pixeles */
    uint8_t first;         /* primer caracter (0 = tabla especial de digitos) */
    uint8_t count;
    const uint8_t *data;   /* filas MSB-first, (w+7)/8 bytes por fila */
} font_t;
extern const font_t FONT_SMALL, FONT_MEDIUM, FONT_BIG_DIGITS;
/* Devuelve el glifo o NULL si el caracter no existe en la fuente */
const uint8_t *font_glyph(const font_t *f, char c);

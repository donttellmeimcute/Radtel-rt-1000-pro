#pragma once
#include <stdint.h>
#include "font.h"
/* Pantalla 240x320 (GC9307) en vertical. API con origen ARRIBA-izquierda,
 * y creciendo hacia abajo (el driver convierte al marco fisico del panel). */
#define LCD_W 240
#define LCD_H 320
#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
enum {
    C_BLACK = 0x0000, C_WHITE = 0xFFFF, C_RED = 0xF800, C_GREEN = 0x07E0, C_BLUE = 0x001F,
    C_YELLOW = 0xFFE0, C_CYAN = 0x07FF, C_MAGENTA = 0xF81F, C_ORANGE = 0xFC83,
    C_GRAY = 0x8410, C_DGRAY = 0x4208,
};
void lcd_init(void);
void lcd_backlight(uint8_t level);      /* 0 = apagado, 255 = maximo (DAC) */
void lcd_fill(int x, int y, int w, int h, uint16_t c);
void lcd_clear(uint16_t c);
void lcd_pixel(int x, int y, uint16_t c);
void lcd_hline(int x, int y, int w, uint16_t c);
void lcd_vline(int x, int y, int h, uint16_t c);
void lcd_rect(int x, int y, int w, int h, uint16_t c);
int  lcd_char(int x, int y, char ch, const font_t *f, uint16_t fg, uint16_t bg);
int  lcd_text(int x, int y, const char *s, const font_t *f, uint16_t fg, uint16_t bg);
int  lcd_text_width(const char *s, const font_t *f);
void lcd_text_center(int y, const char *s, const font_t *f, uint16_t fg, uint16_t bg);
/* Bitmap 1bpp fila por fila MSB-first */
void lcd_bitmap(int x, int y, int w, int h, const uint8_t *bits, uint16_t fg, uint16_t bg);
void lcd_stream_begin(int x, int y, int w, int h);
void lcd_stream_bytes(const uint8_t *b, int n);

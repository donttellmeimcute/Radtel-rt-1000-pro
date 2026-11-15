/* Driver de la pantalla del RT-1000 Pro.
 * Hardware (ingenieria inversa del firmware original, ver docs/hardware.md):
 *   SPI3 solo TX: SCK=PB3 (AF6), MOSI=PB5 (AF6), CS=PA15 (AF6, CS por hardware)
 *   DC=PC13 (0 = comando), RST=PC14 (activo bajo), retroiluminacion = DAC1 (PA4)
 * El panel trabaja con inversion activada (0x21), por eso se envia ~color.
 * Marco fisico: x 0..239 = RASET(0x2B), y 0..319 = CASET(0x2A) con y=0 abajo;
 * el flujo de pixeles va por columnas, de abajo hacia arriba.                   */
#include "lcd.h"
#include "gpio.h"
#include "system.h"

static const pin_t P_SCK = {GPIOB, 3}, P_MOSI = {GPIOB, 5}, P_CS = {GPIOA, 15};
static const pin_t P_DC = {GPIOC, 13}, P_RST = {GPIOC, 14}, P_BL = {GPIOA, 4};

static inline void spi_wait_idle(void)
{
    while (!(SPI3->sts & SPI_STS_TDBE)) { }
    while (SPI3->sts & SPI_STS_BF) { }
}
static inline void wr8(uint8_t v)
{
    while (!(SPI3->sts & SPI_STS_TDBE)) { }
    SPI3->dt = v;
}
static void cmd(uint8_t c)
{
    spi_wait_idle();
    gpio_clr(P_DC);
    wr8(c);
    spi_wait_idle();
    gpio_set(P_DC);
}
static void cmd_data(uint8_t c, const uint8_t *d, int n)
{
    cmd(c);
    while (n--) wr8(*d++);
}
static inline void px(uint16_t c) { c = (uint16_t)~c; wr8(c >> 8); wr8(c & 0xFF); }

/* Ventana en coordenadas logicas (origen arriba-izquierda) */
static void window(int x, int y, int w, int h)
{
    int y0 = LCD_H - (y + h), y1 = LCD_H - 1 - y;     /* fisico: y=0 abajo */
    int x1 = x + w - 1;
    uint8_t a[4] = {(uint8_t)(x >> 8), (uint8_t)x, (uint8_t)(x1 >> 8), (uint8_t)x1};
    uint8_t b[4] = {(uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1};
    cmd_data(0x2B, a, 4);
    cmd_data(0x2A, b, 4);
    cmd(0x2C);
}

static int clip(int *x, int *y, int *w, int *h)
{
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > LCD_W) *w = LCD_W - *x;
    if (*y + *h > LCD_H) *h = LCD_H - *y;
    return *w > 0 && *h > 0;
}

void lcd_backlight(uint8_t level)
{
    DAC_D1DTH8R = level;
}

void lcd_init(void)
{
    CRM_AHBEN1 |= CRM_AHBEN1_GPIOA | CRM_AHBEN1_GPIOB | CRM_AHBEN1_GPIOC;
    CRM_APB1EN |= CRM_APB1EN_SPI3 | CRM_APB1EN_DAC;

    gpio_af(P_SCK, 6); gpio_af(P_MOSI, 6); gpio_af(P_CS, 6);
    gpio_mode(P_DC, GPIO_OUT, GPIO_NOPULL);
    gpio_mode(P_RST, GPIO_OUT, GPIO_NOPULL);
    gpio_mode(P_BL, GPIO_ANALOG, GPIO_NOPULL);

    /* SPI3: maestro, solo TX en una linea (slben|slbtd), div /2, 8 bits, MSB,
     * CPOL=1 CPHA=0 como el original, CS hardware (ctrl2.hwcsoe) */
    SPI3->ctrl1 = 0;
    SPI3->ctrl2 = 1u << 2;                      /* hwcsoe */
    SPI3->ctrl1 = (1u << 15) | (1u << 14)       /* slben, slbtd */
                | (1u << 2)                     /* msten */
                | (0u << 3)                     /* mdiv /2 */
                | (1u << 1);                    /* clkpol=1, clkpha=0 */
    SPI3->ctrl1 |= 1u << 6;                     /* spien */

    DAC_CTRL |= 1u;                             /* DAC canal 1 on */
    lcd_backlight(0);

    gpio_set(P_RST); delay_ms(1);
    gpio_clr(P_RST); delay_ms(1);
    gpio_set(P_RST); delay_ms(120);

    static const uint8_t gp[14] = {0x0C, 0x46, 0x25, 0x56, 0xAC, 0x24, 0x25, 0x00, 0x00, 0x12, 0x15, 0x16, 0x17, 0x0F};
    static const uint8_t gn[14] = {0x00, 0x26, 0x25, 0x3A, 0xB9, 0x0F, 0x10, 0x00, 0x00, 0x07, 0x07, 0x17, 0x16, 0x0F};
    cmd(0xFE); cmd(0xFE); cmd(0xEF);
    cmd_data(0xB3, (const uint8_t[]){0x03}, 1);
    cmd_data(0xB6, (const uint8_t[]){0x01}, 1);
    cmd_data(0xA3, (const uint8_t[]){0x11}, 1);
    cmd(0x21);
    cmd_data(0x36, (const uint8_t[]){0xE0}, 1);
    cmd_data(0x3A, (const uint8_t[]){0x05}, 1);
    cmd_data(0xB4, (const uint8_t[]){0x21}, 1);
    cmd_data(0xB1, (const uint8_t[]){0xC0}, 1);
    cmd_data(0xE6, (const uint8_t[]){0x50, 0x43}, 2);
    cmd_data(0xE7, (const uint8_t[]){0x38, 0x43}, 2);
    cmd_data(0xF0, gp, 14);
    cmd_data(0xF1, gn, 14);
    cmd_data(0x35, (const uint8_t[]){0x00}, 1);
    cmd_data(0x44, (const uint8_t[]){0x00}, 1);
    cmd(0xFE); cmd(0xFF);
    cmd(0x11); delay_ms(120);
    cmd(0x29);
    lcd_clear(C_BLACK);
}

void lcd_fill(int x, int y, int w, int h, uint16_t c)
{
    if (!clip(&x, &y, &w, &h)) return;
    window(x, y, w, h);
    uint16_t ic = (uint16_t)~c;
    for (int32_t n = (int32_t)w * h; n > 0; n--) { wr8(ic >> 8); wr8(ic & 0xFF); }
}

void lcd_clear(uint16_t c) { lcd_fill(0, 0, LCD_W, LCD_H, c); }
void lcd_pixel(int x, int y, uint16_t c) { lcd_fill(x, y, 1, 1, c); }
void lcd_hline(int x, int y, int w, uint16_t c) { lcd_fill(x, y, w, 1, c); }
void lcd_vline(int x, int y, int h, uint16_t c) { lcd_fill(x, y, 1, h, c); }
void lcd_rect(int x, int y, int w, int h, uint16_t c)
{
    lcd_hline(x, y, w, c); lcd_hline(x, y + h - 1, w, c);
    lcd_vline(x, y, h, c); lcd_vline(x + w - 1, y, h, c);
}

/* El flujo va por columnas (x externo) y de abajo hacia arriba (y interno) */
void lcd_bitmap(int x, int y, int w, int h, const uint8_t *bits, uint16_t fg, uint16_t bg)
{
    if (x < 0 || y < 0 || x + w > LCD_W || y + h > LCD_H) return;
    int bpr = (w + 7) / 8;
    window(x, y, w, h);
    for (int cx = 0; cx < w; cx++)
        for (int ry = h - 1; ry >= 0; ry--)
            px((bits[ry * bpr + cx / 8] & (0x80 >> (cx & 7))) ? fg : bg);
}

int lcd_char(int x, int y, char ch, const font_t *f, uint16_t fg, uint16_t bg)
{
    const uint8_t *g = font_glyph(f, ch);
    if (!g) g = font_glyph(f, ' ');
    if (g) lcd_bitmap(x, y, f->w, f->h, g, fg, bg);
    else lcd_fill(x, y, f->w, f->h, bg);
    return f->w;
}

int lcd_text(int x, int y, const char *s, const font_t *f, uint16_t fg, uint16_t bg)
{
    int x0 = x;
    for (; *s && x + f->w <= LCD_W; s++) x += lcd_char(x, y, *s, f, fg, bg);
    return x - x0;
}

int lcd_text_width(const char *s, const font_t *f)
{
    int n = 0; while (*s++) n++;
    return n * f->w;
}

void lcd_text_center(int y, const char *s, const font_t *f, uint16_t fg, uint16_t bg)
{
    int w = lcd_text_width(s, f);
    lcd_text((LCD_W - w) / 2, y, s, f, fg, bg);
}

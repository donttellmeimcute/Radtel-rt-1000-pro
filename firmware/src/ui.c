#include "ui.h"
#include "radio.h"
#include "settings.h"
#include "adc.h"
#include "nmea.h"
#include <stdio.h>
#include <string.h>

extern gps_fix_t g_gps;
extern uint8_t g_aprs_unread;
#define LIST_Y 44
#define LIST_ROW 24
#define LIST_ROWS 11

void fmt_freq(char *o, uint32_t f)
{
    sprintf(o, "%3lu.%05lu", (unsigned long)(f / 100000u), (unsigned long)(f % 100000u));
}

void fmt_tone(char *o, uint16_t t)
{
    int ty = tone_type(t);
    uint16_t c = tone_code(t);
    if (ty == 1) sprintf(o, "%u.%u", c / 10, c % 10);
    else if (ty == 2 || ty == 3) sprintf(o, "D%03o%c", c, ty == 2 ? 'N' : 'I');
    else strcpy(o, "OFF");
}

void ui_status_bar(int force)
{
    static int last_pct = -1, last_min = -1;
    static uint8_t last_flags = 0xFF;
    uint16_t mv = battery_mv();
    int pct = battery_percent(mv);
    uint8_t flags = (uint8_t)((g_set.key_lock ? 1 : 0) | (g_set.gps_on ? 2 : 0) | (g_gps.valid ? 4 : 0) |
                              (g_set.dual_watch ? 8 : 0) | (g_set.vox ? 16 : 0) | (g_aprs_unread ? 32 : 0) |
                              (g_set.aprs_beacon_s ? 64 : 0));
    int minute = g_gps.valid ? g_gps.hh * 60 + g_gps.mm : -1;
    if (!force && pct / 5 == last_pct / 5 && flags == last_flags && minute == last_min) return;
    last_pct = pct; last_flags = flags; last_min = minute;
    char b[24];
    lcd_fill(0, 0, LCD_W, STATUS_H, RGB(30, 30, 40));
    int x = 2;
    if (flags & 1) { lcd_text(x, 2, "L", &FONT_SMALL, C_YELLOW, RGB(30, 30, 40)); x += 12; }
    if (flags & 2) { lcd_text(x, 2, "GPS", &FONT_SMALL, (flags & 4) ? C_GREEN : C_RED, RGB(30, 30, 40)); x += 28; }
    if (flags & 64) { lcd_text(x, 2, "AP", &FONT_SMALL, C_CYAN, RGB(30, 30, 40)); x += 20; }
    if (flags & 32) { lcd_text(x, 2, "M", &FONT_SMALL, C_MAGENTA, RGB(30, 30, 40)); x += 12; }
    if (flags & 8) { lcd_text(x, 2, "DW", &FONT_SMALL, UI_ACCENT, RGB(30, 30, 40)); x += 20; }
    if (flags & 16) { lcd_text(x, 2, "VOX", &FONT_SMALL, C_ORANGE, RGB(30, 30, 40)); x += 28; }
    if (minute >= 0) {
        int m = minute + g_set.tz_quarter_h * 15;
        m = (m % 1440 + 1440) % 1440;
        sprintf(b, "%02d:%02d", m / 60, m % 60);
        lcd_text(120, 2, b, &FONT_SMALL, UI_FG, RGB(30, 30, 40));
    }
    sprintf(b, "%d.%dV", mv / 1000, (mv / 100) % 10);
    lcd_text(170, 2, b, &FONT_SMALL, pct < 15 ? C_RED : UI_FG, RGB(30, 30, 40));
    /* icono de bateria */
    lcd_rect(214, 4, 22, 12, UI_FG);
    lcd_fill(236, 7, 2, 6, UI_FG);
    lcd_fill(216, 6, 18 * pct / 100, 8, pct < 15 ? C_RED : C_GREEN);
}

void ui_title(const char *t)
{
    lcd_fill(0, STATUS_H, LCD_W, 22, UI_SEL_BG);
    lcd_text_center(STATUS_H + 3, t, &FONT_SMALL, UI_FG, UI_SEL_BG);
}

void ui_footer(const char *t)
{
    lcd_fill(0, LCD_H - 18, LCD_W, 18, RGB(30, 30, 40));
    lcd_text(4, LCD_H - 17, t, &FONT_SMALL, UI_DIM, RGB(30, 30, 40));
}

void ui_clear_body(void) { lcd_fill(0, STATUS_H, LCD_W, LCD_H - STATUS_H, UI_BG); }

void ui_message(const char *l1, const char *l2, uint16_t color)
{
    lcd_fill(16, 110, LCD_W - 32, 84, RGB(20, 20, 30));
    lcd_rect(16, 110, LCD_W - 32, 84, color);
    lcd_text_center(130, l1, &FONT_MEDIUM, color, RGB(20, 20, 30));
    if (l2) lcd_text_center(162, l2, &FONT_SMALL, UI_FG, RGB(20, 20, 30));
}

void ui_list(int count, int sel, int *top, ui_item_fn item, ui_value_fn value)
{
    char b1[40], b2[40];
    if (sel < *top) *top = sel;
    if (sel >= *top + LIST_ROWS) *top = sel - LIST_ROWS + 1;
    for (int r = 0; r < LIST_ROWS; r++) {
        int i = *top + r;
        int y = LIST_Y + r * LIST_ROW;
        uint16_t bg = (i == sel) ? UI_SEL_BG : UI_BG;
        lcd_fill(0, y, LCD_W - 4, LIST_ROW, bg);
        if (i >= count) continue;
        const char *t = item(i, b1);
        lcd_text(6, y + 4, t, &FONT_SMALL, UI_FG, bg);
        if (value) {
            const char *v = value(i, b2);
            if (v) {
                int w = lcd_text_width(v, &FONT_SMALL);
                lcd_text(LCD_W - 10 - w, y + 4, v, &FONT_SMALL, UI_ACCENT, bg);
            }
        }
    }
    /* barra de desplazamiento */
    lcd_fill(LCD_W - 4, LIST_Y, 4, LIST_ROWS * LIST_ROW, RGB(30, 30, 40));
    if (count > LIST_ROWS) {
        int h = LIST_ROWS * LIST_ROW * LIST_ROWS / count;
        int y = LIST_Y + (LIST_ROWS * LIST_ROW - h) * *top / (count - LIST_ROWS);
        lcd_fill(LCD_W - 4, y, 4, h, UI_DIM);
    }
}

void ui_rssi_bar(int x, int y, int w, int dbm)
{
    /* -130 dBm .. -50 dBm */
    int v = (dbm + 130) * w / 80;
    if (v < 0) v = 0;
    if (v > w) v = w;
    lcd_fill(x, y, v, 6, dbm > -90 ? C_GREEN : dbm > -110 ? C_YELLOW : C_ORANGE);
    lcd_fill(x + v, y, w - v, 6, RGB(40, 40, 40));
}

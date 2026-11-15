/* Receptor de radiodifusion con el Si4732: FM, MW, LW, SW y SSB (con parche). */
#include "app.h"
#include "ui.h"
#include "si473x.h"
#include "radio.h"
#include "bk4819.h"
#include "settings.h"
#include "audio.h"
#include "board.h"
#include "system.h"
#include <stdio.h>

typedef struct { const char *name; uint8_t mode; uint16_t lo, hi, step, def; } bc_band_t;
/* FM en unidades de 10 kHz, el resto en kHz */
static const bc_band_t bands[] = {
    {"FM", SI_MODE_FM, 6400, 10800, 10, 10110},
    {"MW", SI_MODE_AM, 520, 1710, 10, 1000},
    {"SW 41m", SI_MODE_AM, 7100, 7600, 5, 7200},
    {"SW", SI_MODE_AM, 2300, 30000, 5, 9600},
    {"LW", SI_MODE_AM, 153, 279, 9, 198},
    {"SSB 40m", SI_MODE_LSB, 7000, 7300, 1, 7100},
    {"SSB 20m", SI_MODE_USB, 14000, 14350, 1, 14200},
    {"SSB HF", SI_MODE_USB, 1800, 30000, 1, 3700},
};
#define NB ((int)(sizeof(bands) / sizeof(bands[0])))
static uint8_t started, bw_idx, sel_field;   /* 0 freq, 1 banda, 2 ancho, 3 BFO, 4 AGC, 5 vol */
static uint16_t step_mul = 1;
static si_status_t st;

static void draw(void)
{
    const bc_band_t *b = &bands[g_set.bc_band % NB];
    uint16_t f = g_set.bc_freq[g_set.bc_band % 8];
    char s[128];
    lcd_fill(0, 50, LCD_W, 200, UI_BG);
    lcd_text(8, 50, b->name, &FONT_MEDIUM, UI_ACCENT, UI_BG);
    if (b->mode == SI_MODE_FM) sprintf(s, "%3u.%02u", f / 100, f % 100);
    else sprintf(s, "%5u", f);
    int x = 20;
    for (int i = 0; s[i]; i++) { lcd_char(x, 90, s[i] == ' ' ? '-' : s[i], &FONT_BIG_DIGITS, UI_FG, UI_BG); x += s[i] == '.' ? 8 : 16; }
    lcd_text(x + 6, 106, b->mode == SI_MODE_FM ? "MHz" : "kHz", &FONT_SMALL, UI_DIM, UI_BG);
    static const char *fields[6] = {"Frec", "Banda", "Ancho", "BFO", "AGC", "Vol"};
    char v[6][16];
    sprintf(v[0], "x%u", step_mul * b->step);
    sprintf(v[1], "%s", b->name);
    sprintf(v[2], "%u", bw_idx);
    sprintf(v[3], "%dHz", g_set.bc_bfo);
    sprintf(v[4], g_set.bc_agc ? "%u" : "AUTO", g_set.bc_agc);
    sprintf(v[5], "%u", g_set.bc_volume);
    for (int i = 0; i < 6; i++) {
        uint16_t bg = i == sel_field ? UI_SEL_BG : UI_BG;
        snprintf(s, sizeof(s), "%-5s %s", fields[i], v[i]);
        lcd_fill(8 + (i % 2) * 116, 140 + (i / 2) * 22, 112, 20, bg);
        lcd_text(10 + (i % 2) * 116, 142 + (i / 2) * 22, s, &FONT_SMALL, UI_FG, bg);
    }
    if ((b->mode == SI_MODE_LSB || b->mode == SI_MODE_USB) && !si473x_ssb_patch_available())
        lcd_text(8, 216, "Falta parche SSB (ver docs)", &FONT_SMALL, UI_WARN, UI_BG);
}

static void start(void)
{
    const bc_band_t *b = &bands[g_set.bc_band % NB];
    uint16_t *f = &g_set.bc_freq[g_set.bc_band % 8];
    if (*f < b->lo || *f > b->hi) *f = b->def;
    int r = si473x_start(b->mode, *f);
    started = r == 0;
    if (r == -2) app_toast("Parche SSB no instalado");
    else if (r) app_toast("Error del receptor");
    if (started) {
        si473x_set_band_limits(b->lo, b->hi, b->step);
        si473x_set_volume(g_set.bc_volume);
        si473x_set_agc(g_set.bc_agc);
        si473x_set_bandwidth(bw_idx);
        if (b->mode >= SI_MODE_LSB) si473x_set_ssb_bfo(g_set.bc_bfo);
    }
}

static void bc_enter(void)
{
    radio_sleep();                       /* BK4819 en reposo */
    gpio_clr(PIN_RF_PATH);               /* ruta como el original al usar el receptor */
    audio_amp_request(AMP_BCAST, 1);
    ui_title("Receptor AM/FM/SW");
    ui_footer("MENU:campo  ^v:cambiar  #:busca");
    start();
    draw();
}

static void bc_leave(void)
{
    si473x_stop();
    audio_amp_request(AMP_BCAST, 0);
    app_retune();
}

static void bc_key(key_event_t *e)
{
    if (e->ev != EV_PRESS && e->ev != EV_REPEAT) return;
    const bc_band_t *b = &bands[g_set.bc_band % NB];
    uint16_t *f = &g_set.bc_freq[g_set.bc_band % 8];
    int d = e->key == KEY_UP ? 1 : e->key == KEY_DOWN ? -1 : 0;
    if (e->key == KEY_EXIT) { app_goto(SCR_MAIN); return; }
    if (e->key == KEY_MENU) { sel_field = (uint8_t)((sel_field + 1) % 6); draw(); return; }
    if (e->key == KEY_HASH) { si473x_seek(1); return; }
    if (e->key == KEY_STAR) { si473x_seek(0); return; }
    if (e->key == KEY_1) { step_mul = 1; } else if (e->key == KEY_2) { step_mul = 10; } else if (e->key == KEY_3) { step_mul = 100; }
    if (d) {
        switch (sel_field) {
        case 0: {
            int32_t nf = *f + d * (int32_t)(b->step * step_mul);
            if (nf < b->lo) nf = b->hi;
            if (nf > b->hi) nf = b->lo;
            *f = (uint16_t)nf; si473x_tune(*f);
        } break;
        case 1: g_set.bc_band = (uint8_t)((g_set.bc_band + NB + d) % NB); start(); break;
        case 2: bw_idx = (uint8_t)((bw_idx + 7 + d) % 7); si473x_set_bandwidth(bw_idx); break;
        case 3: g_set.bc_bfo = (int16_t)(g_set.bc_bfo + d * 25); si473x_set_ssb_bfo(g_set.bc_bfo); break;
        case 4: g_set.bc_agc = (uint8_t)((g_set.bc_agc + 38 + d) % 38); si473x_set_agc(g_set.bc_agc); break;
        case 5: if ((d > 0 && g_set.bc_volume < 63) || (d < 0 && g_set.bc_volume > 0)) g_set.bc_volume = (uint8_t)(g_set.bc_volume + d); si473x_set_volume(g_set.bc_volume); break;
        }
        settings_mark_dirty();
    }
    draw();
}

static void bc_tick(void)
{
    static uint8_t div;
    if (!started || ++div < 25) return;
    div = 0;
    si473x_status(&st);
    if (st.freq && st.freq != g_set.bc_freq[g_set.bc_band % 8]) {
        g_set.bc_freq[g_set.bc_band % 8] = st.freq; draw();
    }
    char s[32];
    sprintf(s, "RSSI %2u dBuV  SNR %2u dB", st.rssi, st.snr);
    lcd_text(8, 250, s, &FONT_SMALL, UI_FG, UI_BG);
    lcd_fill(8, 270, 224, 6, RGB(40, 40, 40));
    lcd_fill(8, 270, st.rssi * 224 / 80 > 224 ? 224 : st.rssi * 224 / 80, 6, C_GREEN);
}

const screen_ops_t scr_bcast = {bc_enter, bc_key, bc_tick, bc_leave};

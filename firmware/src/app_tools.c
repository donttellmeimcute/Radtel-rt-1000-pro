/* Herramientas: DTMF (envio y control remoto), busqueda de subtonos,
 * frecuencimetro, analizador de espectro, lista de canales y "Acerca de". */
#include "app.h"
#include "ui.h"
#include "radio.h"
#include "settings.h"
#include "bk4819.h"
#include "tones.h"
#include "system.h"
#include "audio.h"
#include "board.h"
#include "version.h"
#include "layout.h"
#include <stdio.h>
#include <string.h>

/* ============================ DTMF ============================ */
int dtmf_send_string(const char *s)
{
    int was_rx = g_radio.state != RS_TX;
    if (was_rx && radio_tx_start()) return -1;
    delay_ms(g_set.dtmf_delay_100ms * 100u);
    bk4819_dtmf_tx_enable();
    for (; *s; s++) {
        bk4819_dtmf_tone(*s);
        delay_ms(g_set.dtmf_tone_ms ? g_set.dtmf_tone_ms : 80);
        bk4819_write(0x70, 0);
        delay_ms(g_set.dtmf_gap_ms ? g_set.dtmf_gap_ms : 80);
    }
    bk4819_set_af(0);
    bk4819_tx_on(tone_type(g_radio.cur.tx_tone) != 0);
    if (was_rx) radio_tx_stop();
    return 0;
}

static char rx_dtmf[24];
static uint8_t rx_dtmf_len;
static uint32_t rx_dtmf_ms;

void dtmf_service(void)
{
    static uint8_t enabled;
    if (g_radio.state != RS_RX) { enabled = 0; return; }
    if (!enabled) { bk4819_dtmf_rx_enable(g_set.dtmf_threshold); enabled = 1; }
    int c = bk4819_dtmf_rx_get();
    if (c >= 0) {
        if (rx_dtmf_len < sizeof(rx_dtmf) - 1) { rx_dtmf[rx_dtmf_len++] = (char)c; rx_dtmf[rx_dtmf_len] = 0; }
        rx_dtmf_ms = g_ticks_ms;
    }
    if (rx_dtmf_len && g_ticks_ms - rx_dtmf_ms > 1500) {
        if (g_set.dtmf_remote) {
            if (g_set.dtmf_kill[0] && strstr(rx_dtmf, g_set.dtmf_kill)) { g_set.killed = 2; settings_save(); }
            else if (g_set.dtmf_stun[0] && strstr(rx_dtmf, g_set.dtmf_stun)) { g_set.killed = 1; settings_save(); }
            else if (g_set.dtmf_wake[0] && strstr(rx_dtmf, g_set.dtmf_wake) && g_set.killed == 1) { g_set.killed = 0; settings_save(); }
        }
        if (g_screen == SCR_MAIN || g_screen == SCR_DTMF) {
            char b[32]; sprintf(b, "DTMF: %s", rx_dtmf);
            app_toast(b);
        }
        rx_dtmf_len = 0; rx_dtmf[0] = 0;
    }
}

static char dt_buf[17];
static uint8_t dt_len, dt_mem;
static void dt_draw(void)
{
    char b[32];
    lcd_fill(0, 50, LCD_W, 180, UI_BG);
    lcd_text(8, 54, "Codigo:", &FONT_SMALL, UI_DIM, UI_BG);
    lcd_text(8, 76, dt_len ? dt_buf : "_", &FONT_MEDIUM, C_YELLOW, UI_BG);
    sprintf(b, "Memoria %u: %s", dt_mem + 1, g_set.dtmf_mem[dt_mem]);
    lcd_text(8, 120, b, &FONT_SMALL, UI_FG, UI_BG);
    lcd_text(8, 150, "PTT/MENU: enviar codigo", &FONT_SMALL, UI_DIM, UI_BG);
    lcd_text(8, 170, "^v: memoria  #largo: guardar", &FONT_SMALL, UI_DIM, UI_BG);
}
static void dt_enter(void) { dt_len = 0; dt_buf[0] = 0; ui_title("DTMF"); ui_footer("0-9 * # A-D(largo 1-4)"); dt_draw(); }
static void dt_key(key_event_t *e)
{
    static const char longmap[10] = {0, 'A', 'B', 'C', 'D', 0, 0, 0, 0, 0};
    if (e->ev == EV_LONG && e->key >= KEY_1 && e->key <= KEY_4 && dt_len) { dt_buf[dt_len - 1] = longmap[e->key]; dt_draw(); return; }
    if (e->ev == EV_LONG && e->key == KEY_HASH && dt_len) { strcpy(g_set.dtmf_mem[dt_mem], dt_buf); settings_mark_dirty(); app_toast("Guardado"); return; }
    if (e->ev != EV_PRESS) return;
    char c = 0;
    if (e->key <= KEY_9) c = (char)('0' + e->key);
    else if (e->key == KEY_STAR) c = '*';
    else if (e->key == KEY_HASH) c = '#';
    if (c && dt_len < 16) { dt_buf[dt_len++] = c; dt_buf[dt_len] = 0; }
    if (e->key == KEY_EXIT) { if (dt_len) dt_buf[--dt_len] = 0; else { app_goto(SCR_MAIN); return; } }
    if (e->key == KEY_UP) dt_mem = (uint8_t)((dt_mem + 1) % 16);
    if (e->key == KEY_DOWN) dt_mem = (uint8_t)((dt_mem + 15) % 16);
    if (e->key == KEY_MENU) {
        const char *s = dt_len ? dt_buf : g_set.dtmf_mem[dt_mem];
        if (*s) { ui_message("Enviando DTMF", s, UI_ACCENT); dtmf_send_string(s); }
    }
    dt_draw();
}
const screen_ops_t scr_dtmf = {dt_enter, dt_key, NULL, NULL};

/* ======================= Busca CTCSS/DCS ======================= */
static uint8_t ts_found;
static void ts_enter(void)
{
    ts_found = 0;
    ui_title("Busca CTCSS/DCS");
    ui_footer("MENU:aplicar al VFO  EXIT");
    lcd_text(8, 60, "Escuchando...", &FONT_MEDIUM, UI_FG, UI_BG);
    bk4819_tone_scan_start();
}
static int ts_type; static uint16_t ts_code;
static void ts_tick(void)
{
    int t; uint16_t c;
    if (bk4819_tone_scan_result(&t, &c)) {
        if (!ts_found || t != ts_type || c != ts_code) {
            char b[24];
            ts_found = 1; ts_type = t; ts_code = c;
            fmt_tone(b, tone_make(t, c));
            lcd_fill(0, 60, LCD_W, 60, UI_BG);
            lcd_text(8, 60, "Encontrado:", &FONT_SMALL, UI_DIM, UI_BG);
            lcd_text(8, 84, b, &FONT_MEDIUM, C_GREEN, UI_BG);
            audio_beep(1500, 80);
        }
    }
}
static void ts_key(key_event_t *e)
{
    if (e->ev != EV_PRESS) return;
    if (e->key == KEY_MENU && ts_found) {
        channel_t *v = &g_set.area[g_set.active].vfo;
        v->rx_tone = v->tx_tone = tone_make(ts_type, ts_code);
        settings_mark_dirty();
        app_goto(SCR_MAIN);
    } else if (e->key == KEY_EXIT) app_goto(SCR_MAIN);
}
static void ts_leave(void) { app_retune(); }
const screen_ops_t scr_tonescan = {ts_enter, ts_key, ts_tick, ts_leave};

/* ======================== Frecuencimetro ======================== */
static uint32_t fc_last;
static void fc_enter(void)
{
    fc_last = 0;
    ui_title("Frecuencimetro");
    ui_footer("Acerca un radio transmitiendo. MENU:usar");
    lcd_text(8, 60, "Midiendo...", &FONT_MEDIUM, UI_FG, UI_BG);
    bk4819_freq_counter_start();
}
static void fc_tick(void)
{
    uint32_t f;
    if (bk4819_freq_counter_read(&f, 0) && f > 1800000 && f < 130000000) {
        char b[16];
        f = (f + 250) / 500 * 500;   /* redondeo a 5 kHz */
        if (f != fc_last) {
            fc_last = f; fmt_freq(b, f);
            lcd_fill(0, 60, LCD_W, 60, UI_BG);
            int x = 8;
            for (int i = 0; b[i]; i++) { lcd_char(x, 70, b[i] == ' ' ? '-' : b[i], &FONT_BIG_DIGITS, C_GREEN, UI_BG); x += b[i] == '.' ? 8 : 16; }
            audio_beep(1500, 60);
        }
    }
}
static void fc_key(key_event_t *e)
{
    if (e->ev != EV_PRESS) return;
    if (e->key == KEY_MENU && fc_last) {
        channel_t *v = &g_set.area[g_set.active].vfo;
        v->rx = v->tx = fc_last; settings_mark_dirty(); app_goto(SCR_MAIN);
    } else if (e->key == KEY_EXIT) app_goto(SCR_MAIN);
}
static void fc_leave(void) { bk4819_freq_counter_stop(); app_retune(); }
const screen_ops_t scr_fcount = {fc_enter, fc_key, fc_tick, fc_leave};

/* ========================= Espectro ========================= */
#define SP_N 112
static uint32_t sp_center;
static uint8_t sp_span_idx = 2, sp_x;
static const uint32_t sp_steps[5] = {125, 250, 1250, 2500, 10000};   /* por barra (10 Hz) */
static uint8_t sp_peak[SP_N];
static void sp_draw_axis(void)
{
    char b[64], f[16];
    lcd_fill(0, 44, LCD_W, 254, UI_BG);
    fmt_freq(f, sp_center);
    sprintf(b, "%s  paso %lu.%02lu kHz", f, (unsigned long)(sp_steps[sp_span_idx] / 100), (unsigned long)(sp_steps[sp_span_idx] % 100));
    lcd_text(4, 46, b, &FONT_SMALL, UI_FG, UI_BG);
    lcd_vline(LCD_W / 2, 66, 220, RGB(60, 60, 60));
}
static void sp_enter(void)
{
    channel_t c; app_area_channel(g_set.active, &c);
    sp_center = c.rx; sp_x = 0;
    memset(sp_peak, 0, sizeof(sp_peak));
    ui_title("Espectro");
    ui_footer("^v:frec  1/2:ancho  MENU:ir  EXIT");
    sp_draw_axis();
}
static void sp_tick(void)
{
    /* barre 4 puntos por tick */
    for (int k = 0; k < 4; k++) {
        uint32_t f = sp_center + (uint32_t)((int32_t)sp_x - SP_N / 2) * sp_steps[sp_span_idx];
        bk4819_set_freq(f, 0);
        delay_us(1800);
        int dbm = bk4819_rssi_dbm();
        int h = (dbm + 140) * 2;
        if (h < 0) h = 0;
        if (h > 210) h = 210;
        int x = sp_x * 2 + 8;
        lcd_fill(x, 76, 2, 210 - h, UI_BG);
        lcd_fill(x, 76 + 210 - h, 2, h, h > 100 ? C_GREEN : h > 60 ? C_YELLOW : UI_ACCENT);
        sp_x = (uint8_t)((sp_x + 1) % SP_N);
    }
}
static void sp_key(key_event_t *e)
{
    if (e->ev != EV_PRESS && e->ev != EV_REPEAT) return;
    if (e->key == KEY_UP) sp_center += sp_steps[sp_span_idx] * 8;
    else if (e->key == KEY_DOWN) sp_center -= sp_steps[sp_span_idx] * 8;
    else if (e->key == KEY_1 && sp_span_idx > 0) sp_span_idx--;
    else if (e->key == KEY_2 && sp_span_idx < 4) sp_span_idx++;
    else if (e->key == KEY_MENU) { channel_t *v = &g_set.area[g_set.active].vfo; v->rx = v->tx = sp_center; settings_mark_dirty(); app_goto(SCR_MAIN); return; }
    else if (e->key == KEY_EXIT) { app_goto(SCR_MAIN); return; }
    sp_draw_axis();
}
static void sp_leave(void) { app_retune(); }
const screen_ops_t scr_spectrum = {sp_enter, sp_key, sp_tick, sp_leave};

/* ======================= Lista de canales ======================= */
static int cl_sel, cl_top;
static const char *cl_item(int i, char *b)
{
    channel_t c; char n[17], f[16];
    if (!channel_load((uint16_t)i, &c)) { sprintf(b, "%03d  ---", i + 1); return b; }
    channel_name(&c, n); fmt_freq(f, c.rx);
    sprintf(b, "%03d %s %.9s", i + 1, f, n);
    return b;
}
static void cl_enter(void)
{
    if (g_set.area[g_set.active].mode == AREA_MR) cl_sel = g_set.area[g_set.active].ch;
    ui_title("Canales");
    ui_footer("MENU:usar  EXIT:salir");
    ui_list(CHANNEL_COUNT, cl_sel, &cl_top, cl_item, NULL);
}
static void cl_key(key_event_t *e)
{
    if (e->ev != EV_PRESS && e->ev != EV_REPEAT) return;
    if (e->key == KEY_UP) cl_sel = (cl_sel + CHANNEL_COUNT - 1) % CHANNEL_COUNT;
    else if (e->key == KEY_DOWN) cl_sel = (cl_sel + 1) % CHANNEL_COUNT;
    else if (e->key == KEY_MENU) {
        channel_t c;
        if (channel_load((uint16_t)cl_sel, &c)) {
            g_set.area[g_set.active].mode = AREA_MR; g_set.area[g_set.active].ch = (uint16_t)cl_sel;
            settings_mark_dirty(); app_retune(); app_goto(SCR_MAIN);
        }
        return;
    } else if (e->key == KEY_EXIT) { app_goto(SCR_MAIN); return; }
    ui_list(CHANNEL_COUNT, cl_sel, &cl_top, cl_item, NULL);
}
const screen_ops_t scr_chlist = {cl_enter, cl_key, NULL, NULL};

/* ========================= Acerca de ========================= */
static void ab_enter(void)
{
    char b[40];
    ui_title("Acerca de");
    lcd_text(8, 54, "RT-1000 Pro firmware libre", &FONT_SMALL, C_YELLOW, UI_BG);
    lcd_text(8, 76, "Version " FW_VERSION, &FONT_SMALL, UI_FG, UI_BG);
    sprintf(b, "Calibracion: %s", g_radio.cal_ok ? "fabrica OK" : "NO encontrada");
    lcd_text(8, 98, b, &FONT_SMALL, g_radio.cal_ok ? C_GREEN : C_RED, UI_BG);
    extern uint16_t g_flash_id;
    sprintf(b, "Flash SPI ID: %04X", g_flash_id);
    lcd_text(8, 120, b, &FONT_SMALL, UI_FG, UI_BG);
    lcd_text(8, 150, "Codigo fuente: GitHub", &FONT_SMALL, UI_DIM, UI_BG);
    lcd_text(8, 170, "donttellmeimcute/", &FONT_SMALL, UI_DIM, UI_BG);
    lcd_text(8, 190, "Radtel-rt-1000-pro", &FONT_SMALL, UI_DIM, UI_BG);
    ui_footer("EXIT:salir");
}
static void ab_key(key_event_t *e) { if (e->ev == EV_PRESS && e->key == KEY_EXIT) app_goto(SCR_MAIN); }
const screen_ops_t scr_about = {ab_enter, ab_key, NULL, NULL};

/* Pantalla principal: dos areas (A/B), VFO / canal / zona, entrada directa
 * de frecuencia, escaneo, doble escucha y alarma. */
#include "app.h"
#include "ui.h"
#include "radio.h"
#include "settings.h"
#include "system.h"
#include "audio.h"
#include "bk4819.h"
#include "board.h"
#include <stdio.h>
#include <string.h>

static char input[12];
static uint8_t input_len;
static uint8_t scanning;
static int8_t scan_dir = 1;
static uint32_t scan_hold_until, dw_switch_ms;
static uint8_t dw_listening_other;
static uint8_t alarm_on;
static int16_t last_rssi_px[2] = {-1, -1};

#define AREA_Y(a) (STATUS_H + 6 + (a) * 136)

static void draw_area(int a)
{
    channel_t c;
    char b[32], t[16];
    app_area_channel(a, &c);
    area_t *ar = &g_set.area[a];
    int active = (a == g_set.active);
    int y = AREA_Y(a);
    uint16_t bg = active ? RGB(10, 20, 40) : UI_BG;
    lcd_fill(0, y, LCD_W, 128, bg);
    if (active) lcd_rect(0, y, LCD_W, 128, g_radio.state == RS_TX ? C_RED : UI_ACCENT);

    /* encabezado */
    const char *mode = ar->mode == AREA_VFO ? "VFO" : ar->mode == AREA_MR ? "MR" : "ZONA";
    if (ar->mode == AREA_MR) sprintf(b, "%c %s CH-%03u", 'A' + a, mode, ar->ch + 1);
    else if (ar->mode == AREA_ZONE) sprintf(b, "%c Z%u-%u", 'A' + a, ar->zone + 1, ar->zone_pos + 1);
    else sprintf(b, "%c %s", 'A' + a, mode);
    lcd_text(6, y + 4, b, &FONT_SMALL, active ? UI_ACCENT : UI_DIM, bg);
    char name[17];
    channel_name(&c, name);
    if (name[0]) lcd_text(LCD_W - 6 - lcd_text_width(name, &FONT_SMALL), y + 4, name, &FONT_SMALL, C_YELLOW, bg);

    /* frecuencia grande */
    int show_input = active && input_len;
    if (show_input) {
        char s[12]; int i;
        for (i = 0; i < 9; i++) s[i] = '-';
        s[9] = 0;
        for (i = 0; i < input_len && i < 8; i++) s[i < 3 ? i : i + 1] = input[i];
        s[3] = '.';
        for (i = 0; s[i]; i++) lcd_char(8 + i * 16 + (i > 3 ? -8 : 0), y + 26, s[i], &FONT_BIG_DIGITS, C_YELLOW, bg);
    } else {
        uint32_t f = (a == g_set.active && g_radio.state == RS_TX) ? c.tx : c.rx;
        fmt_freq(b, f);
        int x = 8;
        for (int i = 0; b[i]; i++) {
            lcd_char(x, y + 26, b[i] == ' ' ? '-' : b[i], &FONT_BIG_DIGITS,
                     g_radio.state == RS_TX && active ? C_RED : UI_FG, bg);
            x += b[i] == '.' ? 8 : 16;
        }
        lcd_fill(x, y + 26, LCD_W - 4 - x, 32, bg);
        lcd_text(x + 4, y + 42, "MHz", &FONT_SMALL, UI_DIM, bg);
    }

    /* linea de informacion */
    static const char *mods[4] = {"FM", "AM", "USB", "LSB"};
    char rt[10], tt[10];
    fmt_tone(rt, c.rx_tone); fmt_tone(tt, c.tx_tone);
    sprintf(b, "%s %s %s", mods[ch_mod(&c)], ch_narrow(&c) ? "N" : "W", ch_high_power(&c) ? "H" : "L");
    lcd_text(6, y + 66, b, &FONT_SMALL, UI_FG, bg);
    if (c.tx != c.rx) {
        int32_t off = (int32_t)c.tx - (int32_t)c.rx;
        sprintf(t, "%c%ld.%03ld", off > 0 ? '+' : '-', (long)((off > 0 ? off : -off) / 100000),
                (long)(((off > 0 ? off : -off) / 100) % 1000));
        lcd_text(100, y + 66, t, &FONT_SMALL, C_ORANGE, bg);
    }
    sprintf(b, "R:%s T:%s", rt, tt);
    lcd_text(6, y + 86, b, &FONT_SMALL, UI_DIM, bg);
    if (ch_scrambler(&c)) { sprintf(b, "SCR%d", ch_scrambler(&c)); lcd_text(190, y + 86, b, &FONT_SMALL, C_MAGENTA, bg); }
    if (!radio_tx_allowed(c.tx)) lcd_text(190, y + 66, "RX", &FONT_SMALL, C_ORANGE, bg);
    last_rssi_px[a] = -1;
}

static void draw_footer(void)
{
    if (scanning) ui_footer(scan_dir > 0 ? "Escaneando >>  (EXIT para parar)" : "Escaneando <<  (EXIT para parar)");
    else ui_footer("MENU  *:Escan  #:A/B  0..9:frec");
}

static void m_enter(void)
{
    draw_area(0);
    draw_area(1);
    draw_footer();
}

void main_scan_toggle(void)
{
    scanning ^= 1;
    if (!scanning) app_retune();
    scan_hold_until = 0;
    draw_footer();
}

void alarm_toggle(void)
{
    alarm_on ^= 1;
    if (!alarm_on) { audio_tone(0); gpio_clr(PIN_LED_RED); if (g_radio.state == RS_TX) radio_tx_stop(); }
    else if (g_set.alarm_type >= 1) radio_tx_start();
}

static void step_freq(int dir)
{
    area_t *ar = &g_set.area[g_set.active];
    if (ar->mode == AREA_MR) {
        int n = channel_next_valid(ar->ch, dir);
        if (n >= 0) ar->ch = (uint16_t)n;
    } else if (ar->mode == AREA_ZONE) {
        int p = ar->zone_pos;
        for (int i = 0; i < ZONE_MAX_CH; i++) {
            p = (p + dir + ZONE_MAX_CH) % ZONE_MAX_CH;
            if (zone_channel(ar->zone, (uint16_t)p) >= 0) break;
        }
        ar->zone_pos = (uint8_t)p;
    } else {
        uint32_t st = step_table_10hz[g_set.step_idx % step_count];
        int32_t off = (int32_t)ar->vfo.tx - (int32_t)ar->vfo.rx;
        uint32_t f = ar->vfo.rx - ar->vfo.rx % st;
        f = dir > 0 ? f + st : (ar->vfo.rx % st ? f : f - st);
        if (f < 1800000) f = 130000000 - st;
        if (f >= 130000000) f = 1800000;
        ar->vfo.rx = f;
        ar->vfo.tx = (uint32_t)((int32_t)f + off);
    }
    settings_mark_dirty();
}

static void commit_input(void)
{
    area_t *ar = &g_set.area[g_set.active];
    if (ar->mode == AREA_MR || ar->mode == AREA_ZONE) {
        int n = 0;
        for (int i = 0; i < input_len; i++) n = n * 10 + (input[i] - '0');
        if (ar->mode == AREA_MR) {
            channel_t c;
            if (n >= 1 && n <= CHANNEL_COUNT && channel_load((uint16_t)(n - 1), &c)) ar->ch = (uint16_t)(n - 1);
            else app_toast("Canal vacio");
        } else if (n >= 1 && n <= ZONE_COUNT) { ar->zone = (uint8_t)(n - 1); ar->zone_pos = 0; }
    } else {
        uint32_t f = 0;
        int i;
        for (i = 0; i < 8; i++) f = f * 10 + (i < input_len ? (uint32_t)(input[i] - '0') : 0);
        /* 8 digitos = MMM.kkkhh -> unidades de 10 Hz */
        if (radio_band_of(f) >= 0) {
            int32_t off = (int32_t)ar->vfo.tx - (int32_t)ar->vfo.rx;
            ar->vfo.rx = f;
            ar->vfo.tx = (uint32_t)((int32_t)f + off);
            if (radio_band_of(ar->vfo.tx) < 0) ar->vfo.tx = f;
        } else app_toast("Fuera de banda");
    }
    input_len = 0;
    settings_mark_dirty();
    app_retune();
}

static void m_key(key_event_t *e)
{
    if (e->ev != EV_PRESS && e->ev != EV_REPEAT && e->ev != EV_LONG) return;
    area_t *ar = &g_set.area[g_set.active];
    if (alarm_on && e->key == KEY_EXIT) { alarm_toggle(); return; }
    if (scanning && (e->key == KEY_EXIT || e->key == KEY_STAR || e->key == KEY_MENU)) { main_scan_toggle(); return; }
    if (scanning && (e->key == KEY_UP || e->key == KEY_DOWN)) { scan_dir = e->key == KEY_UP ? 1 : -1; scan_hold_until = 0; draw_footer(); return; }

    if (e->key <= KEY_9 && e->ev == EV_PRESS) {
        int maxlen = ar->mode == AREA_VFO ? 8 : (ar->mode == AREA_MR ? 4 : 3);
        if (input_len < maxlen) input[input_len++] = (char)('0' + e->key);
        if (input_len == maxlen) commit_input();
        draw_area(g_set.active);
        return;
    }
    if (input_len) {
        if (e->key == KEY_EXIT) { input_len--; draw_area(g_set.active); }
        else if (e->key == KEY_MENU || e->key == KEY_HASH) { commit_input(); m_enter(); }
        return;
    }
    switch (e->key) {
    case KEY_UP: case KEY_DOWN:
        step_freq(e->key == KEY_UP ? 1 : -1);
        app_retune();
        draw_area(g_set.active);
        break;
    case KEY_MENU:
        if (e->ev == EV_LONG) app_goto(SCR_CHLIST);
        else if (e->ev == EV_PRESS) app_goto(SCR_MENU);
        break;
    case KEY_HASH:
        if (e->ev == EV_LONG) { g_set.key_lock ^= 1; ui_status_bar(1); app_toast(g_set.key_lock ? "Bloqueado" : "Desbloqueado"); }
        else if (e->ev == EV_PRESS) {
            g_set.active ^= 1; settings_mark_dirty(); app_retune(); m_enter();
        }
        break;
    case KEY_STAR:
        if (e->ev == EV_LONG) {             /* invertir TX/RX (reverse) */
            channel_t *v = &ar->vfo;
            uint32_t t = v->rx; v->rx = v->tx; v->tx = t;
            settings_mark_dirty(); app_retune(); draw_area(g_set.active);
        } else if (e->ev == EV_PRESS) main_scan_toggle();
        break;
    case KEY_EXIT:
        if (e->ev == EV_LONG) { ar->mode = (uint8_t)((ar->mode + 1) % 3); settings_mark_dirty(); app_retune(); m_enter(); }
        break;
    case KEY_0:
        break;
    default: break;
    }
}

/* Escaneo: VFO recorre [scan_lo, scan_hi]; MR/zona recorre canales validos */
static void scan_tick(void)
{
    uint32_t now = g_ticks_ms;
    if (g_radio.sq_open) {
        if (g_set.scan_mode == SCAN_SE) { scanning = 0; draw_footer(); return; }
        if (!scan_hold_until) scan_hold_until = now + (g_set.scan_mode == SCAN_TO ? g_set.scan_resume_s * 1000u : 0);
        if (g_set.scan_mode == SCAN_CO || now < scan_hold_until) return;
    } else if (scan_hold_until && g_set.scan_mode == SCAN_CO) {
        if (now < scan_hold_until + 2000) return;
    }
    scan_hold_until = 0;
    area_t *ar = &g_set.area[g_set.active];
    if (ar->mode == AREA_VFO) {
        uint32_t st = step_table_10hz[g_set.step_idx % step_count];
        uint32_t f = ar->vfo.rx + (scan_dir > 0 ? st : -st);
        if (f > g_set.scan_hi) f = g_set.scan_lo;
        if (f < g_set.scan_lo) f = g_set.scan_hi;
        ar->vfo.rx = ar->vfo.tx = f;
    } else step_freq(scan_dir);
    app_retune();
    draw_area(g_set.active);
}

/* Doble escucha: alterna brevemente al area inactiva */
static void dual_watch_tick(void)
{
    uint32_t now = g_ticks_ms;
    if (g_radio.state != RS_RX || g_radio.sq_open || scanning) return;
    if (now - dw_switch_ms < (dw_listening_other ? 60u : 400u)) return;
    dw_switch_ms = now;
    channel_t c;
    dw_listening_other ^= 1;
    app_area_channel(dw_listening_other ? g_set.active ^ 1 : g_set.active, &c);
    radio_tune(&c);
}

static void m_tick(void)
{
    if (scanning) scan_tick();
    else if (g_set.dual_watch) dual_watch_tick();
    if (alarm_on) {
        static uint16_t f = 850; static int8_t d = 1;
        f = (uint16_t)(f + d * 40);
        if (f >= 1450 || f <= 850) d = (int8_t)-d;
        if (g_set.alarm_type != 1) audio_tone(f);
        if (g_set.alarm_type >= 1 && g_radio.state == RS_TX) {
            bk4819_write(0x70, 0xC000 | 0x60);
            bk4819_write(0x71, (uint16_t)(f * 103244u / 10000u));
        }
        static uint8_t blink; if (++blink % 10 == 0) gpio_write(PIN_LED_RED, (blink / 10) & 1);
    }
    /* medidor de RSSI */
    int a = dw_listening_other ? g_set.active ^ 1 : g_set.active;
    int y = AREA_Y(a) + 108;
    int px = g_radio.sq_open ? (g_radio.rssi_dbm + 130) * 200 / 80 : 0;
    if (px != last_rssi_px[a]) {
        last_rssi_px[a] = (int16_t)px;
        uint16_t bg = a == g_set.active ? RGB(10, 20, 40) : UI_BG;
        if (g_radio.state == RS_TX) {
            lcd_fill(6, y, 228, 14, bg);
            lcd_text(6, y, "TX", &FONT_SMALL, C_RED, bg);
        } else if (g_radio.sq_open) {
            char b[12];
            sprintf(b, "%4ddBm", g_radio.rssi_dbm);
            lcd_text(170, y - 2, b, &FONT_SMALL, UI_FG, bg);
            ui_rssi_bar(6, y + 2, 160, g_radio.rssi_dbm);
        } else lcd_fill(6, y - 2, 228, 18, bg);
    }
}

const screen_ops_t scr_main = {m_enter, m_key, m_tick, NULL};

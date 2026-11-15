/* APRS (baliza, recepcion, lista) y GPS (NMEA, pantalla, traza). */
#include "app.h"
#include "ui.h"
#include "radio.h"
#include "settings.h"
#include "aprs_hw.h"
#include "afsk.h"
#include "uart.h"
#include "spiflash.h"
#include "layout.h"
#include "system.h"
#include "audio.h"
#include "bk4819.h"
#include "board.h"
#include <stdio.h>
#include <string.h>

gps_fix_t g_gps;
uint8_t g_aprs_unread;

/* ============================ GPS ============================ */
static const uint32_t gps_bauds[10] = {4800, 9600, 14400, 19200, 38400, 56000, 57600, 115200, 128000, 256000};
static uint8_t gps_running;
static uint32_t last_track_ms;

void gps_apply(void)
{
    if (g_set.gps_on) { uart_gps_init(gps_bauds[g_set.gps_baud_idx % 10]); gps_running = 1; }
    else if (gps_running) { uart_gps_stop(); gps_running = 0; g_gps.valid = 0; }
}

static void track_append(void)
{
    uint8_t cnt[2], rec[0x30];
    spiflash_read(FLASH_GPS_TRACK, cnt, 2);
    uint16_t n = (uint16_t)(cnt[0] << 8 | cnt[1]);
    if (n == 0xFFFF) n = 0;
    memset(rec, 0, sizeof(rec));
    rec[0] = g_gps.year; rec[1] = g_gps.mon; rec[2] = g_gps.day;
    rec[3] = g_gps.hh; rec[4] = g_gps.mm; rec[5] = g_gps.ss;
    memcpy(rec + 6, &g_gps.lat_e7, 4); memcpy(rec + 10, &g_gps.lon_e7, 4);
    memcpy(rec + 14, &g_gps.alt_dm, 4); memcpy(rec + 18, &g_gps.speed_kmh10, 2);
    memcpy(rec + 20, &g_gps.course_deg10, 2);
    spiflash_write(FLASH_GPS_TRACK + 2 + (n % GPS_TRACK_MAX) * 0x30u, rec, 0x30);
    n++;
    cnt[0] = (uint8_t)(n >> 8); cnt[1] = (uint8_t)n;
    spiflash_write(FLASH_GPS_TRACK, cnt, 2);
}

void gps_service(void)
{
    static uint8_t applied;
    if (!applied) { applied = 1; gps_apply(); }
    if (!gps_running) return;
    int c;
    while ((c = uart_gps_getc()) >= 0) nmea_feed(&g_gps, (char)c);
    if (g_set.gps_track && g_gps.valid && g_ticks_ms - last_track_ms > g_set.gps_track_s * 1000u) {
        last_track_ms = g_ticks_ms;
        track_append();
    }
}

static void fmt_coord(char *o, int32_t v, int lat)
{
    char h = lat ? (v < 0 ? 'S' : 'N') : (v < 0 ? 'W' : 'E');
    if (v < 0) v = -v;
    sprintf(o, "%c %3ld.%06ld", h, (long)(v / 10000000), (long)((v % 10000000) / 10));
}

static void gps_draw(void)
{
    char b[40];
    lcd_fill(0, 46, LCD_W, 250, UI_BG);
    if (!g_set.gps_on) { lcd_text(8, 60, "GPS apagado (Menu > GPS)", &FONT_SMALL, UI_WARN, UI_BG); return; }
    lcd_text(8, 50, g_gps.valid ? "Posicion valida" : "Sin posicion", &FONT_MEDIUM, g_gps.valid ? C_GREEN : C_RED, UI_BG);
    fmt_coord(b, g_gps.lat_e7, 1); lcd_text(8, 84, "Lat ", &FONT_SMALL, UI_DIM, UI_BG); lcd_text(48, 84, b, &FONT_SMALL, UI_FG, UI_BG);
    fmt_coord(b, g_gps.lon_e7, 0); lcd_text(8, 104, "Lon ", &FONT_SMALL, UI_DIM, UI_BG); lcd_text(48, 104, b, &FONT_SMALL, UI_FG, UI_BG);
    if (g_set.units_imperial) sprintf(b, "Alt  %ld ft", (long)(g_gps.alt_dm * 328 / 1000));
    else sprintf(b, "Alt  %ld.%ld m", (long)(g_gps.alt_dm / 10), (long)((g_gps.alt_dm < 0 ? -g_gps.alt_dm : g_gps.alt_dm) % 10));
    lcd_text(8, 124, b, &FONT_SMALL, UI_FG, UI_BG);
    if (g_set.units_imperial) sprintf(b, "Vel  %u mph", g_gps.speed_kmh10 * 62 / 1000);
    else sprintf(b, "Vel  %u.%u km/h", g_gps.speed_kmh10 / 10, g_gps.speed_kmh10 % 10);
    lcd_text(8, 144, b, &FONT_SMALL, UI_FG, UI_BG);
    sprintf(b, "Rumbo %u grados", g_gps.course_deg10 / 10); lcd_text(8, 164, b, &FONT_SMALL, UI_FG, UI_BG);
    sprintf(b, "Satelites %u", g_gps.sats); lcd_text(8, 184, b, &FONT_SMALL, UI_FG, UI_BG);
    sprintf(b, "UTC 20%02u-%02u-%02u %02u:%02u:%02u", g_gps.year, g_gps.mon, g_gps.day, g_gps.hh, g_gps.mm, g_gps.ss);
    lcd_text(8, 204, b, &FONT_SMALL, UI_FG, UI_BG);
    /* Localizador Maidenhead */
    if (g_gps.valid) {
        int32_t lon = g_gps.lon_e7 / 100 + 18000000, lat = g_gps.lat_e7 / 100 + 9000000;   /* grados*1e5 */
        b[0] = (char)('A' + lon / 2000000); b[1] = (char)('A' + lat / 1000000);
        b[2] = (char)('0' + (lon % 2000000) / 200000); b[3] = (char)('0' + (lat % 1000000) / 100000);
        b[4] = (char)('a' + (lon % 200000) * 24 / 200000); b[5] = (char)('a' + (lat % 100000) * 24 / 100000); b[6] = 0;
        lcd_text(8, 224, "Locator", &FONT_SMALL, UI_DIM, UI_BG);
        lcd_text(80, 224, b, &FONT_SMALL, C_YELLOW, UI_BG);
    }
}
static void gps_enter(void) { ui_title("GPS"); ui_footer("EXIT:salir  MENU:enc/apag"); gps_draw(); }
static void gps_key(key_event_t *e)
{
    if (e->ev != EV_PRESS) return;
    if (e->key == KEY_EXIT) app_goto(SCR_MAIN);
    else if (e->key == KEY_MENU) { g_set.gps_on ^= 1; gps_apply(); settings_mark_dirty(); gps_draw(); }
}
static void gps_tick(void) { static uint8_t d; if (++d >= 50) { d = 0; gps_draw(); } }
const screen_ops_t scr_gps = {gps_enter, gps_key, gps_tick, NULL};

/* ============================ APRS ============================ */
#define RXLIST 16
typedef struct { char text[96]; uint8_t hh, mm; } aprs_msg_t;
static aprs_msg_t rxlist[RXLIST];
static uint8_t rx_head, rx_count;
static volatile uint16_t pending_len;
static uint8_t pending[AX25_MAX_FRAME];
static uint32_t last_beacon_ms;
static uint8_t beacon_state;      /* 0 nada, 1 esperando TX listo, 2 transmitiendo */
static uint32_t beacon_t0;

static void on_frame(const uint8_t *f, int n)   /* desde la interrupcion TMR3 */
{
    if (pending_len || n > AX25_MAX_FRAME) return;
    memcpy(pending, f, (size_t)n);
    pending_len = (uint16_t)n;
}

static int decode_call(const uint8_t *a, char *o)
{
    int n = 0;
    for (int i = 0; i < 6; i++) { char c = (char)(a[i] >> 1); if (c != ' ') o[n++] = c; }
    int ssid = (a[6] >> 1) & 0xF;
    if (ssid) n += sprintf(o + n, "-%d", ssid);
    o[n] = 0;
    return n;
}

static void frame_to_text(const uint8_t *f, int n, char *out, int max)
{
    char call[12];
    int o = 0, i = 0;
    if (n < 16) { out[0] = 0; return; }
    decode_call(f + 7, call); o += snprintf(out + o, (size_t)(max - o), "%s>", call);
    decode_call(f, call); o += snprintf(out + o, (size_t)(max - o), "%s", call);
    i = 14;
    while (!(f[i - 1] & 1) && i + 7 <= n) {
        decode_call(f + i, call);
        o += snprintf(out + o, (size_t)(max - o), ",%s%s", call, (f[i + 6] & 0x80) ? "*" : "");
        i += 7;
    }
    i += 2;
    if (o < max - 1) out[o++] = ':';
    for (; i < n && o < max - 1; i++) out[o++] = (f[i] >= 32 && f[i] < 127) ? (char)f[i] : '.';
    out[o] = 0;
}

static void log_frame(const uint8_t *f, int n)
{
    uint8_t cnt[2], rec[0x100];
    spiflash_read(FLASH_APRS_LOG, cnt, 2);
    uint16_t c = (uint16_t)(cnt[0] << 8 | cnt[1]);
    if (c == 0xFFFF) c = 0;
    memset(rec, '~', sizeof(rec));
    rec[0] = g_gps.year; rec[1] = g_gps.mon; rec[2] = g_gps.day; rec[3] = g_gps.hh; rec[4] = g_gps.mm; rec[5] = g_gps.ss;
    memcpy(rec + 6, f, (size_t)(n > 250 ? 250 : n));
    spiflash_write(FLASH_APRS_LOG + 2 + (c % APRS_LOG_MAX) * 0x100u, rec, 0x100);
    c++;
    cnt[0] = (uint8_t)(c >> 8); cnt[1] = (uint8_t)c;
    spiflash_write(FLASH_APRS_LOG, cnt, 2);
}

void aprs_beacon_now(void)
{
    if (beacon_state) return;
    if (!strcmp(g_set.aprs_call, "NOCALL") || !g_set.aprs_call[0]) { app_toast("Configura indicativo APRS"); return; }
    if (!radio_tx_allowed(g_set.aprs_freq)) { app_toast("APRS: TX no permitido"); return; }
    beacon_state = 1;
}

static void beacon_start(void)
{
    char src[12], info[96];
    sprintf(src, g_set.aprs_ssid ? "%s-%u" : "%s", g_set.aprs_call, g_set.aprs_ssid);
    int32_t lat = g_set.aprs_lat_e7, lon = g_set.aprs_lon_e7;
    if (g_set.aprs_use_gps && g_gps.valid) { lat = g_gps.lat_e7; lon = g_gps.lon_e7; }
    aprs_position(info, lat, lon, g_set.aprs_sym_table, g_set.aprs_sym, g_set.aprs_comment);
    static uint8_t fr[AX25_MAX_FRAME];
    int n = ax25_build_ui(fr, g_set.aprs_dest, src, g_set.aprs_path, info);
    channel_t c;
    channel_default(&c, g_set.aprs_freq);
    ch_set_high_power(&c, 1);
    aprs_hw_rx_enable(0);
    radio_tune(&c);
    if (radio_tx_start()) { beacon_state = 0; app_retune(); return; }
    bk4819_set_mic_gain(16);
    delay_ms(50);
    aprs_hw_tx(fr, n);
    beacon_state = 2;
    beacon_t0 = g_ticks_ms;
}

void aprs_service(void)
{
    static uint8_t inited, rx_on;
    if (!inited) { inited = 1; aprs_hw_init(on_frame); }
    if (beacon_state == 1 && g_radio.state != RS_TX) beacon_start();
    if (beacon_state == 2 && (!aprs_hw_tx_busy() || g_ticks_ms - beacon_t0 > 5000)) {
        beacon_state = 0;
        radio_tx_stop();
        app_retune();
        last_beacon_ms = g_ticks_ms;
        if (g_screen == SCR_MAIN) app_redraw();
    }
    if (g_set.aprs_beacon_s && !beacon_state && g_ticks_ms - last_beacon_ms > g_set.aprs_beacon_s * 1000u) {
        last_beacon_ms = g_ticks_ms;
        aprs_beacon_now();
    }
    uint8_t want_rx = g_set.aprs_rx && g_radio.state == RS_RX && !beacon_state;
    if (want_rx != rx_on) { rx_on = want_rx; aprs_hw_rx_enable(want_rx); }
    if (pending_len) {
        aprs_msg_t *m = &rxlist[rx_head];
        frame_to_text(pending, pending_len, m->text, sizeof(m->text));
        m->hh = g_gps.hh; m->mm = g_gps.mm;
        log_frame(pending, pending_len);
        rx_head = (uint8_t)((rx_head + 1) % RXLIST);
        if (rx_count < RXLIST) rx_count++;
        pending_len = 0;
        g_aprs_unread = 1;
        audio_beep(1800, 60);
        if (g_screen == SCR_APRS) app_redraw();
    }
}

static int aprs_sel, aprs_top;
static const char *aprs_item(int i, char *b)
{
    aprs_msg_t *m = &rxlist[(rx_head + RXLIST - 1 - i) % RXLIST];
    strncpy(b, m->text, 28); b[28] = 0;
    return b;
}
static void aprs_enter(void)
{
    g_aprs_unread = 0;
    ui_title("APRS recibidos");
    ui_footer("MENU:detalle  #:baliza  EXIT");
    if (!rx_count) lcd_text(8, 60, "Sin paquetes", &FONT_SMALL, UI_DIM, UI_BG);
    else ui_list(rx_count, aprs_sel, &aprs_top, aprs_item, NULL);
}
static void aprs_key(key_event_t *e)
{
    if (e->ev != EV_PRESS && e->ev != EV_REPEAT) return;
    if (e->key == KEY_EXIT) { app_goto(SCR_MAIN); return; }
    if (e->key == KEY_HASH) { aprs_beacon_now(); return; }
    if (!rx_count) return;
    if (e->key == KEY_UP && aprs_sel > 0) aprs_sel--;
    if (e->key == KEY_DOWN && aprs_sel < rx_count - 1) aprs_sel++;
    if (e->key == KEY_MENU) {
        aprs_msg_t *m = &rxlist[(rx_head + RXLIST - 1 - aprs_sel) % RXLIST];
        ui_clear_body(); ui_title("Detalle APRS");
        int len = (int)strlen(m->text);
        for (int l = 0; l * 28 < len && l < 10; l++) {
            char line[29]; strncpy(line, m->text + l * 28, 28); line[28] = 0;
            lcd_text(4, 50 + l * 20, line, &FONT_SMALL, UI_FG, UI_BG);
        }
        return;
    }
    ui_list(rx_count, aprs_sel, &aprs_top, aprs_item, NULL);
}
const screen_ops_t scr_aprs = {aprs_enter, aprs_key, NULL, NULL};

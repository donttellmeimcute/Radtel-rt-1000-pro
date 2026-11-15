/* Nucleo de la aplicacion: bucle principal, gestion de pantallas, PTT/TX,
 * temporizadores (retroiluminacion, TOT, bloqueo) y pantalla principal. */
#include "app.h"
#include "ui.h"
#include "radio.h"
#include "settings.h"
#include "board.h"
#include "system.h"
#include "audio.h"
#include "bk4819.h"
#include "cps.h"
#include "uart.h"
#include "adc.h"
#include "aprs_hw.h"
#include <stdio.h>
#include <string.h>

screen_t g_screen;
static const screen_ops_t *ops;
static uint32_t last_key_ms, last_tick_ms, last_status_ms, last_poll_ms, last_batt_ms;
static uint8_t backlight_on = 1;
static char toast[32];
static uint32_t toast_until;
static uint8_t tx_via_ptt;

static const screen_ops_t *const screens[] = {
    [SCR_MAIN] = &scr_main, [SCR_MENU] = &scr_menu, [SCR_BCAST] = &scr_bcast,
    [SCR_APRS] = &scr_aprs, [SCR_GPS] = &scr_gps, [SCR_DTMF] = &scr_dtmf,
    [SCR_TONESCAN] = &scr_tonescan, [SCR_FCOUNT] = &scr_fcount, [SCR_SPECTRUM] = &scr_spectrum,
    [SCR_CHLIST] = &scr_chlist, [SCR_ABOUT] = &scr_about,
};

void app_toast(const char *m)
{
    strncpy(toast, m, sizeof(toast) - 1);
    toast_until = g_ticks_ms + 1500;
    ui_message(toast, NULL, UI_WARN);
}

void app_goto(screen_t s)
{
    if (ops && ops->leave) ops->leave();
    g_screen = s;
    ops = screens[s];
    lcd_clear(UI_BG);
    ui_status_bar(1);
    if (ops && ops->enter) ops->enter();
}

void app_redraw(void) { app_goto(g_screen); }

static const uint8_t bl_levels[5] = {0x3C, 0x41, 0x46, 0x4C, 0xFF};

void app_backlight_kick(void)
{
    last_key_ms = g_ticks_ms;
    if (!backlight_on) {
        backlight_on = 1;
        lcd_backlight(bl_levels[g_set.brightness % 5]);
    }
}

void app_area_channel(int a, channel_t *out)
{
    area_t *ar = &g_set.area[a];
    if (ar->mode == AREA_MR) {
        if (!channel_load(ar->ch, out)) {
            int n = channel_next_valid(ar->ch, 1);
            if (n >= 0) { ar->ch = (uint16_t)n; channel_load(ar->ch, out); }
            else { ar->mode = AREA_VFO; *out = ar->vfo; }
        }
    } else if (ar->mode == AREA_ZONE) {
        int c = zone_channel(ar->zone, ar->zone_pos);
        if (c < 0 || !channel_load((uint16_t)c, out)) { ar->mode = AREA_VFO; *out = ar->vfo; }
    } else {
        *out = ar->vfo;
    }
}

void app_retune(void)
{
    channel_t c;
    app_area_channel(g_set.active, &c);
    radio_tune(&c);
}

/* ------------------------------------------------------------------ */
/* PTT / TX                                                           */
/* ------------------------------------------------------------------ */
static void tx_begin(void)
{
    if (g_screen == SCR_BCAST) return;
    int r = radio_tx_start();
    if (r == -1) { app_toast("TX no permitido"); audio_beep(400, 200); return; }
    if (r == -2) { app_toast("Canal ocupado"); audio_beep(400, 200); return; }
    if (r == -3) { app_toast("Radio bloqueado"); return; }
    tx_via_ptt = 1;
    if (g_set.dtmf_ptt_id & 1) dtmf_send_string(g_set.dtmf_id);
    if (g_screen == SCR_MAIN) scr_main.enter();
}

static void tx_end(void)
{
    if (!tx_via_ptt) return;
    tx_via_ptt = 0;
    if (g_set.dtmf_ptt_id & 2) dtmf_send_string(g_set.dtmf_id);
    radio_tx_stop();
    if (g_screen == SCR_MAIN) scr_main.enter();
}

/* ------------------------------------------------------------------ */
void app_side_function(uint8_t fn)
{
    switch (fn) {
    case SK_MONITOR: g_radio.monitor ^= 1; app_toast(g_radio.monitor ? "Monitor ON" : "Monitor OFF"); break;
    case SK_SCAN: if (g_screen == SCR_MAIN) { extern void main_scan_toggle(void); main_scan_toggle(); } break;
    case SK_BCAST: app_goto(g_screen == SCR_BCAST ? SCR_MAIN : SCR_BCAST); break;
    case SK_APRS_BEACON: aprs_beacon_now(); break;
    case SK_TONE_SCAN: app_goto(SCR_TONESCAN); break;
    case SK_ALARM: { extern void alarm_toggle(void); alarm_toggle(); } break;
    case SK_POWER: {
        channel_t *v = &g_set.area[g_set.active].vfo;
        ch_set_high_power(v, !ch_high_power(v)); settings_mark_dirty(); app_retune();
        app_toast(ch_high_power(v) ? "Potencia ALTA" : "Potencia BAJA");
    } break;
    case SK_VFO_MR: {
        area_t *a = &g_set.area[g_set.active];
        a->mode = (uint8_t)((a->mode + 1) % 3); settings_mark_dirty(); app_retune(); app_redraw();
    } break;
    case SK_DUAL: g_set.dual_watch ^= 1; settings_mark_dirty(); ui_status_bar(1); break;
    case SK_LIGHT: g_set.brightness = (uint8_t)((g_set.brightness + 1) % 5); lcd_backlight(bl_levels[g_set.brightness]); settings_mark_dirty(); break;
    case SK_GPS: app_goto(SCR_GPS); break;
    default: break;
    }
}

static void handle_global_key(key_event_t *e)
{
    if (e->key == KEY_PTT) {
        if (e->ev == EV_PRESS) tx_begin();
        else if (e->ev == EV_RELEASE) tx_end();
        return;
    }
    uint8_t fn = SK_NONE;
    if (e->key == KEY_SIDE1) fn = e->ev == EV_LONG ? g_set.side1_long : (e->ev == EV_RELEASE ? 0xFE : SK_NONE);
    else if (e->key == KEY_SIDE2) fn = e->ev == EV_LONG ? g_set.side2_long : (e->ev == EV_RELEASE ? 0xFE : SK_NONE);
    else if (e->key == KEY_ALARM && e->ev == EV_PRESS) fn = g_set.alarm_key;
    /* pulsacion corta lateral: al soltar si no hubo pulsacion larga */
    static uint8_t long_seen;
    if ((e->key == KEY_SIDE1 || e->key == KEY_SIDE2)) {
        if (e->ev == EV_PRESS) long_seen = 0;
        if (e->ev == EV_LONG) long_seen = 1;
        if (fn == 0xFE) fn = long_seen ? SK_NONE : (e->key == KEY_SIDE1 ? g_set.side1_short : g_set.side2_short);
    }
    if (fn != SK_NONE && fn != 0xFE) { app_side_function(fn); return; }
    if (e->key == KEY_SIDE1 || e->key == KEY_SIDE2 || e->key == KEY_ALARM) return;

    if (g_set.key_lock && !(e->key == KEY_HASH && e->ev == EV_LONG)) {
        if (e->ev == EV_PRESS) app_toast("Teclado bloqueado (# largo)");
        return;
    }
    if (e->ev == EV_PRESS && g_set.key_beep) audio_beep(1000, 30);
    if (ops && ops->key) ops->key(e);
}

static void power_check(void)
{
    uint16_t mv = battery_mv();
    static uint8_t low_count;
    if (mv < 5900 && mv > 1000) {
        if (++low_count > 5) {
            lcd_clear(C_BLACK);
            lcd_backlight(0xFF);
            lcd_text_center(150, "Bateria baja", &FONT_MEDIUM, C_RED, C_BLACK);
            settings_save();
            delay_ms(2000);
            gpio_clr(PIN_POWER_HOLD);
            for (;;) { }
        }
    } else low_count = 0;
}

void app_init(void)
{
    ops = NULL;
    lcd_backlight(bl_levels[g_set.brightness % 5]);
    last_key_ms = g_ticks_ms;
    if (g_set.killed) { g_screen = SCR_KILLED; lcd_clear(C_BLACK); lcd_text_center(150, "BLOQUEADO", &FONT_MEDIUM, C_RED, C_BLACK); return; }
    app_retune();
    app_goto(SCR_MAIN);
}

void app_loop(void)
{
    key_event_t e;
    /* Sesion de programacion por PC */
    if (cps_poll()) {
        if (g_screen != SCR_PC) {
            radio_sleep();
            g_screen = SCR_PC; ops = NULL;
            lcd_clear(UI_BG);
            ui_message("PC Programando", "No apagar el radio", UI_ACCENT);
        }
        return;
    }
    if (cps_session_ended()) {
        settings_load();
        app_retune();
        app_goto(SCR_MAIN);
    }
    if (g_screen == SCR_KILLED) {
        dtmf_service();
        if (!g_set.killed) app_init();
        return;
    }

    while (keys_get(&e)) {
        app_backlight_kick();
        handle_global_key(&e);
    }
    int enc = encoder_take();
    if (enc) {
        app_backlight_kick();
        key_event_t ke = {enc > 0 ? KEY_UP : KEY_DOWN, EV_PRESS};
        for (int i = 0; i < (enc > 0 ? enc : -enc); i++) if (ops && ops->key && !g_set.key_lock) ops->key(&ke);
    }

    uint32_t now = g_ticks_ms;
    if (now - last_poll_ms >= 10) {
        last_poll_ms = now;
        if (g_screen != SCR_BCAST && g_screen != SCR_SPECTRUM && g_screen != SCR_FCOUNT) radio_poll();
    }
    if (now - last_tick_ms >= 20) {
        last_tick_ms = now;
        if (ops && ops->tick) ops->tick();
    }
    if (now - last_status_ms >= 1000) {
        last_status_ms = now;
        ui_status_bar(0);
    }
    if (now - last_batt_ms >= 5000) { last_batt_ms = now; power_check(); }
    if (toast_until && (int32_t)(now - toast_until) > 0) { toast_until = 0; app_redraw(); }

    /* Tiempo maximo de TX */
    if (g_radio.state == RS_TX && g_set.tot_s && now - g_radio.tx_start_ms > g_set.tot_s * 10000u) {
        tx_via_ptt = 1; tx_end();
        app_toast("Tiempo TX agotado");
    }
    /* Retroiluminacion */
    if (backlight_on && g_set.backlight_s && g_radio.state != RS_TX &&
        now - last_key_ms > g_set.backlight_s * 1000u && !g_radio.sq_open) {
        backlight_on = 0;
        lcd_backlight(0);
    }
    if (!backlight_on && g_radio.sq_open && g_radio.state == RS_RX) app_backlight_kick();
    /* Bloqueo automatico */
    if (g_set.auto_lock_s && !g_set.key_lock && now - last_key_ms > g_set.auto_lock_s * 1000u) {
        g_set.key_lock = 1; ui_status_bar(1);
    }
    audio_voice_busy();
    aprs_service();
    gps_service();
    dtmf_service();
    settings_service();
}

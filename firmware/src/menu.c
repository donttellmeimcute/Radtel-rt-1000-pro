/* Menu de ajustes generico: listas, numeros, interruptores, textos y acciones. */
#include "app.h"
#include "ui.h"
#include "radio.h"
#include "settings.h"
#include "tones.h"
#include "system.h"
#include "audio.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>

typedef enum { T_LIST, T_NUM, T_BOOL, T_TEXT, T_ACTION, T_TONE, T_FREQ, T_OFFSET } itype_t;
typedef struct {
    const char *name;
    itype_t type;
    void *ptr;                  /* uint8_t* (LIST/NUM/BOOL), char* (TEXT), uint16_t* (TONE), uint32_t* (FREQ) */
    int16_t min, max;           /* NUM; LIST usa max = cantidad-1; TEXT usa max = longitud */
    const char *const *labels;  /* LIST */
    void (*action)(void);       /* ACTION o callback tras cambio */
    uint8_t vfo_field;          /* 1 = campo del VFO activo (bits de channel_t) */
} item_t;

static const char *const L_OFFON[] = {"OFF", "ON"};
static const char *const L_STEP[] = {"0.25k", "0.5k", "1k", "1.25k", "2.5k", "5k", "6.25k", "8.33k", "10k", "12.5k", "20k", "25k", "50k", "100k"};
static const char *const L_MOD[] = {"FM", "AM", "USB", "LSB"};
static const char *const L_BW[] = {"Ancho", "Estrecho"};
static const char *const L_PWR[] = {"Baja", "Alta"};
static const char *const L_SCAN[] = {"Tiempo", "Portadora", "Parar"};
static const char *const L_SK[] = {"Nada", "Monitor", "Escanear", "Radio AM/FM", "Baliza APRS", "Busca tono",
                                   "Alarma", "Potencia", "VFO/MR", "Doble esc.", "Brillo", "GPS"};
static const char *const L_ALARM[] = {"Local", "TX", "Ambos"};
static const char *const L_PTTID[] = {"OFF", "Inicio", "Final", "Ambos"};
static const char *const L_BAUD[] = {"4800", "9600", "14400", "19200", "38400", "56000", "57600", "115200", "128000", "256000"};
static const char *const L_STE[] = {"OFF", "55Hz", "120g", "180g", "240g"};
static const char *const L_UNITS[] = {"Metrico", "Imperial"};

/* Campos temporales para editar bits del VFO activo */
static uint8_t v_mod, v_bw, v_pwr, v_scr, v_ste;
static int32_t v_offset;

static void vfo_pull(void)
{
    channel_t *c = &g_set.area[g_set.active].vfo;
    v_mod = (uint8_t)ch_mod(c); v_bw = (uint8_t)ch_narrow(c); v_pwr = (uint8_t)ch_high_power(c);
    v_scr = (uint8_t)ch_scrambler(c); v_ste = (uint8_t)ch_ste(c);
    v_offset = (int32_t)c->tx - (int32_t)c->rx;
}
static void vfo_push(void)
{
    channel_t *c = &g_set.area[g_set.active].vfo;
    ch_set_mod(c, v_mod); ch_set_narrow(c, v_bw); ch_set_high_power(c, v_pwr);
    ch_set_scrambler(c, v_scr);
    c->b3 = (uint8_t)((c->b3 & ~7) | (v_ste & 7));
    c->tx = (uint32_t)((int32_t)c->rx + v_offset);
    app_retune();
}

static void act_save_channel(void);
static void act_delete_channel(void);
static void act_reset(void);
static void act_aprs_beacon(void) { aprs_beacon_now(); }
static void apply_brightness(void) { app_backlight_kick(); extern void app_bl_apply(void); app_bl_apply(); }
static void apply_radio(void) { app_retune(); }
static void goto_bcast(void) { app_goto(SCR_BCAST); }
static void goto_aprs(void) { app_goto(SCR_APRS); }
static void goto_gps(void) { app_goto(SCR_GPS); }
static void goto_dtmf(void) { app_goto(SCR_DTMF); }
static void goto_tscan(void) { app_goto(SCR_TONESCAN); }
static void goto_fcount(void) { app_goto(SCR_FCOUNT); }
static void goto_spec(void) { app_goto(SCR_SPECTRUM); }
static void goto_chlist(void) { app_goto(SCR_CHLIST); }
static void goto_about(void) { app_goto(SCR_ABOUT); }
static void apply_gps(void) { extern void gps_apply(void); gps_apply(); }


static item_t items[] = {
    {"Radio AM/FM/SW/SSB", T_ACTION, 0, 0, 0, 0, goto_bcast, 0},
    {"Lista de canales", T_ACTION, 0, 0, 0, 0, goto_chlist, 0},
    {"APRS", T_ACTION, 0, 0, 0, 0, goto_aprs, 0},
    {"GPS", T_ACTION, 0, 0, 0, 0, goto_gps, 0},
    {"DTMF enviar", T_ACTION, 0, 0, 0, 0, goto_dtmf, 0},
    {"Busca CTCSS/DCS", T_ACTION, 0, 0, 0, 0, goto_tscan, 0},
    {"Frecuencimetro", T_ACTION, 0, 0, 0, 0, goto_fcount, 0},
    {"Espectro", T_ACTION, 0, 0, 0, 0, goto_spec, 0},
    /* VFO activo */
    {"Modulacion", T_LIST, &v_mod, 0, 3, L_MOD, vfo_push, 1},
    {"Ancho de banda", T_LIST, &v_bw, 0, 1, L_BW, vfo_push, 1},
    {"Potencia", T_LIST, &v_pwr, 0, 1, L_PWR, vfo_push, 1},
    {"Tono RX", T_TONE, 0, 0, 0, 0, apply_radio, 2},
    {"Tono TX", T_TONE, 0, 0, 0, 0, apply_radio, 3},
    {"Desplazamiento", T_OFFSET, &v_offset, 0, 0, 0, vfo_push, 1},
    {"Scrambler", T_NUM, &v_scr, 0, 15, 0, vfo_push, 1},
    {"Cola de tono STE", T_LIST, &v_ste, 0, 4, L_STE, vfo_push, 1},
    {"Paso", T_LIST, &g_set.step_idx, 0, 13, L_STEP, 0, 0},
    {"Guardar en canal", T_ACTION, 0, 0, 0, 0, act_save_channel, 0},
    {"Borrar canal", T_ACTION, 0, 0, 0, 0, act_delete_channel, 0},
    /* radio */
    {"Squelch", T_NUM, &g_set.squelch, 0, 10, 0, apply_radio, 0},
    {"Umbral glitch", T_NUM, &g_set.glitch, 0, 10, 0, apply_radio, 0},
    {"Volumen", T_NUM, &g_set.volume, 0, 15, 0, apply_radio, 0},
    {"Ganancia MIC", T_NUM, &g_set.mic_gain, 0, 31, 0, 0, 0},
    {"VOX", T_NUM, &g_set.vox, 0, 9, 0, apply_radio, 0},
    {"Doble escucha", T_BOOL, &g_set.dual_watch, 0, 1, L_OFFON, 0, 0},
    {"TOT (x10 s)", T_NUM, &g_set.tot_s, 0, 60, 0, 0, 0},
    {"Pitido fin TX", T_BOOL, &g_set.roger, 0, 1, L_OFFON, 0, 0},
    {"Modo escaneo", T_LIST, &g_set.scan_mode, 0, 2, L_SCAN, 0, 0},
    {"Escan. inicio", T_FREQ, &g_set.scan_lo, 0, 0, 0, 0, 0},
    {"Escan. fin", T_FREQ, &g_set.scan_hi, 0, 0, 0, 0, 0},
    {"TX banda aerea", T_BOOL, &g_set.air_tx, 0, 1, L_OFFON, 0, 0},
    {"TX en todo rango", T_BOOL, &g_set.allow_tx_all, 0, 1, L_OFFON, 0, 0},
    /* pantalla y teclas */
    {"Brillo", T_NUM, &g_set.brightness, 0, 4, 0, apply_brightness, 0},
    {"Luz apagado (s)", T_NUM, &g_set.backlight_s, 0, 120, 0, 0, 0},
    {"Pitido teclas", T_BOOL, &g_set.key_beep, 0, 1, L_OFFON, 0, 0},
    {"Voz", T_BOOL, &g_set.voice, 0, 1, L_OFFON, 0, 0},
    {"Auto bloqueo (s)", T_NUM, &g_set.auto_lock_s, 0, 120, 0, 0, 0},
    {"LED portadora", T_BOOL, &g_set.carrier_led, 0, 1, L_OFFON, 0, 0},
    {"Lateral 1 corto", T_LIST, &g_set.side1_short, 0, SK_COUNT - 1, L_SK, 0, 0},
    {"Lateral 1 largo", T_LIST, &g_set.side1_long, 0, SK_COUNT - 1, L_SK, 0, 0},
    {"Lateral 2 corto", T_LIST, &g_set.side2_short, 0, SK_COUNT - 1, L_SK, 0, 0},
    {"Lateral 2 largo", T_LIST, &g_set.side2_long, 0, SK_COUNT - 1, L_SK, 0, 0},
    {"Tecla alarma", T_LIST, &g_set.alarm_key, 0, SK_COUNT - 1, L_SK, 0, 0},
    {"Tipo de alarma", T_LIST, &g_set.alarm_type, 0, 2, L_ALARM, 0, 0},
    {"Logo de inicio", T_BOOL, &g_set.boot_logo, 0, 1, L_OFFON, 0, 0},
    {"Texto bienvenida", T_TEXT, g_set.welcome, 0, 16, 0, 0, 0},
    /* DTMF */
    {"DTMF ID", T_TEXT, g_set.dtmf_id, 0, 8, 0, 0, 0},
    {"PTT ID", T_LIST, &g_set.dtmf_ptt_id, 0, 3, L_PTTID, 0, 0},
    {"Kill/Stun remoto", T_BOOL, &g_set.dtmf_remote, 0, 1, L_OFFON, 0, 0},
    {"Codigo kill", T_TEXT, g_set.dtmf_kill, 0, 8, 0, 0, 0},
    {"Codigo stun", T_TEXT, g_set.dtmf_stun, 0, 8, 0, 0, 0},
    {"Codigo wake", T_TEXT, g_set.dtmf_wake, 0, 8, 0, 0, 0},
    /* APRS */
    {"APRS indicativo", T_TEXT, g_set.aprs_call, 0, 6, 0, 0, 0},
    {"APRS SSID", T_NUM, &g_set.aprs_ssid, 0, 15, 0, 0, 0},
    {"APRS ruta", T_TEXT, g_set.aprs_path, 0, 23, 0, 0, 0},
    {"APRS comentario", T_TEXT, g_set.aprs_comment, 0, 39, 0, 0, 0},
    {"APRS frecuencia", T_FREQ, &g_set.aprs_freq, 0, 0, 0, 0, 0},
    {"APRS baliza (min)", T_NUM, 0, 0, 120, 0, 0, 4},
    {"APRS recepcion", T_BOOL, &g_set.aprs_rx, 0, 1, L_OFFON, 0, 0},
    {"APRS usar GPS", T_BOOL, &g_set.aprs_use_gps, 0, 1, L_OFFON, 0, 0},
    {"Enviar baliza", T_ACTION, 0, 0, 0, 0, act_aprs_beacon, 0},
    /* GPS */
    {"GPS", T_BOOL, &g_set.gps_on, 0, 1, L_OFFON, apply_gps, 0},
    {"GPS baudios", T_LIST, &g_set.gps_baud_idx, 0, 9, L_BAUD, apply_gps, 0},
    {"Zona horaria (h)", T_NUM, 0, -12, 14, 0, 0, 5},
    {"Registrar traza", T_BOOL, &g_set.gps_track, 0, 1, L_OFFON, 0, 0},
    {"Intervalo traza(s)", T_NUM, 0, 5, 600, 0, 0, 6},
    {"Unidades", T_LIST, &g_set.units_imperial, 0, 1, L_UNITS, 0, 0},
    /* sistema */
    {"Acerca de", T_ACTION, 0, 0, 0, 0, goto_about, 0},
    {"Restablecer ajustes", T_ACTION, 0, 0, 0, 0, act_reset, 0},
};
#define N_ITEMS ((int)(sizeof(items) / sizeof(items[0])))

static int sel, top, editing;
static int32_t edit_val;
static char edit_text[41];
static uint8_t edit_pos;
static uint16_t edit_tone;
static char num_buf[10]; static uint8_t num_len;

static int32_t get_num(const item_t *it)
{
    switch (it->vfo_field) {
    case 4: return g_set.aprs_beacon_s / 60;
    case 5: return g_set.tz_quarter_h / 4;
    case 6: return g_set.gps_track_s;
    }
    return *(uint8_t *)it->ptr;
}
static void set_num(const item_t *it, int32_t v)
{
    switch (it->vfo_field) {
    case 4: g_set.aprs_beacon_s = (uint16_t)(v * 60); return;
    case 5: g_set.tz_quarter_h = (int8_t)(v * 4); return;
    case 6: g_set.gps_track_s = (uint16_t)v; return;
    }
    *(uint8_t *)it->ptr = (uint8_t)v;
}

static uint16_t get_tone(const item_t *it)
{
    channel_t *v = &g_set.area[g_set.active].vfo;
    return it->vfo_field == 2 ? v->rx_tone : v->tx_tone;
}
static void set_tone(const item_t *it, uint16_t t)
{
    channel_t *v = &g_set.area[g_set.active].vfo;
    if (it->vfo_field == 2) v->rx_tone = t; else v->tx_tone = t;
}

static const char *item_name(int i, char *b) { (void)b; return items[i].name; }

static const char *item_value(int i, char *b)
{
    const item_t *it = &items[i];
    int ed = editing && i == sel;
    switch (it->type) {
    case T_LIST: case T_BOOL: return it->labels[ed ? edit_val : *(uint8_t *)it->ptr];
    case T_NUM: sprintf(b, ed ? "<%ld>" : "%ld", (long)(ed ? edit_val : get_num(it))); return b;
    case T_TEXT:
        if (ed) { sprintf(b, "%s_", edit_text); return b + (strlen(b) > 14 ? strlen(b) - 14 : 0); }
        strncpy(b, (const char *)it->ptr, 14); b[14] = 0; return b;
    case T_TONE: fmt_tone(b, ed ? edit_tone : get_tone(it)); return b;
    case T_FREQ:
        if (ed) { sprintf(b, "%s_", num_buf); return b; }
        fmt_freq(b, *(uint32_t *)it->ptr); return b;
    case T_OFFSET:
        if (ed) { sprintf(b, "%c%s_", edit_val < 0 ? '-' : '+', num_buf); return b; }
        { int32_t o = *(int32_t *)it->ptr; sprintf(b, "%c%ld.%04ld", o < 0 ? '-' : '+', (long)((o < 0 ? -o : o) / 100000), (long)(((o < 0 ? -o : o) / 10) % 10000)); }
        return b;
    case T_ACTION: return ">";
    }
    return NULL;
}

static void redraw(void) { ui_list(N_ITEMS, sel, &top, item_name, item_value); }

static void mn_enter(void)
{
    vfo_pull();
    editing = 0;
    ui_title("Menu");
    ui_footer("MENU:editar  EXIT:salir");
    redraw();
}

/* Multipulsacion para textos */
static const char *const tap[10] = {" 0-/", "1.,?!", "ABC2", "DEF3", "GHI4", "JKL5", "MNO6", "PQRS7", "TUV8", "WXYZ9"};
static uint8_t last_tap_key = 0xFF, tap_idx;
static uint32_t last_tap_ms;

static void tone_step(int dir)
{
    int ty = tone_type(edit_tone);
    uint16_t c = tone_code(edit_tone);
    int idx = -1;
    if (ty == 1) for (int i = 0; i < CTCSS_COUNT; i++) if (ctcss_tones[i] == c) idx = i;
    if (ty >= 2) for (int i = 0; i < DCS_COUNT; i++) if (dcs_codes[i] == c) idx = CTCSS_COUNT + i + (ty == 3 ? DCS_COUNT : 0);
    int total = CTCSS_COUNT + 2 * DCS_COUNT + 1;
    int pos = ty == 0 ? 0 : idx + 1;
    pos = (pos + dir + total) % total;
    if (pos == 0) edit_tone = 0;
    else if (pos <= CTCSS_COUNT) edit_tone = tone_make(1, ctcss_tones[pos - 1]);
    else if (pos <= CTCSS_COUNT + DCS_COUNT) edit_tone = tone_make(2, dcs_codes[pos - 1 - CTCSS_COUNT]);
    else edit_tone = tone_make(3, dcs_codes[pos - 1 - CTCSS_COUNT - DCS_COUNT]);
}

static uint32_t parse_freq(void)
{
    uint32_t f = 0;
    for (int i = 0; i < 8; i++) f = f * 10 + (i < num_len ? (uint32_t)(num_buf[i] - '0') : 0);
    return f;
}

static void finish_edit(int save)
{
    const item_t *it = &items[sel];
    if (save) {
        switch (it->type) {
        case T_LIST: case T_BOOL: *(uint8_t *)it->ptr = (uint8_t)edit_val; break;
        case T_NUM: set_num(it, edit_val); break;
        case T_TEXT: strcpy((char *)it->ptr, edit_text); break;
        case T_TONE: set_tone(it, edit_tone); break;
        case T_FREQ: if (num_len) { uint32_t f = parse_freq(); if (radio_band_of(f) >= 0) *(uint32_t *)it->ptr = f; } break;
        case T_OFFSET: {
            uint32_t f = 0;
            for (int i = 0; i < 7; i++) f = f * 10 + (i < num_len ? (uint32_t)(num_buf[i] - '0') : 0);
            v_offset = edit_val < 0 ? -(int32_t)(f / 10) : (int32_t)(f / 10);   /* MMM.kkkk -> 10 Hz */
        } break;
        default: break;
        }
        if (it->action) it->action();
        settings_mark_dirty();
    }
    editing = 0;
    num_len = 0;
    redraw();
}

static void mn_key(key_event_t *e)
{
    if (e->ev != EV_PRESS && e->ev != EV_REPEAT) return;
    const item_t *it = &items[sel];
    if (!editing) {
        if (e->key == KEY_UP) { sel = (sel + N_ITEMS - 1) % N_ITEMS; redraw(); }
        else if (e->key == KEY_DOWN) { sel = (sel + 1) % N_ITEMS; redraw(); }
        else if (e->key == KEY_EXIT) { app_goto(SCR_MAIN); }
        else if (e->key == KEY_MENU) {
            if (it->type == T_ACTION) { it->action(); return; }
            editing = 1;
            if (it->type == T_LIST || it->type == T_BOOL) edit_val = *(uint8_t *)it->ptr;
            else if (it->type == T_NUM) edit_val = get_num(it);
            else if (it->type == T_TEXT) { strcpy(edit_text, (char *)it->ptr); edit_pos = (uint8_t)strlen(edit_text); last_tap_key = 0xFF; }
            else if (it->type == T_TONE) edit_tone = get_tone(it);
            else if (it->type == T_OFFSET) edit_val = v_offset < 0 ? -1 : 1;
            num_len = 0; num_buf[0] = 0;
            redraw();
        } else if (e->key <= KEY_9) {   /* salto rapido por numero de item */
            sel = (e->key * 10) % N_ITEMS; redraw();
        }
        return;
    }
    /* edicion */
    if (e->key == KEY_EXIT) {
        if (it->type == T_TEXT && edit_pos) { edit_text[--edit_pos] = 0; last_tap_key = 0xFF; redraw(); }
        else if ((it->type == T_FREQ || it->type == T_OFFSET) && num_len) { num_buf[--num_len] = 0; redraw(); }
        else finish_edit(0);
        return;
    }
    if (e->key == KEY_MENU) { finish_edit(1); return; }
    switch (it->type) {
    case T_LIST: case T_BOOL:
        if (e->key == KEY_UP) edit_val = edit_val >= it->max ? 0 : edit_val + 1;
        if (e->key == KEY_DOWN) edit_val = edit_val <= 0 ? it->max : edit_val - 1;
        break;
    case T_NUM:
        if (e->key == KEY_UP && edit_val < it->max) edit_val++;
        if (e->key == KEY_DOWN && edit_val > it->min) edit_val--;
        if (e->key <= KEY_9) { edit_val = edit_val * 10 + e->key; if (edit_val > it->max) edit_val = e->key; }
        break;
    case T_TONE:
        if (e->key == KEY_UP) tone_step(1);
        if (e->key == KEY_DOWN) tone_step(-1);
        if (e->key == KEY_0) edit_tone = 0;
        break;
    case T_FREQ: case T_OFFSET:
        if (e->key <= KEY_9 && num_len < 8) { num_buf[num_len++] = (char)('0' + e->key); num_buf[num_len] = 0; }
        if (it->type == T_OFFSET && (e->key == KEY_STAR || e->key == KEY_UP || e->key == KEY_DOWN)) edit_val = -edit_val;
        break;
    case T_TEXT:
        if (e->key <= KEY_9) {
            uint32_t now = g_ticks_ms;
            const char *set = tap[e->key];
            if (e->key == last_tap_key && now - last_tap_ms < 1000 && edit_pos) {
                tap_idx = (uint8_t)((tap_idx + 1) % strlen(set));
                edit_text[edit_pos - 1] = set[tap_idx];
            } else if (edit_pos < it->max) {
                tap_idx = 0;
                edit_text[edit_pos++] = set[0];
                edit_text[edit_pos] = 0;
            }
            last_tap_key = e->key; last_tap_ms = now;
        } else if (e->key == KEY_STAR && edit_pos < it->max) { edit_text[edit_pos++] = '-'; edit_text[edit_pos] = 0; last_tap_key = 0xFF; }
        else if (e->key == KEY_HASH && edit_pos < it->max) { edit_text[edit_pos++] = ','; edit_text[edit_pos] = 0; last_tap_key = 0xFF; }
        break;
    default: break;
    }
    redraw();
}

static void act_save_channel(void)
{
    /* Guarda el VFO activo en el primer canal libre */
    channel_t c;
    for (int i = 0; i < CHANNEL_COUNT; i++) {
        if (!channel_load((uint16_t)i, &c)) {
            channel_save((uint16_t)i, &g_set.area[g_set.active].vfo);
            char b[24]; sprintf(b, "Guardado CH-%03d", i + 1);
            app_toast(b);
            return;
        }
    }
    app_toast("Memoria llena");
}

static void act_delete_channel(void)
{
    area_t *a = &g_set.area[g_set.active];
    if (a->mode != AREA_MR) { app_toast("Solo en modo MR"); return; }
    channel_erase(a->ch);
    app_toast("Canal borrado");
}

static void act_reset(void)
{
    settings_defaults();
    settings_save();
    app_retune();
    app_toast("Ajustes restablecidos");
}

const screen_ops_t scr_menu = {mn_enter, mn_key, NULL, NULL};

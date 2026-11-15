#pragma once
#include <stdint.h>
#include "keys.h"
#include "channel.h"
#include "nmea.h"

typedef enum {
    SCR_MAIN, SCR_MENU, SCR_BCAST, SCR_APRS, SCR_GPS, SCR_DTMF, SCR_TONESCAN, SCR_FCOUNT,
    SCR_SPECTRUM, SCR_CHLIST, SCR_PC, SCR_KILLED, SCR_ABOUT,
} screen_t;

typedef struct {
    void (*enter)(void);
    void (*key)(key_event_t *e);
    void (*tick)(void);          /* cada ~20 ms */
    void (*leave)(void);
} screen_ops_t;

extern screen_t g_screen;
extern gps_fix_t g_gps;
extern uint8_t g_aprs_unread;

void app_init(void);
void app_loop(void);
void app_goto(screen_t s);
void app_redraw(void);
void app_area_channel(int area, channel_t *out);   /* canal efectivo del area */
void app_retune(void);
void app_backlight_kick(void);
void app_side_function(uint8_t fn);
void app_toast(const char *msg);

/* Pantallas (cada modulo exporta sus operaciones) */
extern const screen_ops_t scr_main, scr_menu, scr_bcast, scr_aprs, scr_gps, scr_dtmf,
    scr_tonescan, scr_fcount, scr_spectrum, scr_chlist, scr_about;

/* Servicios en segundo plano */
void aprs_service(void);
void aprs_beacon_now(void);
void gps_service(void);
void dtmf_service(void);
int  dtmf_send_string(const char *s);

#pragma once
#include <stdint.h>
#include <stddef.h>
#include "channel.h"

enum { AREA_VFO = 0, AREA_MR = 1, AREA_ZONE = 2 };
enum { SCAN_TO = 0, SCAN_CO = 1, SCAN_SE = 2 };
enum { SK_NONE, SK_MONITOR, SK_SCAN, SK_BCAST, SK_APRS_BEACON, SK_TONE_SCAN, SK_ALARM, SK_POWER,
       SK_VFO_MR, SK_DUAL, SK_LIGHT, SK_GPS, SK_COUNT };

typedef struct {
    uint8_t  mode;               /* AREA_* */
    uint16_t ch;                 /* canal en modo MR */
    uint8_t  zone, zone_pos;
    channel_t vfo;               /* parametros del VFO */
} area_t;

typedef struct {
    uint32_t magic;
    uint16_t version, size;
    /* --- radio --- */
    area_t   area[2];
    uint8_t  active;             /* area activa 0/1 */
    uint8_t  dual_watch;
    uint8_t  squelch;            /* 0..10 */
    uint8_t  glitch;             /* 0..10 */
    uint8_t  volume;             /* 0..15 */
    uint8_t  mic_gain;           /* 0..31 */
    uint8_t  step_idx;
    uint8_t  tot_s;              /* tiempo max de TX en s/10 (0 = sin limite) */
    uint8_t  vox;                /* 0 = apagado, 1..9 */
    uint8_t  roger;              /* pitido de fin de TX */
    uint8_t  scan_mode;
    uint8_t  scan_resume_s;
    uint32_t scan_lo, scan_hi;   /* rango de escaneo de VFO (10 Hz) */
    uint8_t  allow_tx_all;       /* 0 = solo bandas de radioaficionado/permitidas */
    uint8_t  air_tx;             /* permitir TX en banda aerea (118-137 MHz) */
    uint8_t  repeater_tail;      /* eliminar cola de tono del repetidor */
    /* --- interfaz --- */
    uint8_t  brightness;         /* 0..4 */
    uint8_t  backlight_s;        /* 0 = siempre */
    uint8_t  key_beep;
    uint8_t  voice;
    uint8_t  key_lock;
    uint8_t  auto_lock_s;
    uint8_t  side1_short, side1_long, side2_short, side2_long, alarm_key;
    uint8_t  alarm_type;         /* 0 local, 1 TX, 2 ambos */
    uint8_t  batt_save;
    uint8_t  carrier_led;
    char     welcome[17];
    uint8_t  boot_logo;          /* mostrar logo desde flash SPI */
    /* --- DTMF --- */
    char     dtmf_id[9];
    char     dtmf_kill[9], dtmf_stun[9], dtmf_wake[9];
    uint8_t  dtmf_remote;        /* aceptar kill/stun remoto */
    uint8_t  dtmf_ptt_id;        /* 0 no, 1 al inicio, 2 al final, 3 ambos */
    uint8_t  dtmf_tone_ms, dtmf_gap_ms, dtmf_delay_100ms;
    uint8_t  dtmf_threshold;
    char     dtmf_mem[16][17];
    uint8_t  killed;             /* 1 = stun, 2 = kill */
    /* --- APRS --- */
    char     aprs_call[7];
    uint8_t  aprs_ssid;
    char     aprs_dest[7];
    char     aprs_path[24];
    char     aprs_sym_table, aprs_sym;
    char     aprs_comment[40];
    uint16_t aprs_beacon_s;      /* 0 = manual */
    uint32_t aprs_freq;          /* 10 Hz (144.390 / 144.800 ...) */
    uint8_t  aprs_rx;            /* decodificar en la frecuencia APRS */
    uint8_t  aprs_use_gps;
    int32_t  aprs_lat_e7, aprs_lon_e7;
    /* --- GPS --- */
    uint8_t  gps_on;
    uint8_t  gps_baud_idx;
    int8_t   tz_quarter_h;       /* zona horaria en cuartos de hora */
    uint8_t  gps_track;          /* registrar traza */
    uint16_t gps_track_s;
    uint8_t  units_imperial;
    /* --- receptor de radiodifusion (Si4732) --- */
    uint8_t  bc_band;
    uint16_t bc_freq[8];
    uint8_t  bc_bw, bc_agc, bc_volume;
    int16_t  bc_bfo;
    uint8_t  bc_ssb_lsb;
    uint32_t crc;
} settings_t;

extern settings_t g_set;
void settings_load(void);
void settings_save(void);
void settings_defaults(void);
void settings_mark_dirty(void);
void settings_service(void);        /* guarda con retraso desde el bucle principal */

extern const uint32_t step_table_10hz[];
extern const uint8_t step_count;

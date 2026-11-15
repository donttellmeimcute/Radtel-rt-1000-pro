#include "settings.h"
#include "spiflash.h"
#include "layout.h"
#include "system.h"
#include <string.h>

#define SET_MAGIC 0x4F525431u   /* "1TRO" */
#define SET_VERSION 3

settings_t g_set;
static uint32_t dirty_at;
static uint8_t dirty;

const uint32_t step_table_10hz[] = {25, 50, 100, 125, 250, 500, 625, 833, 1000, 1250, 2000, 2500, 5000, 10000};
const uint8_t step_count = sizeof(step_table_10hz) / sizeof(step_table_10hz[0]);

static uint32_t crc32(const void *d, uint32_t n)
{
    const uint8_t *p = d; uint32_t c = 0xFFFFFFFFu;
    while (n--) { c ^= *p++; for (int i = 0; i < 8; i++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1)); }
    return ~c;
}

void settings_defaults(void)
{
    settings_t *s = &g_set;
    memset(s, 0, sizeof(*s));
    s->magic = SET_MAGIC; s->version = SET_VERSION; s->size = sizeof(*s);
    channel_default(&s->area[0].vfo, 14652000);    /* 146.520 MHz */
    channel_default(&s->area[1].vfo, 44600000);    /* 446.000 MHz */
    s->area[0].mode = s->area[1].mode = AREA_VFO;
    s->squelch = 3; s->glitch = 4; s->volume = 10; s->mic_gain = 12; s->step_idx = 5;
    s->tot_s = 18; s->scan_resume_s = 5; s->scan_lo = 14400000; s->scan_hi = 14800000;
    s->brightness = 3; s->backlight_s = 15; s->key_beep = 1; s->auto_lock_s = 0;
    s->side1_short = SK_MONITOR; s->side1_long = SK_SCAN;
    s->side2_short = SK_BCAST; s->side2_long = SK_APRS_BEACON; s->alarm_key = SK_ALARM;
    s->carrier_led = 1; s->boot_logo = 1;
    strcpy(s->welcome, "RT-1000 libre");
    strcpy(s->dtmf_id, "101");
    strcpy(s->dtmf_kill, "AB911"); strcpy(s->dtmf_stun, "AB912"); strcpy(s->dtmf_wake, "AB913");
    s->dtmf_tone_ms = 80; s->dtmf_gap_ms = 80; s->dtmf_delay_100ms = 3; s->dtmf_threshold = 0x18;
    strcpy(s->aprs_call, "NOCALL"); s->aprs_ssid = 7; strcpy(s->aprs_dest, "APRT10");
    strcpy(s->aprs_path, "WIDE1-1,WIDE2-1");
    s->aprs_sym_table = '/'; s->aprs_sym = '['; strcpy(s->aprs_comment, "RT-1000 firmware libre");
    s->aprs_beacon_s = 0; s->aprs_freq = 14439000; s->aprs_use_gps = 1;
    s->gps_baud_idx = 1; s->gps_track_s = 30;
    s->bc_band = 0; s->bc_volume = 50;
    static const uint16_t bcdef[8] = {10110, 1000, 7100, 14200, 9600, 198, 0, 0};
    memcpy(s->bc_freq, bcdef, sizeof(bcdef));
}

static int try_load(uint32_t addr)
{
    spiflash_read(addr, &g_set, sizeof(g_set));
    return g_set.magic == SET_MAGIC && g_set.version == SET_VERSION && g_set.size == sizeof(g_set) &&
           g_set.crc == crc32(&g_set, offsetof(settings_t, crc));
}

void settings_load(void)
{
    if (try_load(FLASH_OS_SETTINGS)) return;
    if (try_load(FLASH_OS_SETTINGS_BAK)) return;
    settings_defaults();
}

void settings_save(void)
{
    g_set.crc = crc32(&g_set, offsetof(settings_t, crc));
    spiflash_write(FLASH_OS_SETTINGS, &g_set, sizeof(g_set));
    spiflash_write(FLASH_OS_SETTINGS_BAK, &g_set, sizeof(g_set));
    dirty = 0;
}

void settings_mark_dirty(void) { dirty = 1; dirty_at = g_ticks_ms; }

void settings_service(void)
{
    if (dirty && (uint32_t)(g_ticks_ms - dirty_at) > 3000) settings_save();
}

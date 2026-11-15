/* Driver Si473x escrito desde cero segun la nota de aplicacion AN332
 * (publica) y la secuencia observada en el firmware original. */
#include "si473x.h"
#include "i2c.h"
#include "gpio.h"
#include "system.h"
#include "spiflash.h"
#include "layout.h"

#define ADDR 0x11
static const pin_t P_EN = {GPIOA, 8};
static int cur_mode = SI_MODE_OFF;

static int wait_cts(void)
{
    uint8_t s = 0;
    for (int i = 0; i < 100; i++) {
        if (i2c2_read(ADDR, &s, 1) == 0 && (s & 0x80)) return 0;
        delay_ms(1);
    }
    return -1;
}

static int cmd(const uint8_t *b, int n)
{
    if (wait_cts()) return -1;
    if (i2c2_write(ADDR, b, n)) return -1;
    return wait_cts();
}

static void prop(uint16_t p, uint16_t v)
{
    uint8_t b[6] = {0x12, 0, (uint8_t)(p >> 8), (uint8_t)p, (uint8_t)(v >> 8), (uint8_t)v};
    cmd(b, 6);
}

static void power_down(void)
{
    static const uint8_t b[] = {0x11};
    cmd(b, 1);
}

int si473x_ssb_patch_available(void)
{
    uint8_t hdr[8];
    spiflash_read(FLASH_SSB_PATCH, hdr, 8);
    return hdr[0] == 'S' && hdr[1] == 'S' && hdr[2] == 'B' && hdr[3] == 'P';
}

/* Formato en flash: "SSBP" + u32 longitud LE, seguido de bloques de 8 bytes */
static int load_ssb_patch(void)
{
    uint8_t hdr[8], blk[8];
    spiflash_read(FLASH_SSB_PATCH, hdr, 8);
    if (hdr[0] != 'S' || hdr[1] != 'S' || hdr[2] != 'B' || hdr[3] != 'P') return -1;
    uint32_t len = hdr[4] | (hdr[5] << 8) | ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
    if (len == 0 || len > 0x8000 || (len & 7)) return -1;
    static const uint8_t pu[] = {0x01, 0x31, 0x05};   /* POWER_UP AM + PATCH + XOSC */
    if (cmd(pu, 3)) return -1;
    delay_ms(1);
    for (uint32_t o = 0; o < len; o += 8) {
        spiflash_read(FLASH_SSB_PATCH + 8 + o, blk, 8);
        if (i2c2_write(ADDR, blk, 8)) return -1;
        if (wait_cts()) return -1;
    }
    return 0;
}

static void base_props(void)
{
    prop(0xFF00, 0);           /* desactivar modo debug */
    prop(0x4000, 63);          /* RX_VOLUME */
}

int si473x_start(int mode, uint16_t freq)
{
    CRM_AHBEN1 |= CRM_AHBEN1_GPIOA;
    gpio_mode(P_EN, GPIO_OUT, GPIO_NOPULL);
    if (cur_mode != SI_MODE_OFF) power_down();
    gpio_clr(P_EN); delay_ms(5);
    gpio_set(P_EN); delay_ms(10);
    cur_mode = SI_MODE_OFF;

    if (mode == SI_MODE_FM) {
        static const uint8_t pu[] = {0x01, 0x10, 0x05};
        if (cmd(pu, 3)) return -1;
        delay_ms(500);           /* arranque del cristal de 32.768 kHz */
        static const uint8_t g1[] = {0x80, 0x02}, g2[] = {0x81, 0x02};
        cmd(g1, 2); cmd(g2, 2);
        base_props();
        prop(0x1403, 6);         /* FM_SEEK_TUNE_SNR_THRESHOLD */
        prop(0x3103, 0x7800);
    } else if (mode == SI_MODE_AM) {
        static const uint8_t pu[] = {0x01, 0x11, 0x05};
        if (cmd(pu, 3)) return -1;
        delay_ms(500);
        base_props();
        prop(0x3302, 0);         /* AM_SOFT_MUTE_MAX_ATTEN */
        prop(0x3103, 0x7800);    /* AM_AVC_MAX_GAIN */
        prop(0x3404, 0x10);      /* AM_SEEK_TUNE_RSSI_THRESHOLD */
    } else if (mode == SI_MODE_LSB || mode == SI_MODE_USB) {
        if (load_ssb_patch()) return -2;
        delay_ms(500);
        base_props();
        prop(0x0101, 0x9013);    /* SSB_MODE: AFCDIS|AVCEN, 4 kHz */
        prop(0x3302, 0);
        prop(0x3103, 0x7800);
        prop(0x0100, 0);         /* SSB_BFO */
    } else {
        gpio_clr(P_EN);
        return 0;
    }
    cur_mode = mode;
    si473x_set_agc(0);
    si473x_tune(freq);
    return 0;
}

void si473x_stop(void)
{
    if (cur_mode != SI_MODE_OFF) power_down();
    gpio_clr(P_EN);
    cur_mode = SI_MODE_OFF;
}

void si473x_tune(uint16_t f)
{
    if (cur_mode == SI_MODE_FM) {
        uint8_t b[5] = {0x20, 0x00, (uint8_t)(f >> 8), (uint8_t)f, 0};
        cmd(b, 5);
    } else if (cur_mode != SI_MODE_OFF) {
        uint8_t arg1 = cur_mode == SI_MODE_USB ? 0x80 : cur_mode == SI_MODE_LSB ? 0x40 : 0x00;
        uint8_t b[6] = {0x40, arg1, (uint8_t)(f >> 8), (uint8_t)f, 0, (uint8_t)(f > 1800 ? 1 : 0)};
        cmd(b, 6);
    }
}

void si473x_seek(int up)
{
    if (cur_mode == SI_MODE_FM) {
        uint8_t b[2] = {0x21, (uint8_t)(up ? 0x0C : 0x04)};
        cmd(b, 2);
    } else if (cur_mode == SI_MODE_AM) {
        uint8_t b[6] = {0x41, (uint8_t)(up ? 0x0C : 0x04), 0, 0, 0, 0};
        cmd(b, 6);
    }
}

void si473x_status(si_status_t *s)
{
    uint8_t r[8] = {0};
    uint8_t c[2] = {(uint8_t)(cur_mode == SI_MODE_FM ? 0x22 : 0x42), 0x00};
    if (cmd(c, 2) == 0 && i2c2_read(ADDR, r, 8) == 0) {
        s->freq = (uint16_t)(r[2] << 8 | r[3]);
        s->rssi = r[4];
        s->snr = r[5];
        s->valid = r[1] & 0x01;
    }
}

void si473x_set_volume(uint8_t v) { prop(0x4000, v > 63 ? 63 : v); }

void si473x_set_agc(uint8_t gain)
{
    uint8_t b[3] = {(uint8_t)(cur_mode == SI_MODE_FM ? 0x28 : 0x48), (uint8_t)(gain ? 1 : 0),
                    (uint8_t)(gain ? gain - 1 : 0)};
    cmd(b, 3);
}

void si473x_set_ssb_bfo(int16_t hz) { prop(0x0100, (uint16_t)hz); }

void si473x_set_bandwidth(uint8_t idx)
{
    if (cur_mode == SI_MODE_AM) {
        static const uint8_t bw[7] = {0, 1, 2, 6, 3, 5, 4};   /* 6,4,3,2.5,2,1.8,1 kHz */
        prop(0x3102, bw[idx % 7] | 0x0100);                    /* AM_CHANNEL_FILTER + PWR_LINE_NOISE_REJ */
    } else if (cur_mode == SI_MODE_LSB || cur_mode == SI_MODE_USB) {
        static const uint16_t bw[6] = {0x9004, 0x9005, 0x9000, 0x9011, 0x9012, 0x9013};
        prop(0x0101, bw[idx % 6]);
    }
}

void si473x_set_band_limits(uint16_t lo, uint16_t hi, uint16_t spacing)
{
    if (cur_mode == SI_MODE_FM) {
        prop(0x1400, lo); prop(0x1401, hi); prop(0x1402, spacing);
    } else {
        prop(0x3400, lo); prop(0x3401, hi); prop(0x3402, spacing);
    }
}

/* Control del transceptor BK4819: bandas, calibracion de fabrica, rutas de RF,
 * squelch, subtonos y TX. Las secuencias replican el firmware original
 * (docs/rf.md); los valores de calibracion se leen de la flash SPI de fabrica
 * (direccion 0x000000, copia en 0x001000) y NUNCA se escriben.               */
#include "radio.h"
#include "bk4819.h"
#include "board.h"
#include "system.h"
#include "spiflash.h"
#include "layout.h"
#include "settings.h"
#include "audio.h"
#include <string.h>

radio_t g_radio;

/* Bandas de calibracion del original (unidades de 10 Hz) y rutas RX/TX */
static const struct { uint32_t lo, hi; uint8_t rx_path, tx_path; } bands[15] = {
    {13600000, 17400000, 3, 3}, {40000000, 48000000, 4, 4}, {1800000, 6400000, 1, 1},
    {6400000, 13600000, 3, 3}, {17400000, 24000000, 3, 3}, {24000000, 32000000, 6, 3},
    {32000000, 40000000, 4, 4}, {48000000, 56000000, 4, 4}, {56000000, 66000000, 4, 4},
    {84000000, 92000000, 5, 5}, {92000000, 100000000, 5, 5}, {100000000, 108000000, 5, 5},
    {108000000, 116000000, 5, 5}, {116000000, 124000000, 5, 5}, {124000000, 130000000, 5, 5},
};

static uint8_t cal[0x70];            /* bloque de calibracion de la banda actual */
static uint8_t cal_am[0x40];         /* tablas AM (0x6B0-0x6EF) */
static uint8_t cal_band = 0xFF;

/* Tablas de squelch del original (indice 0..10) */
static const uint8_t sq_glitch[11] = {0x1E, 0x32, 0x46, 0x5A, 0x6E, 0x82, 0x96, 0xAA, 0xBE, 0xD2, 0xFF};
static const uint8_t sq_noise_add[11] = {0x0A, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
static const uint8_t sq_rssi_add[11] = {0x00, 0x00, 0x02, 0x04, 0x06, 0x0A, 0x10, 0x16, 0x1C, 0x22, 0x26};

static uint32_t cal_base;

static void load_cal_base(void)
{
    uint8_t m;
    spiflash_read(FLASH_CAL, &m, 1);
    if (m == 0x9A) { cal_base = FLASH_CAL; g_radio.cal_ok = 1; return; }
    spiflash_read(FLASH_CAL_BACKUP, &m, 1);
    if (m == 0x9A) { cal_base = FLASH_CAL_BACKUP; g_radio.cal_ok = 1; return; }
    g_radio.cal_ok = 0;
}

static void default_cal(void)
{
    memset(cal, 0, sizeof(cal));
    cal[0] = 0x00; cal[1] = 0x80;                  /* offset 0 (0x8000) */
    cal[3] = 0xE2; cal[4] = 0x34;                  /* desviacion ancho 0x34E2 */
    cal[10] = 0x10; cal[11] = 0x2A;                /* desviacion estrecho */
    memset(cal + 0x10, 0x28, 16);                  /* PA alta (conservador) */
    memset(cal + 0x20, 0x0C, 16);                  /* PA baja */
    memset(cal + 0x30, 0x40, 16); memset(cal + 0x50, 0x40, 16);
    memset(cal + 0x40, 0x50, 16); memset(cal + 0x60, 0x50, 16);
}

int radio_band_of(uint32_t f)
{
    for (int i = 0; i < 15; i++)
        if (f >= bands[i].lo && f < bands[i].hi) return i;
    return -1;
}

static void select_band(uint32_t f)
{
    int b = radio_band_of(f);
    g_radio.band = (uint8_t)(b < 0 ? 0xFF : b);
    if (b < 0 || b == cal_band) return;
    cal_band = (uint8_t)b;
    if (g_radio.cal_ok) {
        spiflash_read(cal_base + 0x20 + (uint32_t)b * 0x70, cal, 0x70);
        spiflash_read(cal_base + 0x6B0, cal_am, 0x40);
    } else default_cal();
}

static uint8_t subband(uint32_t f)
{
    if (g_radio.band == 0xFF) return 0;
    uint32_t i = (f - bands[g_radio.band].lo) / 500000u;
    return (uint8_t)(i > 15 ? 15 : i);
}

static int16_t cal_offset(void) { return (int16_t)((cal[0] | cal[1] << 8) - 0x8000); }

/* Rutas RX: bits de GPIO del BK4819 en REG_33 */
static void rx_path(uint8_t id)
{
    static const uint8_t bit[7] = {0, 0x01, 0x01, 0x10, 0x20, 0x02, 0x02};
    bk4819_set_gpio(0x33, id < 7 ? bit[id] : 0);
    gpio_write(PIN_RF_PATH, !(id == 3 || id == 4));
}

static void tx_path(uint8_t id)
{
    gpio_clr(PIN_RF_LOWBAND_TX);
    bk4819_set_gpio(0x0C, 0);
    if (id == 1 || id == 2) gpio_set(PIN_RF_LOWBAND_TX);
    else if (id == 3) bk4819_set_gpio(0x08, 0x08);
    else if (id >= 4) bk4819_set_gpio(0x04, 0x04);
    gpio_write(PIN_RF_PATH, !(id == 3 || id == 4));
}

static void tx_path_off(void)
{
    gpio_clr(PIN_RF_LOWBAND_TX);
    bk4819_set_gpio(0x0C, 0);
}

static void set_subtone(uint16_t t)
{
    int type = tone_type(t);
    bk4819_set_subtone(type > 3 ? 0 : type, tone_code(t));
}

void radio_apply_squelch(void)
{
    uint8_t sq = g_set.squelch > 10 ? 10 : g_set.squelch;
    uint8_t gl = g_set.glitch > 10 ? 10 : g_set.glitch;
    int narrow = ch_narrow(&g_radio.cur);
    uint8_t sb = subband(g_radio.cur.rx);
    uint8_t nsq = cal[(narrow ? 0x50 : 0x30) + sb];
    uint8_t rsq = cal[(narrow ? 0x60 : 0x40) + sb];
    uint16_t r4f = (uint16_t)(((nsq + 2 + sq_noise_add[sq]) << 8) | (uint8_t)(nsq - 4 + sq_noise_add[sq]));
    uint16_t r78 = (uint16_t)(((rsq + 2 + sq_rssi_add[sq]) << 8) | (uint8_t)(rsq - 4 + sq_rssi_add[sq]));
    bk4819_set_squelch(sq_glitch[gl], r4f, r78);
}

void radio_init(void)
{
    CRM_AHBEN1 |= CRM_AHBEN1_GPIOA | CRM_AHBEN1_GPIOB | CRM_AHBEN1_GPIOC | CRM_AHBEN1_GPIOF;
    pin_t outs[] = {PIN_RF_LOWBAND_TX, PIN_RF_PATH, PIN_RF_AM1, PIN_RF_AM2};
    for (unsigned i = 0; i < sizeof(outs) / sizeof(outs[0]); i++) {
        gpio_clr(outs[i]);
        gpio_mode(outs[i], GPIO_OUT, GPIO_NOPULL);
    }
    gpio_set(PIN_RF_AM2);        /* estado inicial del original */
    load_cal_base();
    default_cal();
    bk4819_init();
    g_radio.state = RS_IDLE;
}

void radio_set_volume(uint8_t v) { bk4819_set_volume(v); }

void radio_tune(const channel_t *c)
{
    if (g_radio.state == RS_TX) radio_tx_stop();
    g_radio.cur = *c;
    uint32_t f = c->rx;
    int mod = ch_mod(c);

    bk4819_set_agc_default();
    bk4819_set_af(0);
    bk4819_set_bandwidth(!ch_narrow(c));
    bk4819_write(0x31, bk4819_read(0x31) & ~0x0008u);   /* compander off */
    bk4819_write(0x28, 0x6B38);
    bk4819_write(0x29, 0xAB40);

    select_band(f);
    bk4819_set_freq(f, cal_offset());
    set_subtone(c->rx_tone);
    radio_apply_squelch();
    int am = mod != MOD_FM;
    gpio_write(PIN_RF_AM1, am);
    gpio_write(PIN_RF_AM2, am);
    /* REG_73 bit4: AFC apagado en SSB */
    uint16_t r73 = bk4819_read(0x73);
    bk4819_write(0x73, (mod >= MOD_USB) ? (r73 | 0x10) : (r73 & ~0x10u));
    bk4819_scrambler((uint8_t)ch_scrambler(c));
    bk4819_rx_on();
    rx_path(g_radio.band == 0xFF ? 0 : bands[g_radio.band].rx_path);
    bk4819_vox(g_set.vox != 0);
    g_radio.state = RS_RX;
    g_radio.sq_open = 0;
}

/* Bandas en las que se permite TX por defecto (unidades de 10 Hz) */
static const uint32_t ham_bands[][2] = {
    {2800000, 2970000},    /* 10 m */
    {5000000, 5400000},    /* 6 m */
    {14400000, 14800000},  /* 2 m */
    {22000000, 22500000},  /* 1.25 m */
    {42000000, 45000000},  /* 70 cm */
    {44600000, 44620000},  /* PMR446 */
    {46256250, 46772500},  /* FRS/GMRS */
    {90200000, 92800000},  /* 33 cm */
    {124000000, 130000000},/* 23 cm */
};

int radio_tx_allowed(uint32_t f)
{
    if (radio_band_of(f) < 0) return 0;
    if (f >= 10800000 && f < 13700000) return g_set.air_tx;     /* banda aerea */
    if (g_set.allow_tx_all) return 1;
    for (unsigned i = 0; i < sizeof(ham_bands) / sizeof(ham_bands[0]); i++)
        if (f >= ham_bands[i][0] && f < ham_bands[i][1]) return 1;
    return 0;
}

int radio_tx_start(void)
{
    const channel_t *c = &g_radio.cur;
    uint32_t f = c->tx;
    if (g_set.killed) return -3;
    if (!radio_tx_allowed(f)) return -1;
    if (ch_busy_lock(c) && g_radio.sq_open) return -2;
    int mod = ch_mod(c);
    if (mod >= MOD_USB) return -1;   /* el BK4819 no transmite SSB */

    gpio_clr(PIN_LED_GREEN);
    gpio_set(PIN_LED_RED);
    audio_amp_request(AMP_RX, 0);
    bk4819_set_gpio(0x33, 0);           /* rutas RX apagadas */
    bk4819_set_af(0);
    select_band(f);
    bk4819_set_freq(f, cal_offset());
    set_subtone(c->tx_tone);
    bk4819_scrambler((uint8_t)ch_scrambler(c));
    bk4819_set_mic_gain(g_set.mic_gain);
    int narrow = ch_narrow(c);
    bk4819_set_deviation((uint16_t)(narrow ? (cal[10] | cal[11] << 8) : (cal[3] | cal[4] << 8)));
    uint8_t sb = subband(f);
    if (mod == MOD_AM) {
        /* AM: el original usa DAC2 (PA5) como nivel de modulacion/PA */
        uint8_t bi = (g_radio.band == 3) ? 0x20 : 0x00;
        bk4819_write(0x48, 0xB003);
        bk4819_set_af(1);
        bk4819_write(0x7D, 0xE940);
        bk4819_write(0x19, bk4819_read(0x19) | 0x8000);
        bk4819_write(0x7E, bk4819_read(0x7E) & 0x0FC0);
        uint16_t r2b = bk4819_read(0x2B);
        r2b = (uint16_t)((r2b & ~0x0700u) | (narrow ? 0 : 0x0100) | 0x07);
        bk4819_write(0x2B, r2b);
        bk4819_write(0x37, 0x1D0F);
        delay_ms(10);
        bk4819_write(0x52, 0x028F);
        bk4819_write(0x30, 0x0200);
        bk4819_write(0x30, 0xC2FE);
        tx_path(bands[g_radio.band].tx_path);
        gpio_clr(PIN_RF_AM2);
        DAC_CTRL |= 1u << 16;
        DAC_D2DTH8R = cal_am[bi + 0x10 + sb];
        bk4819_set_pa(cal_am[bi + sb] & 0x3F);
    } else {
        bk4819_tx_on(tone_type(c->tx_tone) != 0);
        tx_path(bands[g_radio.band].tx_path);
        gpio_clr(PIN_RF_AM2);
        bk4819_set_pa(cal[(ch_high_power(c) ? 0x10 : 0x20) + sb] & 0x3F);
    }
    g_radio.state = RS_TX;
    g_radio.tx_start_ms = g_ticks_ms;
    return 0;
}

void radio_tx_stop(void)
{
    if (g_radio.state != RS_TX) return;
    const channel_t *c = &g_radio.cur;
    if (tone_type(c->tx_tone) && ch_ste(c)) {
        bk4819_tail_tone();
        delay_ms(200);
    }
    if (g_set.roger) {
        bk4819_write(0x70, 0xC000 | 0x60);
        bk4819_write(0x71, (uint16_t)(1500u * 103244u / 10000u));
        delay_ms(150);
        bk4819_write(0x70, 0);
    }
    tx_path_off();
    bk4819_write(0x36, 0x007F);
    delay_ms(20);
    DAC_D2DTH8R = 0;
    gpio_clr(PIN_LED_RED);
    g_radio.state = RS_IDLE;
    radio_tune(c);
}

void radio_poll(void)
{
    if (g_radio.state != RS_RX) return;
    uint16_t raw = bk4819_rssi_raw();
    g_radio.rssi_dbm = (int16_t)(raw / 2) - 167;
    g_radio.noise = bk4819_noise();
    int open = g_radio.monitor || g_set.squelch == 0 || bk4819_squelch_open();
    if (open != g_radio.sq_open) {
        g_radio.sq_open = (uint8_t)open;
        if (open) {
            int mod = ch_mod(&g_radio.cur);
            radio_set_volume(g_set.volume);
            bk4819_set_af(mod == MOD_FM ? 1 : mod == MOD_AM ? 7 : 5);
            if (g_set.carrier_led) gpio_set(PIN_LED_GREEN);
            gpio_clr(PIN_RF_AM1);
            audio_amp_request(AMP_RX, 1);
        } else {
            bk4819_set_af(0);
            gpio_clr(PIN_LED_GREEN);
            audio_amp_request(AMP_RX, 0);
            gpio_write(PIN_RF_AM1, ch_mod(&g_radio.cur) != MOD_FM);
        }
    }
}

void radio_sleep(void)
{
    bk4819_set_gpio(0x33, 0);
    bk4819_sleep();
    g_radio.state = RS_IDLE;
}

void radio_wake(void) { radio_tune(&g_radio.cur); }

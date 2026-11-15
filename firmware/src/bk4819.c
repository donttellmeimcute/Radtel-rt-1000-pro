/* Driver BK4819 escrito desde cero. Las secuencias de registros replican lo
 * observado en el firmware original (ver docs/rf.md).                          */
#include "bk4819.h"
#include "gpio.h"
#include "system.h"
#include "tones.h"

static const pin_t SCN = {GPIOC, 8}, SCK = {GPIOC, 9}, SDA = {GPIOC, 7};

static inline void d(void) { delay_us(1); }

static void tx8(uint8_t v)
{
    for (int i = 7; i >= 0; i--) {
        gpio_clr(SCK);
        gpio_write(SDA, (v >> i) & 1);
        d();
        gpio_set(SCK);
        d();
    }
}

void bk4819_write(uint8_t reg, uint16_t val)
{
    gpio_clr(SCK);
    gpio_clr(SCN);
    tx8(reg & 0x7F);
    tx8(val >> 8);
    tx8(val & 0xFF);
    gpio_clr(SCK);
    gpio_set(SCN);
}

uint16_t bk4819_read(uint8_t reg)
{
    uint16_t v = 0;
    gpio_clr(SCK);
    gpio_clr(SCN);
    tx8(reg | 0x80);
    gpio_mode(SDA, GPIO_IN, GPIO_NOPULL);
    d();
    for (int i = 0; i < 16; i++) {
        gpio_clr(SCK);
        d();
        gpio_set(SCK);
        v = (uint16_t)((v << 1) | gpio_read(SDA));
        d();
    }
    gpio_clr(SCK);
    gpio_set(SCN);
    gpio_mode(SDA, GPIO_OUT, GPIO_NOPULL);
    return v;
}

void bk4819_set_agc_default(void)
{
    bk4819_write(0x13, 0x03BE);
    bk4819_write(0x12, 0x037B);
    bk4819_write(0x11, 0x027B);
    bk4819_write(0x10, 0x007A);
    bk4819_write(0x14, 0x0019);
    bk4819_write(0x49, 0x2A38);
    bk4819_write(0x7B, 0x8420);
}

static const uint16_t filt54[8] = {0x8BD7, 0x8AED, 0x8A96, 0x8908, 0x8DDE, 0x8EB3, 0x8F88, 0x8BE3};
static const uint16_t filt55[8] = {0x3507, 0x35E1, 0x3606, 0x3779, 0x335A, 0x32BA, 0x3201, 0x352D};
static const uint16_t filt75[8] = {0xE61C, 0xDF22, 0xD42D, 0xCC35, 0xFA02, 0xF50B, 0xF50B, 0xF50B};

void bk4819_set_filter_table(uint8_t idx)
{
    if (idx > 7) idx = 7;
    bk4819_write(0x75, filt75[idx]);
    bk4819_write(0x54, filt54[idx]);
    bk4819_write(0x55, filt55[idx]);
    bk4819_write(0x74, filt75[idx]);
    bk4819_write(0x44, filt54[idx]);
    bk4819_write(0x45, filt55[idx]);
}

void bk4819_init(void)
{
    CRM_AHBEN1 |= CRM_AHBEN1_GPIOC;
    gpio_set(SCN);
    gpio_mode(SCN, GPIO_OUT, GPIO_NOPULL);
    gpio_mode(SCK, GPIO_OUT, GPIO_NOPULL);
    gpio_mode(SDA, GPIO_OUT, GPIO_NOPULL);

    bk4819_write(0x00, 0x8000);
    delay_ms(10);
    bk4819_write(0x00, 0x0000);
    bk4819_write(0x37, 0x1D0F);
    bk4819_set_agc_default();
    bk4819_write(0x33, 0x0000);
    bk4819_write(0x34, 0x0000);
    bk4819_write(0x35, 0x0000);
    bk4819_write(0x1E, 0x4C58);
    bk4819_write(0x1F, 0xA656);
    bk4819_write(0x3E, 0xA037);
    bk4819_write(0x7D, 0xE950);
    bk4819_write(0x26, 0x13A0);
    bk4819_set_filter_table(5);
    bk4819_rx_on();
    bk4819_write(0x71, 0x0000);
    bk4819_scrambler(0);
    bk4819_set_af(0);
}

void bk4819_set_freq(uint32_t f, int16_t cal)
{
    uint32_t r = (uint32_t)((int32_t)f + cal);
    bk4819_write(0x38, r & 0xFFFF);
    bk4819_write(0x39, r >> 16);
}

void bk4819_rx_on(void)
{
    bk4819_write(0x37, 0x1F0F);
    delay_ms(10);
    bk4819_write(0x30, 0x0200);
    bk4819_write(0x30, 0xBFF1);
}

void bk4819_sleep(void)
{
    bk4819_write(0x30, 0x0000);
    bk4819_write(0x37, 0x1D00);
}

void bk4819_set_af(uint8_t af)    { bk4819_write(0x47, (uint16_t)(0x6040 | (af << 8))); }
void bk4819_set_volume(uint8_t v) { bk4819_write(0x48, (uint16_t)(0xB00F | ((v & 0xF) << 4))); }
void bk4819_set_bandwidth(int wide) { bk4819_write(0x43, wide ? 0x3028 : 0x4048); }

void bk4819_set_squelch(uint8_t glitch, uint16_t r4f, uint16_t r78)
{
    bk4819_write(0x4D, (uint16_t)(0xA000 + glitch));
    bk4819_write(0x4E, (uint16_t)(0x6F00 + glitch));
    bk4819_write(0x4F, r4f);
    bk4819_write(0x78, r78);
}

void bk4819_set_subtone(int type, uint16_t code)
{
    if (type == 1) {
        bk4819_write(0x51, 0x9040);
        bk4819_write(0x07, (uint16_t)((code * 0x19Du / 200u) & 0x1FFF));
    } else if (type == 2 || type == 3) {
        uint32_t w = dcs_golay_word(code);
        if (type == 3) w ^= 0x7FFFFF;
        bk4819_write(0x51, type == 2 ? 0x802A : 0xA02A);
        bk4819_write(0x07, 0x0AD7);
        bk4819_write(0x08, w & 0xFFF);
        bk4819_write(0x08, (uint16_t)(0x8000 + (w >> 12)));
    } else {
        bk4819_write(0x51, 0x0000);
    }
}

void bk4819_tail_tone(void)
{
    bk4819_write(0x51, 0x9040);
    bk4819_write(0x07, 0x046F);   /* 55 Hz */
}

void bk4819_set_mic_gain(uint8_t g) { bk4819_write(0x7D, (uint16_t)(0xE940 | (g & 0x1F))); }
void bk4819_set_deviation(uint16_t v) { bk4819_write(0x40, v); }

void bk4819_set_pa(uint8_t p)
{
    bk4819_write(0x36, (uint16_t)(((p & 0x3F) << 10) | (p ? 0xFF : 0x7F)));
}

void bk4819_tx_on(int with_tone)
{
    bk4819_write(0x37, 0x1D0F);
    delay_ms(10);
    bk4819_write(0x52, 0x028F);
    bk4819_write(0x30, 0x0200);
    bk4819_write(0x30, with_tone ? 0xC3FA : 0xC1FE);
}

void bk4819_set_gpio(uint8_t mask, uint8_t value)
{
    uint16_t r = bk4819_read(0x33);
    r = (uint16_t)((r & ~mask) | (value & mask));
    bk4819_write(0x33, r);
}

uint16_t bk4819_rssi_raw(void) { return bk4819_read(0x67) & 0x1FF; }
int bk4819_rssi_dbm(void) { return (int)(bk4819_rssi_raw() / 2) - 167; }
uint8_t bk4819_noise(void) { return bk4819_read(0x65) & 0x7F; }
int bk4819_squelch_open(void) { return (bk4819_read(0x0C) >> 1) & 1; }

void bk4819_scrambler(uint8_t n)
{
    uint16_t r31 = bk4819_read(0x31);
    if (n) {
        bk4819_write(0x71, (uint16_t)(0x68DC + n * 0x408));
        bk4819_write(0x31, r31 | 0x0002);
    } else {
        bk4819_write(0x71, 0x68DC);
        bk4819_write(0x31, r31 & ~0x0002);
    }
}

void bk4819_vox(int on)
{
    uint16_t r = bk4819_read(0x31);
    bk4819_write(0x31, on ? (r | 0x0004) : (r & ~0x0004));
}

/* ---- DTMF / tonos ---- */
void bk4819_dtmf_tx_enable(void)
{
    bk4819_write(0x70, 0xE0E0);
    bk4819_write(0x48, 0xB32A);
    bk4819_set_af(3);
    bk4819_write(0x30, 0xC3FA);
}

void bk4819_dtmf_tone(char c)
{
    uint16_t lo, hi;
    if (!dtmf_freqs(c, &lo, &hi)) { bk4819_write(0x70, 0); return; }
    /* Hz * 10.32444 (factor del BK4819) */
    bk4819_write(0x71, (uint16_t)((lo * 103244u + 5000) / 10000));
    bk4819_write(0x72, (uint16_t)((hi * 103244u + 5000) / 10000));
    bk4819_write(0x70, 0xE0E0);
}

void bk4819_beep(uint16_t hz, int on)
{
    if (!on) { bk4819_write(0x70, 0x0000); bk4819_set_af(0); return; }
    bk4819_write(0x70, 0xC000 | 0x60);
    bk4819_write(0x71, (uint16_t)((hz * 103244u + 5000) / 10000));
    bk4819_set_af(2);    /* tono a altavoz */
    bk4819_write(0x30, 0x0302);
}

static const uint16_t dtmf_coef[16] = {
    0x006F, 0x106B, 0x2067, 0x3062, 0x4050, 0x5047, 0x603A, 0x702C,
    0x8041, 0x9037, 0xA025, 0xB017, 0xC0E4, 0xD0CB, 0xE0B5, 0xF09F,
};

void bk4819_dtmf_rx_enable(uint8_t thr)
{
    for (int i = 0; i < 16; i++) bk4819_write(0x09, dtmf_coef[i]);
    bk4819_write(0x21, 0x06D8);
    bk4819_write(0x24, (uint16_t)(0x807E | ((thr & 0x1F) << 7)));
}

int bk4819_dtmf_rx_get(void)
{
    static const char map[16] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','*','#'};
    uint16_t st = bk4819_read(0x0C);
    if (!(st & 0x0001)) return -1;          /* sin interrupcion pendiente */
    bk4819_write(0x02, 0);
    uint16_t irq = bk4819_read(0x02);
    if (!(irq & 0x0800)) return -1;          /* DTMF/5-tone encontrado */
    return map[(bk4819_read(0x0B) >> 8) & 0xF];
}

/* ---- Escaneo CTCSS/DCS ---- */
void bk4819_tone_scan_start(void)
{
    bk4819_write(0x51, 0x0300);
    delay_ms(200);
    bk4819_rx_on();
}

int bk4819_tone_scan_result(int *type, uint16_t *code)
{
    uint16_t r69 = bk4819_read(0x69);
    if (!(r69 & 0x8000)) {
        uint32_t w = ((uint32_t)(r69 & 0xFFF) << 12) | (bk4819_read(0x6A) & 0xFFF);
        for (int inv = 0; inv < 2; inv++)
            for (int i = 0; i < DCS_COUNT; i++) {
                uint32_t g = dcs_golay_word(dcs_codes[i]);
                if (inv) g ^= 0x7FFFFF;
                if (g == w) { *type = inv ? 3 : 2; *code = dcs_codes[i]; return 1; }
            }
    }
    uint16_t r68 = bk4819_read(0x68);
    if (!(r68 & 0x8000)) {
        uint16_t t = (uint16_t)((r68 & 0x1FFF) * 200u / 0x19Du);
        int best = -1, bd = 1000;
        for (int i = 0; i < CTCSS_COUNT; i++) {
            int dd = t > ctcss_tones[i] ? t - ctcss_tones[i] : ctcss_tones[i] - t;
            if (dd < bd) { bd = dd; best = i; }
        }
        if (best >= 0 && bd < 10) { *type = 1; *code = ctcss_tones[best]; return 1; }
    }
    return 0;
}

/* ---- Contador de frecuencia ---- */
void bk4819_freq_counter_start(void) { bk4819_write(0x32, 0x0B01); }
void bk4819_freq_counter_stop(void)  { bk4819_write(0x32, 0x0000); }
int bk4819_freq_counter_read(uint32_t *f, int16_t cal)
{
    uint16_t hi = bk4819_read(0x0D);
    if (!(hi & 0x8000)) return 0;
    uint32_t v = ((uint32_t)(hi & 0x7FF) << 16) | bk4819_read(0x0E);
    *f = (uint32_t)((int32_t)v - cal);
    return 1;
}

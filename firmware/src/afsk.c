#include "afsk.h"
#include <string.h>
#include <stdio.h>

/* ===================== AX.25 ===================== */
uint16_t ax25_fcs(const uint8_t *d, int n)
{
    uint16_t crc = 0xFFFF;
    while (n--) {
        crc ^= *d++;
        for (int i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ 0x8408 : crc >> 1;
    }
    return (uint16_t)~crc;
}

static int put_call(uint8_t *o, const char *call, int last)
{
    char c[7] = "      "; int ssid = 0, i = 0;
    while (*call && *call != '-' && *call != ',' && i < 6) c[i++] = *call++;
    while (*call && *call != '-' && *call != ',') call++;
    if (*call == '-') { call++; while (*call >= '0' && *call <= '9') ssid = ssid * 10 + (*call++ - '0'); }
    for (i = 0; i < 6; i++) o[i] = (uint8_t)(c[i] << 1);
    o[6] = (uint8_t)(0x60 | ((ssid & 0xF) << 1) | (last ? 1 : 0));
    return 7;
}

int ax25_build_ui(uint8_t *out, const char *dest, const char *src, const char *path, const char *info)
{
    int n = 0;
    int has_path = path && *path;
    n += put_call(out + n, dest, 0);
    n += put_call(out + n, src, !has_path);
    while (has_path && *path) {
        const char *next = strchr(path, ',');
        n += put_call(out + n, path, next == NULL);
        if (n > 7 * 10) break;
        path = next ? next + 1 : "";
    }
    out[n++] = 0x03; out[n++] = 0xF0;
    int il = (int)strlen(info);
    if (n + il > AX25_MAX_FRAME - 2) il = AX25_MAX_FRAME - 2 - n;
    memcpy(out + n, info, (size_t)il);
    return n + il;
}

/* ===================== Modulador ===================== */
/* Seno de 64 puntos, 8 bits */
static const uint8_t sine64[64] = {
    128,140,152,165,176,188,198,208,218,226,234,240,245,250,253,254,
    255,254,253,250,245,240,234,226,218,208,198,188,176,165,152,140,
    128,115,103, 90, 79, 67, 57, 47, 37, 29, 21, 15, 10,  5,  2,  1,
      0,  1,  2,  5, 10, 15, 21, 29, 37, 47, 57, 67, 79, 90,103,115,
};
#define PHASE_INC(f) ((uint32_t)(((uint64_t)(f) << 32) / AFSK_FS))

void afsk_tx_start(afsk_tx_t *t, const uint8_t *frame, int len)
{
    memset(t, 0, sizeof(*t));
    memcpy(t->frame, frame, (size_t)len);
    uint16_t fcs = ax25_fcs(frame, len);
    t->frame[len] = fcs & 0xFF; t->frame[len + 1] = fcs >> 8;
    t->len = (uint16_t)(len + 2);
    t->preamble_flags = 40;  /* ~270 ms de TXDelay */
    t->tail_flags = 3;
    t->state = 0; t->bit_idx = 8;
    t->cur_tone = 0;  /* 0 = mark 1200, 1 = space 2200 */
}

/* Siguiente bit a enviar (antes de NRZI) o -1 */
static int next_bit(afsk_tx_t *t)
{
    for (;;) {
        if (t->stuffing) { t->stuffing = 0; t->ones = 0; return 0; }
        if (t->bit_idx >= 8) {
            if (t->state == 0) {
                if (t->preamble_flags) { t->preamble_flags--; t->cur_byte = 0x7E; t->phase_bits = 1; }
                else { t->state = 1; continue; }
            } else if (t->state == 1) {
                if (t->byte_idx < t->len) { t->cur_byte = t->frame[t->byte_idx++]; t->phase_bits = 0; }
                else { t->state = 2; continue; }
            } else if (t->state == 2) {
                if (t->tail_flags) { t->tail_flags--; t->cur_byte = 0x7E; t->phase_bits = 1; }
                else return -1;
            }
            t->bit_idx = 0;
        }
        int b = (t->cur_byte >> t->bit_idx++) & 1;
        if (t->phase_bits) { t->ones = 0; return b; }   /* flags sin bit-stuffing */
        if (b) { if (++t->ones == 5) t->stuffing = 1; }
        else t->ones = 0;
        return b;
    }
}

int afsk_tx_next_tone(afsk_tx_t *t)
{
    int b = next_bit(t);
    if (b < 0) return -1;
    if (b == 0) t->cur_tone ^= 1;
    return t->cur_tone;
}

int afsk_tx_next(afsk_tx_t *t)
{
    if (t->sample_in_bit == 0) {
        int b = next_bit(t);
        if (b < 0) return -1;
        if (b == 0) t->cur_tone ^= 1;                    /* NRZI: 0 = cambio de tono */
    }
    t->sample_in_bit = (t->sample_in_bit + 1) % (AFSK_FS / 1200);
    t->phase += t->cur_tone ? PHASE_INC(2200) : PHASE_INC(1200);
    return sine64[t->phase >> 26];
}

/* ===================== Demodulador ===================== */
/* Demodulador por correlacion con retardo (delay-line) de 1 bit / 2:
 * producto s[n]*s[n-D] con D tal que 1200 y 2200 Hz den signos opuestos. */
#define SPB (AFSK_FS / 1200)     /* muestras por bit = 8 */
#define DELAY 4                  /* 4 muestras a 9600 Hz ~ 417 us */

void afsk_rx_init(afsk_rx_t *r, ax25_rx_cb cb)
{
    afsk_rx_init_rate(r, cb, AFSK_FS);
}

void afsk_rx_init_rate(afsk_rx_t *r, ax25_rx_cb cb, uint32_t fs)
{
    memset(r, 0, sizeof(*r));
    r->cb = cb;
    r->step = (uint32_t)(((uint64_t)1200 << 32) / fs);
}

static void rx_bit(afsk_rx_t *r, int bit)
{
    /* NRZI decode */
    int b = (bit == r->last_bit);
    r->last_bit = (uint8_t)bit;
    r->shift = (uint8_t)((r->shift >> 1) | (b << 7));
    if (r->shift == 0x7E) {                       /* flag */
        if (r->in_frame && r->len >= 17) {
            uint16_t fcs = ax25_fcs(r->buf, r->len - 2);
            if ((r->buf[r->len - 2] | (r->buf[r->len - 1] << 8)) == fcs && r->cb)
                r->cb(r->buf, r->len - 2);
        }
        r->in_frame = 1; r->len = 0; r->nbits = 0; r->ones = 0;
        return;
    }
    if (!r->in_frame) return;
    if (b) {
        if (++r->ones > 6) { r->in_frame = 0; return; }   /* abortar */
    } else {
        if (r->ones == 5) { r->ones = 0; return; }        /* bit de relleno */
        r->ones = 0;
    }
    r->acc = (uint8_t)((r->acc >> 1) | (b << 7));
    if (++r->nbits == 8) {
        if (r->len < AX25_MAX_FRAME) r->buf[r->len++] = r->acc;
        else r->in_frame = 0;
        r->nbits = 0;
    }
}

/* Nivel digital ya demodulado (mark/space) muestreado a la tasa de init */
void afsk_rx_level(afsk_rx_t *r, int bit)
{
    /* PLL de reloj de bit: fase de 32 bits; se muestrea al desbordar y en
     * cada transicion se acerca la fase a la mitad del bit. */
    uint32_t prev = r->pll;
    r->pll += r->step;
    if (r->pll < prev) rx_bit(r, bit);
    if (bit != r->lastraw) {
        int32_t err = (int32_t)(r->pll - 0x80000000u);
        r->pll -= (uint32_t)(err / 4);
    }
    r->lastraw = (uint8_t)bit;
}

/* Audio (muestras con signo a AFSK_FS): demodulador por linea de retardo */
void afsk_rx_sample(afsk_rx_t *r, int16_t s)
{
    int16_t d = r->hist[(uint8_t)(r->hpos - DELAY) & 15];
    r->hist[r->hpos++ & 15] = s;
    int32_t prod = ((int32_t)s * d) >> 8;
    r->lpf += (prod - r->lpf) >> 2;
    afsk_rx_level(r, r->lpf > 0);
}

/* ===================== APRS ===================== */
int aprs_position(char *out, int32_t lat_e7, int32_t lon_e7, char tab, char sym, const char *comment)
{
    char ns = lat_e7 < 0 ? 'S' : 'N', ew = lon_e7 < 0 ? 'W' : 'E';
    if (lat_e7 < 0) lat_e7 = -lat_e7;
    if (lon_e7 < 0) lon_e7 = -lon_e7;
    int lat_d = lat_e7 / 10000000, lon_d = lon_e7 / 10000000;
    int lat_m = (int)((int64_t)(lat_e7 % 10000000) * 6000 / 10000000); /* minutos*100 */
    int lon_m = (int)((int64_t)(lon_e7 % 10000000) * 6000 / 10000000);
    return sprintf(out, "!%02d%02d.%02d%c%c%03d%02d.%02d%c%c%s", lat_d, lat_m / 100, lat_m % 100, ns,
                   tab, lon_d, lon_m / 100, lon_m % 100, ew, sym, comment ? comment : "");
}

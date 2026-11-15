#pragma once
#include <stdint.h>
/* Modem AFSK Bell 202 (1200 baudios, 1200/2200 Hz) + tramas AX.25 para APRS.
 * Independiente del hardware: el modulador entrega muestras y el demodulador
 * las recibe a AFSK_FS muestras por segundo.                                   */
#define AFSK_FS 9600u
#define AX25_MAX_FRAME 330

/* ---------- TX ---------- */
typedef struct {
    uint8_t frame[AX25_MAX_FRAME];
    uint16_t len;            /* bytes de la trama incluyendo FCS */
    uint16_t preamble_flags, tail_flags;
    /* estado interno */
    uint16_t byte_idx; uint8_t bit_idx, ones, phase_bits, state, cur_tone;
    uint32_t phase, sample_in_bit;
    uint8_t cur_byte, stuffing;
} afsk_tx_t;

/* Construye una trama UI APRS. path puede ser NULL o "WIDE1-1,WIDE2-1" */
int ax25_build_ui(uint8_t *out, const char *dest, const char *src, const char *path,
                  const char *info);
uint16_t ax25_fcs(const uint8_t *d, int n);
void afsk_tx_start(afsk_tx_t *t, const uint8_t *frame_wo_fcs, int len);
/* Devuelve muestra de 8 bits (centrada en 128) o -1 al terminar */
int afsk_tx_next(afsk_tx_t *t);
/* Tono del siguiente bit tras NRZI: 0 = mark 1200 Hz, 1 = space 2200 Hz, -1 = fin */
int afsk_tx_next_tone(afsk_tx_t *t);

/* ---------- RX ---------- */
typedef void (*ax25_rx_cb)(const uint8_t *frame, int len);  /* trama sin FCS, FCS ya validado */
typedef struct {
    int16_t hist[16]; uint8_t hpos;
    uint32_t pll, step; uint8_t last_bit, ones, shift, lastraw, acc;
    uint16_t nbits, len;
    uint8_t buf[AX25_MAX_FRAME]; uint8_t in_frame;
    int32_t lpf;
    ax25_rx_cb cb;
} afsk_rx_t;
void afsk_rx_init(afsk_rx_t *r, ax25_rx_cb cb);
void afsk_rx_init_rate(afsk_rx_t *r, ax25_rx_cb cb, uint32_t fs);
void afsk_rx_sample(afsk_rx_t *r, int16_t s);   /* muestra de audio con signo, sin DC */
void afsk_rx_level(afsk_rx_t *r, int level);   /* nivel ya demodulado (modem externo) */

/* Formato APRS: posicion sin comprimir "!DDMM.mmN/DDDMM.mmW>comentario" */
int aprs_position(char *out, int32_t lat_e7, int32_t lon_e7, char sym_table, char sym,
                  const char *comment);

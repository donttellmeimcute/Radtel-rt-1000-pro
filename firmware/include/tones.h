#pragma once
#include <stdint.h>
/* Tablas estandar CTCSS (50 tonos, en 0.1 Hz) y DCS (104 codigos, octal) */
#define CTCSS_COUNT 50
#define DCS_COUNT 104
extern const uint16_t ctcss_tones[CTCSS_COUNT];
extern const uint16_t dcs_codes[DCS_COUNT];
/* Palabra Golay(23,12) de 23 bits para un codigo DCS (octal como entero, p.ej. 0023) */
uint32_t dcs_golay_word(uint16_t code);
/* Pares de frecuencias DTMF (Hz) para '0'-'9','A'-'D','*','#'. Devuelve 0 si invalido */
int dtmf_freqs(char c, uint16_t *low, uint16_t *high);

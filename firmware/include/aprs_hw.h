#pragma once
#include <stdint.h>
#include "afsk.h"
/* Modem APRS por GPIO + TMR3 (26.373 kHz), igual que el hardware original:
 * TX = onda cuadrada 1200/2200 Hz en PA2 o PF1 (segun variante PB8),
 * RX = nivel ya demodulado por el hardware en PA2.                          */
void aprs_hw_init(ax25_rx_cb cb);
void aprs_hw_rx_enable(int on);
int  aprs_hw_tx(const uint8_t *frame, int len);  /* inicia TX (no bloqueante) */
int  aprs_hw_tx_busy(void);

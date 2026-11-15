#pragma once
#include <stdint.h>
#include "channel.h"

typedef enum { RS_IDLE, RS_RX, RS_TX } radio_state_t;

typedef struct {
    radio_state_t state;
    uint8_t  sq_open;        /* senal recibida (squelch abierto) */
    uint8_t  monitor;        /* forzar apertura del squelch */
    int16_t  rssi_dbm;
    uint8_t  noise;
    uint8_t  band;           /* banda de calibracion 0..14, 0xFF fuera de rango */
    uint8_t  cal_ok;         /* 1 si se encontro calibracion de fabrica */
    uint32_t tx_start_ms;
    channel_t cur;           /* canal sintonizado */
} radio_t;
extern radio_t g_radio;

void radio_init(void);
int  radio_band_of(uint32_t f10hz);               /* -1 fuera de cobertura */
int  radio_tx_allowed(uint32_t f10hz);
void radio_tune(const channel_t *c);              /* RX en el canal */
int  radio_tx_start(void);                        /* 0 ok, <0 no permitido */
void radio_tx_stop(void);
void radio_poll(void);                            /* squelch, RSSI, audio (llamar ~cada 10 ms) */
void radio_set_volume(uint8_t v);
void radio_apply_squelch(void);
void radio_sleep(void);
void radio_wake(void);

#pragma once
#include <stdint.h>
/* Codigos de tecla */
enum {
    KEY_NONE = 0xFF,
    KEY_0 = 0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_MENU = 0x0A,   /* fila inferior del teclado: codigos 0x0A-0x0D del original */
    KEY_UP = 0x0B,
    KEY_DOWN = 0x0C,
    KEY_EXIT = 0x0D,
    KEY_STAR = 0x0E,
    KEY_HASH = 0x0F,
    KEY_SIDE1 = 0x16, KEY_SIDE2 = 0x17, KEY_ALARM = 0x18,
    KEY_PTT = 0x20,
};
typedef enum { EV_NONE, EV_PRESS, EV_LONG, EV_RELEASE, EV_REPEAT } key_ev_t;
typedef struct { uint8_t key; key_ev_t ev; } key_event_t;

void keys_init(void);
void keys_tick_1ms(void);              /* llamado desde la interrupcion de 1 ms */
int  keys_get(key_event_t *e);          /* 1 si habia evento */
int  keys_ptt_pressed(void);
int  encoder_take(void);                /* pasos acumulados del encoder (+/-) */
void keys_set_sidekey_adc(uint16_t raw);

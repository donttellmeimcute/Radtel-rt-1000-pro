#pragma once
#include <stdint.h>
#include "lcd.h"
#include "keys.h"

#define UI_BG      C_BLACK
#define UI_FG      C_WHITE
#define UI_ACCENT  RGB(0, 170, 255)
#define UI_DIM     C_GRAY
#define UI_SEL_BG  RGB(0, 70, 140)
#define UI_WARN    C_ORANGE
#define STATUS_H   20

void ui_status_bar(int force);
void ui_title(const char *t);
void ui_footer(const char *t);
void ui_message(const char *l1, const char *l2, uint16_t color);  /* cuadro centrado */
void ui_clear_body(void);
/* Lista desplazable generica: dibuja items [top, top+rows) con el seleccionado resaltado */
typedef const char *(*ui_item_fn)(int idx, char *buf);   /* devuelve texto del item */
typedef const char *(*ui_value_fn)(int idx, char *buf);  /* valor a la derecha (puede ser NULL) */
void ui_list(int count, int sel, int *top, ui_item_fn item, ui_value_fn value);
void fmt_freq(char *out, uint32_t f10hz);                /* "146.52000" */
void fmt_tone(char *out, uint16_t t);                    /* "OFF", "88.5", "D023N" */
void ui_rssi_bar(int x, int y, int w, int dbm);

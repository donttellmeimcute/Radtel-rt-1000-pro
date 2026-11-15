#pragma once
#include <stdint.h>
#include "layout.h"
/* Registro de canal/VFO de 0x30 bytes, mismo formato que el firmware original
 * (para que el CPS oficial siga sirviendo). Ver docs/storage_comms.md 1.4.  */
typedef struct __attribute__((packed)) {
    uint8_t  b0;        /* b7:6 tipo: 01 = valido (analogico), 11 = vacio */
    uint8_t  b1;        /* b3:0 scrambler 0..15 */
    uint8_t  b2;        /* b7:6 potencia: 1 = alta, otro = baja */
    uint8_t  b3;        /* b2:0 cola de tono (STE) 0..4; b4:3 busy-lock */
    uint8_t  b4;        /* b5:4 modulacion 0 FM 1 AM 2 USB 3 LSB; b7:6 ancho 0 = ancho */
    uint32_t rx;        /* +0x05 frecuencia RX en unidades de 10 Hz */
    uint32_t tx;        /* +0x09 frecuencia TX */
    uint16_t rx_tone;   /* +0x0D b15:12 tipo (0 nada,1 CTCSS,2 DCS-N,3 DCS-I), b11:0 codigo */
    uint16_t tx_tone;   /* +0x0F */
    uint8_t  rsv11[0x20 - 0x11];
    char     name[16];  /* +0x20 ASCII, relleno 0xFF */
} channel_t;
_Static_assert(sizeof(channel_t) == 0x30, "channel_t debe medir 0x30");

enum { MOD_FM = 0, MOD_AM = 1, MOD_USB = 2, MOD_LSB = 3 };

static inline int  ch_valid(const channel_t *c) { return (c->b0 >> 6) == 1 && c->rx != 0xFFFFFFFFu && c->rx; }
static inline int  ch_mod(const channel_t *c) { return (c->b4 >> 4) & 3; }
static inline void ch_set_mod(channel_t *c, int m) { c->b4 = (uint8_t)((c->b4 & ~0x30) | ((m & 3) << 4)); }
static inline int  ch_narrow(const channel_t *c) { return (c->b4 >> 6) != 0; }
static inline void ch_set_narrow(channel_t *c, int n) { c->b4 = (uint8_t)((c->b4 & 0x3F) | (n ? 0x40 : 0)); }
static inline int  ch_high_power(const channel_t *c) { return (c->b2 >> 6) == 1; }
static inline void ch_set_high_power(channel_t *c, int h) { c->b2 = (uint8_t)((c->b2 & 0x3F) | (h ? 0x40 : 0x80)); }
static inline int  ch_scrambler(const channel_t *c) { return c->b1 & 0x0F; }
static inline void ch_set_scrambler(channel_t *c, int s) { c->b1 = (uint8_t)((c->b1 & 0xF0) | (s & 0xF)); }
static inline int  ch_ste(const channel_t *c) { return c->b3 & 7; }
static inline int  ch_busy_lock(const channel_t *c) { return ((c->b3 >> 3) & 3) == 2; }
static inline int  tone_type(uint16_t t) { return t == 0xFFFF ? 0 : (t >> 12) & 0xF; }
static inline uint16_t tone_code(uint16_t t) { return t & 0x0FFF; }
static inline uint16_t tone_make(int type, uint16_t code) { return type ? (uint16_t)((type << 12) | (code & 0xFFF)) : 0; }

void channel_default(channel_t *c, uint32_t freq);
int  channel_load(uint16_t idx, channel_t *c);      /* 1 si valido */
void channel_save(uint16_t idx, const channel_t *c);
void channel_erase(uint16_t idx);
int  channel_next_valid(int from, int dir);          /* -1 si no hay */
void channel_name(const channel_t *c, char *out17);

/* Zonas: 250 x 0x208 (u16 canal actual, nombre 16 B en +4, lista u16[250] en +0x14) */
#define ZONE_MAX_CH 250
int  zone_load_name(uint8_t z, char *out17);         /* 1 si la zona tiene canales */
int  zone_channel(uint8_t z, uint16_t pos);          /* indice de canal o -1 */

#pragma once
#include <stdint.h>
/* Receptor Si4732/Si4735 en I2C2, direccion 0x11, habilitacion en PA8 */
enum { SI_MODE_OFF, SI_MODE_FM, SI_MODE_AM, SI_MODE_LSB, SI_MODE_USB };
typedef struct { uint8_t rssi, snr, valid; uint16_t freq; } si_status_t;

int  si473x_start(int mode, uint16_t freq);   /* FM: 10 kHz; AM/SSB: kHz */
void si473x_stop(void);
void si473x_tune(uint16_t freq);
void si473x_seek(int up);
void si473x_status(si_status_t *s);
void si473x_set_volume(uint8_t v);              /* 0..63 */
void si473x_set_agc(uint8_t gain);              /* 0 = AGC automatico */
void si473x_set_ssb_bfo(int16_t hz);
void si473x_set_bandwidth(uint8_t idx);         /* AM: 0..6, SSB: 0..5 */
void si473x_set_band_limits(uint16_t lo, uint16_t hi, uint16_t spacing);
/* Parche SSB: el firmware no lo incluye (es propiedad de Silicon Labs).
 * Se carga desde la flash SPI externa si el usuario lo instalo ahi. */
int  si473x_ssb_patch_available(void);

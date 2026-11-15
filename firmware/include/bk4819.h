#pragma once
#include <stdint.h>
/* Driver BK4819 (bus 3 hilos bit-bang: SCN=PC8, SCK=PC9, SDA=PC7) */
void     bk4819_init(void);
void     bk4819_write(uint8_t reg, uint16_t val);
uint16_t bk4819_read(uint8_t reg);
void     bk4819_set_freq(uint32_t f10hz, int16_t cal_offset);   /* f en unidades de 10 Hz */
void     bk4819_rx_on(void);
void     bk4819_sleep(void);
void     bk4819_set_af(uint8_t af);           /* 0 mute, 1 FM, 7 AM, 5 SSB (REG_47) */
void     bk4819_set_volume(uint8_t vol);      /* 0..15 -> REG_48 */
void     bk4819_set_bandwidth(int wide);
void     bk4819_set_agc_default(void);
void     bk4819_set_squelch(uint8_t glitch, uint16_t noise4f, uint16_t rssi78);
/* Subtonos: type 0 nada, 1 CTCSS (code = Hz*10), 2 DCS normal, 3 DCS invertido (code octal) */
void     bk4819_set_subtone(int type, uint16_t code);
void     bk4819_tail_tone(void);
void     bk4819_set_mic_gain(uint8_t g);      /* 0..31 */
void     bk4819_set_deviation(uint16_t reg40);
void     bk4819_set_pa(uint8_t bias6);        /* 0 = PA apagado */
void     bk4819_tx_on(int with_tone);
void     bk4819_set_gpio(uint8_t mask, uint8_t value); /* bits bajos de REG_33 */
uint16_t bk4819_rssi_raw(void);               /* REG_67 & 0x1FF */
int      bk4819_rssi_dbm(void);
uint8_t  bk4819_noise(void);
int      bk4819_squelch_open(void);            /* REG_0C bit1 */
void     bk4819_scrambler(uint8_t n);          /* 0 = apagado */
void     bk4819_vox(int on);
void     bk4819_set_filter_table(uint8_t idx);
/* DTMF */
void     bk4819_dtmf_tx_enable(void);
void     bk4819_dtmf_tone(char c);
void     bk4819_beep(uint16_t hz, int on);
void     bk4819_dtmf_rx_enable(uint8_t thr);
int      bk4819_dtmf_rx_get(void);             /* devuelve caracter o -1 */
/* Escaneo de subtonos */
void     bk4819_tone_scan_start(void);
int      bk4819_tone_scan_result(int *type, uint16_t *code);
/* Contador de frecuencia */
void     bk4819_freq_counter_start(void);
int      bk4819_freq_counter_read(uint32_t *f10hz, int16_t cal_offset);
void     bk4819_freq_counter_stop(void);

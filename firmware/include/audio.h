#pragma once
#include <stdint.h>
/* Fuentes que piden el amplificador del altavoz (PB2) */
enum { AMP_VOICE = 1, AMP_BEEP = 2, AMP_RX = 4, AMP_TONE = 8, AMP_BCAST = 16 };
void audio_init(void);
void audio_amp_request(uint8_t src, int on);
void audio_beep(uint16_t hz, uint16_t ms);     /* no bloqueante */
void audio_tone(uint16_t hz);                  /* tono continuo, 0 = apagar */
void audio_tick_1ms(void);
/* Mensajes de voz (PCM 8 bits, 8 kHz) desde la flash SPI */
void audio_voice_play(uint8_t clip);
int  audio_voice_busy(void);
void audio_voice_stop(void);

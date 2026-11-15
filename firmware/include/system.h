#pragma once
#include <stdint.h>
/* El PLL se configura como el original (HICK*96/8); los temporizadores del
 * firmware original demuestran que el reloj efectivo es 48 MHz. */
#define SYSCLK_HZ 48000000u
#define APB1_HZ (SYSCLK_HZ / 2)
#define APB2_HZ SYSCLK_HZ
#define TMR_HZ  SYSCLK_HZ
extern volatile uint32_t g_ticks_ms;
void system_clock_init(void);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);

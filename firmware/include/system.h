#pragma once
#include <stdint.h>
#define SYSCLK_HZ 96000000u
extern volatile uint32_t g_ticks_ms;
void system_clock_init(void);
void delay_ms(uint32_t ms);
void delay_us(uint32_t us);

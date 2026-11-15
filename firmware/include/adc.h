#pragma once
#include <stdint.h>
void     adc_init(void);
uint16_t adc_read(uint8_t ch);
uint16_t battery_mv(void);   /* tension de bateria en mV */
int      battery_percent(uint16_t mv);

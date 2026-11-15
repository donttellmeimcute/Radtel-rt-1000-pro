#pragma once
#include <stdint.h>
void i2c2_init(void);
int  i2c2_write(uint8_t addr7, const uint8_t *buf, int len);
int  i2c2_read(uint8_t addr7, uint8_t *buf, int len);

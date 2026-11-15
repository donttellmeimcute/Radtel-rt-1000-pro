#pragma once
#include <stdint.h>
void     spiflash_init(void);
uint16_t spiflash_id(void);
void     spiflash_read(uint32_t addr, void *dst, uint32_t len);
void     spiflash_erase_sector(uint32_t addr);          /* 4 KB */
void     spiflash_program(uint32_t addr, const void *src, uint32_t len); /* sin borrar */
/* Escritura segura: lee-borra-mezcla-reescribe sector a sector */
void     spiflash_write(uint32_t addr, const void *src, uint32_t len);

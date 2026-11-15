#include "cps.h"
#include "uart.h"
#include "spiflash.h"
#include "layout.h"
#include "system.h"
#include <string.h>

#define ACK 0x06

static uint8_t buf[1032];
static uint16_t len, need;
static uint8_t active, ended;
static uint32_t last_rx;

static const struct { uint8_t cmd; uint32_t base; uint16_t sectors; } regions[] = {
    {0x40, 0x000000, 1},  {0x90, 0x002000, 1},  {0x91, 0x004000, 12}, {0x92, 0x01C000, 1},
    {0x93, 0x01E000, 32}, {0x94, 0x05E000, 52}, {0x95, 0x0C6000, 5},  {0x96, 0x0D0000, 3},
    {0x97, 0x0D6000, 1},  {0x98, 0x0F0000, 1},  {0x99, 0x0FE000, 1},  {0x9A, 0x100000, 38},
    {0x9C, 0x14C000, 24}, {0x9D, 0x164000, 36}, {0x9E, 0x188000, 16}, {0x9F, 0x198000, 4},
    {0xA0, 0x19C000, 2},  {0xA1, 0x19E000, 421},{0xA3, 0x352000, 158},{0xA5, 0x3F0000, 16},
};

static uint8_t sum8(const uint8_t *p, int n) { uint8_t s = 0; while (n--) s += *p++; return s; }

void cps_init(void) { len = 0; need = 0; active = 0; }

static int frame_len(uint8_t c)
{
    if (c == 0x34) return 5;
    if (c == 0x35) return 9;
    if (c == 0x52) return 4;
    if (c == 0x40 || (c >= 0x90 && c <= 0xA5)) return 1028;
    return 0;
}

static void handle_write(void)
{
    uint8_t cmd = buf[0];
    uint16_t blk = (uint16_t)(buf[1] << 8 | buf[2]);
    const uint8_t *data = buf + 3;
    if (cmd == 0x9B || cmd == 0xA2) {                 /* escritura cruda sin borrado */
        spiflash_program(blk * 0x400u, data, 1024);
    } else if (cmd == 0xA4) {                         /* escritura cruda con borrado por sector */
        if ((blk & 3) == 0) spiflash_erase_sector(blk * 0x400u);
        spiflash_program(blk * 0x400u, data, 1024);
    } else {
        for (unsigned i = 0; i < sizeof(regions) / sizeof(regions[0]); i++) {
            if (regions[i].cmd != cmd) continue;
            if (blk >= regions[i].sectors * 4u) break;
            if (blk == 0)
                for (uint16_t s = 0; s < regions[i].sectors; s++)
                    spiflash_erase_sector(regions[i].base + s * 0x1000u);
            spiflash_program(regions[i].base + blk * 0x400u, data, 1024);
            break;
        }
    }
    uart_pc_putc(ACK);
}

static void handle_frame(void)
{
    uint8_t c = buf[0];
    if (sum8(buf, len - 1) != buf[len - 1]) { uart_pc_putc(0xFF); return; }
    if (c == 0x34) {
        uint8_t sub = buf[3];
        if (sub == 0x10) { active = 1; uart_pc_putc(ACK); }
        else if (sub == 0xEE) { active = 0; ended = 1; uart_pc_putc(ACK); }
        else uart_pc_putc(ACK);
    } else if (c == 0x52) {
        uint16_t blk = (uint16_t)(buf[1] << 8 | buf[2]);
        static uint8_t out[1028];
        out[0] = 0x52; out[1] = buf[1]; out[2] = buf[2];
        spiflash_read(blk * 0x400u, out + 3, 1024);
        uint8_t s = sum8(out, 1027);
        uart_pc_write(out, 1027);
        uart_pc_putc(s);
    } else if (c == 0x35) {
        spiflash_write(0x3FF404, buf + 4, 4);   /* ID del radio */
        uart_pc_putc(ACK);
    } else {
        handle_write();
    }
}

int cps_poll(void)
{
    int ch;
    while ((ch = uart_pc_getc()) >= 0) {
        last_rx = g_ticks_ms;
        if (len == 0) {
            need = (uint16_t)frame_len((uint8_t)ch);
            if (!need) continue;
        }
        buf[len++] = (uint8_t)ch;
        if (len == need) { handle_frame(); len = 0; }
    }
    if (len && (uint32_t)(g_ticks_ms - last_rx) > 500) len = 0;   /* trama incompleta */
    if (active && (uint32_t)(g_ticks_ms - last_rx) > 10000) { active = 0; ended = 1; }
    return active;
}

int cps_session_ended(void)
{
    if (!ended) return 0;
    ended = 0;
    return 1;
}

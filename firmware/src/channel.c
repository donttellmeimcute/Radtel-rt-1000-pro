#include "channel.h"
#include "spiflash.h"
#include "layout.h"
#include <string.h>

void channel_default(channel_t *c, uint32_t f)
{
    memset(c, 0, sizeof(*c));
    memset(c->rsv11, 0, sizeof(c->rsv11));
    c->b0 = 0x40;
    c->b2 = 0x40;
    c->rx = c->tx = f;
    memset(c->name, 0xFF, sizeof(c->name));
}

int channel_load(uint16_t idx, channel_t *c)
{
    if (idx >= CHANNEL_COUNT) return 0;
    spiflash_read(FLASH_CHANNELS + idx * (uint32_t)CHANNEL_SIZE, c, CHANNEL_SIZE);
    return ch_valid(c);
}

void channel_save(uint16_t idx, const channel_t *c)
{
    if (idx >= CHANNEL_COUNT) return;
    spiflash_write(FLASH_CHANNELS + idx * (uint32_t)CHANNEL_SIZE, c, CHANNEL_SIZE);
}

void channel_erase(uint16_t idx)
{
    uint8_t ff[CHANNEL_SIZE];
    memset(ff, 0xFF, sizeof(ff));
    channel_save(idx, (const channel_t *)ff);
}

int channel_next_valid(int from, int dir)
{
    channel_t c;
    for (int i = 1; i <= CHANNEL_COUNT; i++) {
        int n = (from + dir * i + CHANNEL_COUNT * 2) % CHANNEL_COUNT;
        if (channel_load((uint16_t)n, &c)) return n;
    }
    return -1;
}

void channel_name(const channel_t *c, char *out)
{
    int n = 0;
    for (; n < 16 && c->name[n] && (uint8_t)c->name[n] != 0xFF; n++) out[n] = c->name[n];
    while (n > 0 && out[n - 1] == ' ') n--;
    out[n] = 0;
}

int zone_load_name(uint8_t z, char *out)
{
    uint8_t buf[16];
    if (z >= ZONE_COUNT) return 0;
    spiflash_read(FLASH_ZONES + z * (uint32_t)ZONE_SIZE + 4, buf, 16);
    int n = 0;
    for (; n < 16 && buf[n] && buf[n] != 0xFF; n++) out[n] = (char)buf[n];
    out[n] = 0;
    return zone_channel(z, 0) >= 0;
}

int zone_channel(uint8_t z, uint16_t pos)
{
    uint8_t b[2];
    if (z >= ZONE_COUNT || pos >= ZONE_MAX_CH) return -1;
    spiflash_read(FLASH_ZONES + z * (uint32_t)ZONE_SIZE + 0x14 + pos * 2u, b, 2);
    uint16_t v = (uint16_t)(b[0] | b[1] << 8);
    return v < CHANNEL_COUNT ? v : -1;
}

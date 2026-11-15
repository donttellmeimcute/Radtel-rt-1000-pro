/* Flash NOR SPI (W25Qxx) en SPI2: SCK=PB13, MISO=PB14, MOSI=PB15 (AF5), CS=PB12 */
#include "spiflash.h"
#include "board.h"
#include <string.h>

static uint8_t addr4;     /* 1 si el chip usa direcciones de 4 bytes (>=32 MB) */
static uint8_t sector_buf[4096];

static uint8_t xfer(uint8_t v)
{
    while (!(SPI2->sts & SPI_STS_TDBE)) { }
    SPI2->dt = v;
    while (!(SPI2->sts & SPI_STS_RDBF)) { }
    return (uint8_t)SPI2->dt;
}
static inline void cs_low(void)  { gpio_clr(PIN_FLASH_CS); }
static inline void cs_high(void) { while (SPI2->sts & SPI_STS_BF) { } gpio_set(PIN_FLASH_CS); }

static void send_addr(uint32_t a)
{
    if (addr4) xfer((uint8_t)(a >> 24));
    xfer((uint8_t)(a >> 16)); xfer((uint8_t)(a >> 8)); xfer((uint8_t)a);
}

static void wait_busy(void)
{
    cs_low(); xfer(0x05);
    while (xfer(0xFF) & 1) { }
    cs_high();
}

static void wren(void) { cs_low(); xfer(0x06); cs_high(); }

uint16_t spiflash_id(void)
{
    cs_low(); xfer(0x90); xfer(0); xfer(0); xfer(0);
    uint16_t id = (uint16_t)(xfer(0xFF) << 8);
    id |= xfer(0xFF);
    cs_high();
    return id;
}

void spiflash_init(void)
{
    CRM_AHBEN1 |= CRM_AHBEN1_GPIOB;
    CRM_APB1EN |= CRM_APB1EN_SPI2;
    gpio_set(PIN_FLASH_CS);
    gpio_mode(PIN_FLASH_CS, GPIO_OUT, GPIO_PULLUP);
    gpio_af((pin_t){GPIOB, 13}, 5);
    gpio_af((pin_t){GPIOB, 14}, 5);
    gpio_af((pin_t){GPIOB, 15}, 5);
    SPI2->ctrl1 = 0;
    /* maestro, CS por software (swcsen|swcsil), div /2 (12 MHz), modo 0 */
    SPI2->ctrl1 = (1u << 9) | (1u << 8) | (1u << 2) | (0u << 3);
    SPI2->ctrl1 |= 1u << 6;
    uint8_t dev = spiflash_id() & 0xFF;
    if (dev == 0x18 || dev == 0x19) {
        addr4 = 1;
        cs_low(); xfer(0xB7); cs_high();
    }
}

void spiflash_read(uint32_t addr, void *dst, uint32_t len)
{
    uint8_t *d = dst;
    cs_low(); xfer(0x03); send_addr(addr);
    while (len--) *d++ = xfer(0xFF);
    cs_high();
}

void spiflash_erase_sector(uint32_t addr)
{
    wren();
    cs_low(); xfer(0x20); send_addr(addr & ~0xFFFu); cs_high();
    wait_busy();
}

void spiflash_program(uint32_t addr, const void *src, uint32_t len)
{
    const uint8_t *s = src;
    while (len) {
        uint32_t n = 256 - (addr & 0xFF);
        if (n > len) n = len;
        wren();
        cs_low(); xfer(0x02); send_addr(addr);
        for (uint32_t i = 0; i < n; i++) xfer(s[i]);
        cs_high();
        wait_busy();
        addr += n; s += n; len -= n;
    }
}

void spiflash_write(uint32_t addr, const void *src, uint32_t len)
{
    const uint8_t *s = src;
    while (len) {
        uint32_t sec = addr & ~0xFFFu, off = addr - sec;
        uint32_t n = 4096 - off;
        if (n > len) n = len;
        spiflash_read(sec, sector_buf, 4096);
        int need_erase = 0;
        for (uint32_t i = 0; i < n; i++)
            if ((sector_buf[off + i] & s[i]) != s[i]) { need_erase = 1; break; }
        if (memcmp(sector_buf + off, s, n)) {
            if (need_erase) {
                memcpy(sector_buf + off, s, n);
                spiflash_erase_sector(sec);
                spiflash_program(sec, sector_buf, 4096);
            } else {
                spiflash_program(addr, s, n);
            }
        }
        addr += n; s += n; len -= n;
    }
}

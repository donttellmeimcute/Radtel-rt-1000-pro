/* I2C2 maestro por sondeo: SCL=PB10, SDA=PB11 (AF4, drenador abierto). */
#include "i2c.h"
#include "gpio.h"
#include "system.h"

#define TIMEOUT_MS 20

static int wait_flag(uint32_t f)
{
    uint32_t t0 = g_ticks_ms;
    while (!(I2C2->sts & f)) {
        if (I2C2->sts & I2C_STS_ACKFAIL) { I2C2->clr = I2C_STS_ACKFAIL; return -1; }
        if ((uint32_t)(g_ticks_ms - t0) > TIMEOUT_MS) return -2;
    }
    return 0;
}

void i2c2_init(void)
{
    CRM_AHBEN1 |= CRM_AHBEN1_GPIOB;
    CRM_APB1EN |= CRM_APB1EN_I2C2;
    pin_t scl = {GPIOB, 10}, sda = {GPIOB, 11};
    gpio_af(scl, 4); gpio_af(sda, 4);
    GPIOB->omode |= (1u << 10) | (1u << 11);
    I2C2->ctrl1 = 0;
    I2C2->clkctrl = 0x60E02E2E;      /* mismo valor que el firmware original */
    I2C2->ctrl1 = 1;                  /* i2cen */
}

/* ctrl2: saddr[9:0], dir bit10 (1=lectura), cnt[23:16], genstart bit13, genstop bit14, astopen bit25 */
static void start(uint8_t addr7, int rd, int len)
{
    I2C2->ctrl2 = (uint32_t)(addr7 << 1) | (rd ? (1u << 10) : 0) | ((uint32_t)len << 16)
                | (1u << 25) | (1u << 13);
}

static int finish(void)
{
    int r = wait_flag(I2C_STS_STOPF);
    I2C2->clr = I2C_STS_STOPF;
    return r;
}

int i2c2_write(uint8_t addr7, const uint8_t *buf, int len)
{
    start(addr7, 0, len);
    for (int i = 0; i < len; i++) {
        if (wait_flag(I2C_STS_TDIS)) { finish(); return -1; }
        I2C2->txdt = buf[i];
    }
    return finish();
}

int i2c2_read(uint8_t addr7, uint8_t *buf, int len)
{
    start(addr7, 1, len);
    for (int i = 0; i < len; i++) {
        if (wait_flag(I2C_STS_RDBF)) { finish(); return -1; }
        buf[i] = (uint8_t)I2C2->rxdt;
    }
    return finish();
}

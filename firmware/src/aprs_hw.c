#include "aprs_hw.h"
#include "board.h"
#include "system.h"

#define TICK_HZ 26373u    /* 48 MHz / 4 / 455 */
static afsk_tx_t tx;
static afsk_rx_t rx;
static volatile uint8_t mode;    /* 0 apagado, 1 RX, 2 TX */
static uint8_t variant;
static uint8_t bit_tick, half_tick, tone, level;

static pin_t tx_pin(void) { return variant ? PIN_APRS_DATA : PIN_APRS_TX_V0; }

void aprs_hw_init(ax25_rx_cb cb)
{
    CRM_AHBEN1 |= CRM_AHBEN1_GPIOA | CRM_AHBEN1_GPIOB | CRM_AHBEN1_GPIOC | CRM_AHBEN1_GPIOF;
    CRM_APB1EN |= CRM_APB1EN_TMR3;
    gpio_mode(PIN_HW_VARIANT, GPIO_IN, GPIO_PULLUP);
    delay_ms(1);
    variant = (uint8_t)gpio_read(PIN_HW_VARIANT);
    gpio_mode(PIN_APRS_DATA, GPIO_IN, GPIO_PULLUP);
    gpio_clr(PIN_APRS_MIC_SW);
    gpio_mode(PIN_APRS_MIC_SW, GPIO_OUT, GPIO_NOPULL);
    if (!variant) {
        gpio_clr(PIN_APRS_EN_V0);
        gpio_mode(PIN_APRS_EN_V0, GPIO_OUT, GPIO_NOPULL);
        gpio_clr(PIN_APRS_TX_V0);
        gpio_mode(PIN_APRS_TX_V0, GPIO_OUT, GPIO_NOPULL);
    }
    afsk_rx_init_rate(&rx, cb, TICK_HZ);
    TMR3->div = 3;
    TMR3->pr = 454;
    TMR3->iden = 1;
    nvic_enable(IRQ_TMR3, 0);
}

void aprs_hw_rx_enable(int on)
{
    if (mode == 2) return;
    mode = on ? 1 : 0;
    if (!variant) gpio_write(PIN_APRS_EN_V0, on);
    if (on) TMR3->ctrl1 = 1; else TMR3->ctrl1 = 0;
}

int aprs_hw_tx(const uint8_t *frame, int len)
{
    if (mode == 2) return -1;
    TMR3->ctrl1 = 0;
    afsk_tx_start(&tx, frame, len);
    if (variant) gpio_mode(PIN_APRS_DATA, GPIO_OUT, GPIO_NOPULL);
    gpio_set(PIN_APRS_MIC_SW);
    bit_tick = 0; half_tick = 0; tone = 0; level = 0;
    mode = 2;
    TMR3->ctrl1 = 1;
    return 0;
}

int aprs_hw_tx_busy(void) { return mode == 2; }

void TMR3_GLOBAL_IRQHandler(void)
{
    TMR3->ists = 0;
    if (mode == 1) {
        afsk_rx_level(&rx, gpio_read(PIN_APRS_DATA));
        return;
    }
    if (mode != 2) return;
    if (bit_tick == 0) {
        int t = afsk_tx_next_tone(&tx);
        if (t < 0) {
            mode = 0;
            TMR3->ctrl1 = 0;
            gpio_clr(tx_pin());
            gpio_clr(PIN_APRS_MIC_SW);
            if (variant) gpio_mode(PIN_APRS_DATA, GPIO_IN, GPIO_PULLUP);
            return;
        }
        tone = (uint8_t)t;
    }
    if (++bit_tick >= 22) bit_tick = 0;
    if (++half_tick >= (tone ? 6 : 11)) {
        half_tick = 0;
        level ^= 1;
        gpio_write(tx_pin(), level);
    }
}

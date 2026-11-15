#include "uart.h"
#include "board.h"
#include "system.h"

#define RXSZ 2048
typedef struct { volatile uint16_t h, t; uint8_t b[RXSZ]; } ring_t;
static ring_t pc_rx, gps_rx;

static inline void ring_put(ring_t *r, uint8_t c)
{
    uint16_t n = (uint16_t)((r->h + 1) % RXSZ);
    if (n != r->t) { r->b[r->h] = c; r->h = n; }
}
static inline int ring_get(ring_t *r)
{
    if (r->h == r->t) return -1;
    int c = r->b[r->t];
    r->t = (uint16_t)((r->t + 1) % RXSZ);
    return c;
}

void USART6_IRQHandler(void)
{
    while (USART6->sts & USART_STS_RDBF) ring_put(&pc_rx, (uint8_t)USART6->dt);
    USART6->sts &= ~(1u << 3);   /* limpiar overrun (rorerr) leyendo sts+dt */
}

void USART2_IRQHandler(void)
{
    while (USART2->sts & USART_STS_RDBF) ring_put(&gps_rx, (uint8_t)USART2->dt);
}

void uart_pc_init(uint32_t baud)
{
    CRM_AHBEN1 |= CRM_AHBEN1_GPIOA;
    CRM_APB2EN |= CRM_APB2EN_USART6;
    gpio_af(PIN_PC_TX, 8);
    gpio_af(PIN_PC_RX, 8);
    USART6->ctrl1 = 0;
    USART6->baudr = (APB2_HZ + baud / 2) / baud;
    USART6->ctrl1 = USART_CTRL1_UEN | USART_CTRL1_TEN | USART_CTRL1_REN | USART_CTRL1_RDBFIEN;
    nvic_enable(IRQ_USART6, 1);
}

void uart_pc_putc(uint8_t c)
{
    while (!(USART6->sts & USART_STS_TDBE)) { }
    USART6->dt = c;
}
void uart_pc_write(const void *buf, int len)
{
    const uint8_t *p = buf;
    while (len--) uart_pc_putc(*p++);
}
int uart_pc_getc(void) { return ring_get(&pc_rx); }
int uart_pc_available(void) { return pc_rx.h != pc_rx.t; }

void uart_gps_init(uint32_t baud)
{
    CRM_AHBEN1 |= CRM_AHBEN1_GPIOA | CRM_AHBEN1_GPIOB;
    CRM_APB1EN |= CRM_APB1EN_USART2;
    gpio_mode(PIN_GPS_PWR1, GPIO_OUT, GPIO_NOPULL);
    gpio_mode(PIN_GPS_PWR2, GPIO_OUT, GPIO_NOPULL);
    gpio_set(PIN_GPS_PWR1);
    gpio_set(PIN_GPS_PWR2);
    gpio_af(PIN_GPS_RX, 7);
    USART2->ctrl1 = 0;
    USART2->baudr = (APB1_HZ + baud / 2) / baud;
    USART2->ctrl1 = USART_CTRL1_UEN | USART_CTRL1_REN | USART_CTRL1_RDBFIEN;
    nvic_enable(IRQ_USART2, 2);
}

void uart_gps_stop(void)
{
    USART2->ctrl1 = 0;
    gpio_clr(PIN_GPS_PWR1);
    gpio_clr(PIN_GPS_PWR2);
}
int uart_gps_getc(void) { return ring_get(&gps_rx); }

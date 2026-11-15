/* Reloj del sistema: replica la configuracion del firmware original
 * (FUN_0802304c): HICK 8 MHz -> PLL (ns=96, ms=1, fr=/8) = 96 MHz,
 * AHB /1, APB2 /1, APB1 /2, LDO 1.3 V.                                         */
#include "at32f435.h"
#include "system.h"

volatile uint32_t g_ticks_ms;

void SysTick_Handler(void) { g_ticks_ms++; }

static void crm_autostep(int on)
{
    if (on) CRM_MISC2 |= 0x30; else CRM_MISC2 &= ~0x30u;
}

void system_clock_init(void)
{
    FLASH_PSR = 0x154;                         /* mismo valor que el original */
    CRM_APB1EN |= CRM_APB1EN_PWC;
    PWC_LDOOV = (PWC_LDOOV & ~3u) | 3u;        /* LDO 1.3 V */

    CRM_CTRL |= 1u;                            /* HICK on */
    while (!(CRM_CTRL & (1u << 1))) { }

    /* PLL: fuente HICK, ns=96, ms=1, fr=3 (/8) */
    CRM_MISC1 |= 1u << 12;
    uint32_t p = CRM_PLLCFG;
    p &= ~((1u << 22) | (0x1FFu << 6) | 0xFu | (7u << 16));
    p |= (96u << 6) | 1u | (3u << 16);
    CRM_PLLCFG = p;
    CRM_CTRL |= 1u << 24;                      /* PLL on */
    while (!(CRM_CTRL & (1u << 25))) { }

    crm_autostep(1);
    CRM_CFG = (CRM_CFG & ~(0xFu << 4)) | (0u << 4);    /* AHB /1  */
    CRM_CFG = (CRM_CFG & ~(7u << 13)) | (0u << 13);    /* APB2 /1 */
    CRM_CFG = (CRM_CFG & ~(7u << 10)) | (4u << 10);    /* APB1 /2 */
    CRM_CFG = (CRM_CFG & ~3u) | 2u;                    /* SCLK = PLL */
    while (((CRM_CFG >> 2) & 3u) != 2u) { }
    crm_autostep(0);

    SYST_RVR = SYSCLK_HZ / 1000 - 1;
    SYST_CVR = 0;
    SYST_CSR = 7;                                      /* HCLK, IRQ, enable */
}

void delay_ms(uint32_t ms)
{
    uint32_t t0 = g_ticks_ms;
    while ((uint32_t)(g_ticks_ms - t0) < ms) { }
}

void delay_us(uint32_t us)
{
    /* ~4 ciclos por iteracion a 96 MHz; aproximado, suficiente para bit-bang */
    volatile uint32_t n = us * (SYSCLK_HZ / 4000000u);
    while (n--) { }
}

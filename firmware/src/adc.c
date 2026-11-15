#include "adc.h"
#include "board.h"
#include "system.h"

void adc_init(void)
{
    CRM_APB2EN |= CRM_APB2EN_ADC1;
    gpio_mode(PIN_VBAT, GPIO_ANALOG, GPIO_NOPULL);
    gpio_mode(PIN_SIDEKEYS, GPIO_ANALOG, GPIO_NOPULL);
    /* Igual que el original (FUN_08002d64): divisor comun 2, muestreo 4 en ch7/ch12 */
    ADC_CCTRL = (ADC_CCTRL & ~(0xFu << 16)) | (2u << 16);
    ADC1->spt1 = 4u << ((ADC_CH_SIDEKEYS - 10) * 3);         /* canales 10-19 */
    ADC1->spt2 = 4u << (ADC_CH_VBAT * 3);                    /* canales 0-9 */
    ADC1->ctrl2 = 1u;                                        /* adcen */
    uint32_t t0 = g_ticks_ms;
    while (!(ADC1->sts & 0x40) && (uint32_t)(g_ticks_ms - t0) < 10) { }  /* rdy */
    ADC1->ctrl2 |= 1u << 3; while (ADC1->ctrl2 & (1u << 3)) { }   /* init calibracion */
    ADC1->ctrl2 |= 1u << 2; while (ADC1->ctrl2 & (1u << 2)) { }   /* calibracion */
}

uint16_t adc_read(uint8_t ch)
{
    ADC1->osq1 = 0;                        /* 1 conversion */
    ADC1->osq3 = ch & 0x1F;
    ADC1->sts = 0;
    ADC1->ctrl2 |= 1u << 30;               /* ocswtrg (bit 30, como FUN_08027084) */
    uint32_t t0 = g_ticks_ms;
    while (!(ADC1->sts & (1u << 1)))       /* occe: fin de conversion */
        if ((uint32_t)(g_ticks_ms - t0) > 5) return 0;
    return (uint16_t)(ADC1->odt & 0xFFF);
}

/* Formula del original: decivoltios = raw*3/51 + 2  ->  mV = raw*3000/51 + 200 */
uint16_t battery_mv(void)
{
    uint32_t raw = adc_read(ADC_CH_VBAT);
    return (uint16_t)(raw * 3000u / 51u + 200u);
}

/* Bateria Li-ion 2S (6.0 V vacia ... 8.4 V llena) */
int battery_percent(uint16_t mv)
{
    if (mv <= 6000) return 0;
    if (mv >= 8300) return 100;
    return (mv - 6000) * 100 / 2300;
}

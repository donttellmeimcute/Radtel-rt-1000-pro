#pragma once
/* Mapa de pines del Radtel RT-1000 Pro (AT32F435), obtenido por ingenieria
 * inversa del firmware original 260611. C = confirmado, P = probable.       */
#include "gpio.h"

/* Alimentacion */
#define PIN_POWER_HOLD   ((pin_t){GPIOF, 8})   /* C: mantener en alto o el radio se apaga */
/* LEDs (pines SWD reutilizados) */
#define PIN_LED_GREEN    ((pin_t){GPIOA, 13})  /* P: RX / ocupado */
#define PIN_LED_RED      ((pin_t){GPIOA, 14})  /* P: TX */
/* Audio */
#define PIN_SPK_AMP      ((pin_t){GPIOB, 2})   /* C: habilitacion del amplificador */
#define PIN_BEEP_PWM     ((pin_t){GPIOA, 6})   /* C: TMR13_CH1 AF9 */
/* Teclado matricial 4x4 (filas activas en bajo, columnas con pull-up) */
#define PIN_ROW0 ((pin_t){GPIOA, 9})
#define PIN_ROW1 ((pin_t){GPIOF, 6})
#define PIN_ROW2 ((pin_t){GPIOB, 6})
#define PIN_ROW3 ((pin_t){GPIOB, 7})
#define PIN_COL0 ((pin_t){GPIOC, 10})
#define PIN_COL1 ((pin_t){GPIOC, 11})
#define PIN_COL2 ((pin_t){GPIOC, 12})
#define PIN_COL3 ((pin_t){GPIOD, 2})
#define PIN_PTT          ((pin_t){GPIOC, 4})   /* C: activo en bajo */
#define PIN_ENC_A        ((pin_t){GPIOC, 1})   /* C */
#define PIN_ENC_B        ((pin_t){GPIOC, 0})   /* C */
#define PIN_SIDEKEYS     ((pin_t){GPIOC, 2})   /* C: escalera resistiva, ADC1_IN12 */
#define ADC_CH_SIDEKEYS  12
#define PIN_VBAT         ((pin_t){GPIOA, 7})   /* C: ADC1_IN7 */
#define ADC_CH_VBAT      7
/* RF */
#define PIN_RF_LOWBAND_TX ((pin_t){GPIOC, 15}) /* P: ruta TX 18-64 MHz */
#define PIN_RF_PATH      ((pin_t){GPIOF, 0})   /* P: 0 = VHF/UHF, 1 = HF/>=840 MHz */
#define PIN_RF_AM1       ((pin_t){GPIOB, 4})   /* P: alto en AM/SSB */
#define PIN_RF_AM2       ((pin_t){GPIOB, 1})   /* P: alto en AM/SSB RX */
#define PIN_SI47XX_EN    ((pin_t){GPIOA, 8})   /* P */
/* GPS (USART2 RX en PA3) */
#define PIN_GPS_PWR1     ((pin_t){GPIOB, 9})
#define PIN_GPS_PWR2     ((pin_t){GPIOB, 0})
#define PIN_GPS_RX       ((pin_t){GPIOA, 3})
/* PC / programacion (USART6) */
#define PIN_PC_TX        ((pin_t){GPIOA, 11})
#define PIN_PC_RX        ((pin_t){GPIOA, 12})
/* APRS (modem por GPIO, depende de la variante de hardware leida en PB8) */
#define PIN_HW_VARIANT   ((pin_t){GPIOB, 8})
#define PIN_APRS_DATA    ((pin_t){GPIOA, 2})   /* RX demodulado / TX (variante 1) */
#define PIN_APRS_TX_V0   ((pin_t){GPIOF, 1})   /* TX variante 0 */
#define PIN_APRS_EN_V0   ((pin_t){GPIOA, 0})
#define PIN_APRS_MIC_SW  ((pin_t){GPIOC, 3})   /* P: solo en alto durante TX APRS */
/* Flash SPI externa (SPI2) */
#define PIN_FLASH_CS     ((pin_t){GPIOB, 12})

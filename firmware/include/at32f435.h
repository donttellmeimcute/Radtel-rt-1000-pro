/* Definiciones minimas de registros AT32F435/437 escritas a mano a partir del
 * manual de referencia publico y de lo observado en el firmware original.
 * Solo se incluye lo que usa este firmware.                                    */
#pragma once
#include <stdint.h>

#define REG32(a) (*(volatile uint32_t *)(a))

/* ---- GPIO ---- */
typedef struct {
    volatile uint32_t cfgr;   /* 0x00 modo: 0 entrada, 1 salida, 2 alterna, 3 analogica */
    volatile uint32_t omode;  /* 0x04 0 push-pull, 1 open-drain */
    volatile uint32_t odrvr;  /* 0x08 fuerza de salida */
    volatile uint32_t pull;   /* 0x0C 0 nada, 1 pull-up, 2 pull-down */
    volatile uint32_t idt;    /* 0x10 entrada */
    volatile uint32_t odt;    /* 0x14 salida */
    volatile uint32_t scr;    /* 0x18 set (bits 0-15) / clear (bits 16-31) */
    volatile uint32_t wpr;    /* 0x1C */
    volatile uint32_t muxl;   /* 0x20 funcion alterna pines 0-7 */
    volatile uint32_t muxh;   /* 0x24 funcion alterna pines 8-15 */
    volatile uint32_t clr;    /* 0x28 clear */
    volatile uint32_t rsvd[4];
    volatile uint32_t hdrv;   /* 0x3C */
} gpio_t;

#define GPIOA ((gpio_t *)0x40020000)
#define GPIOB ((gpio_t *)0x40020400)
#define GPIOC ((gpio_t *)0x40020800)
#define GPIOD ((gpio_t *)0x40020C00)
#define GPIOE ((gpio_t *)0x40021000)
#define GPIOF ((gpio_t *)0x40021400)

/* ---- CRM (reloj y reset) ---- */
#define CRM_BASE      0x40023800
#define CRM_CTRL      REG32(CRM_BASE + 0x00)
#define CRM_PLLCFG    REG32(CRM_BASE + 0x04)
#define CRM_CFG       REG32(CRM_BASE + 0x08)
#define CRM_CLKINT    REG32(CRM_BASE + 0x0C)
#define CRM_AHBEN1    REG32(CRM_BASE + 0x30)
#define CRM_AHBEN2    REG32(CRM_BASE + 0x34)
#define CRM_APB1EN    REG32(CRM_BASE + 0x40)
#define CRM_APB2EN    REG32(CRM_BASE + 0x44)
#define CRM_CTRLSTS   REG32(CRM_BASE + 0x74)
#define CRM_MISC1     REG32(CRM_BASE + 0xA0)
#define CRM_MISC2     REG32(CRM_BASE + 0xA4)

#define CRM_AHBEN1_GPIOA (1u << 0)
#define CRM_AHBEN1_GPIOB (1u << 1)
#define CRM_AHBEN1_GPIOC (1u << 2)
#define CRM_AHBEN1_GPIOD (1u << 3)
#define CRM_AHBEN1_GPIOE (1u << 4)
#define CRM_AHBEN1_GPIOF (1u << 5)
#define CRM_APB1EN_TMR3  (1u << 1)
#define CRM_APB1EN_SPI2  (1u << 14)
#define CRM_APB1EN_SPI3  (1u << 15)
#define CRM_APB1EN_USART2 (1u << 17)
#define CRM_APB1EN_PWC   (1u << 28)
#define CRM_APB1EN_DAC   (1u << 29)
#define CRM_APB2EN_USART1 (1u << 4)
#define CRM_APB2EN_USART6 (1u << 5)
#define CRM_APB2EN_ADC1  (1u << 8)

/* ---- FLASH / PWC ---- */
#define FLASH_PSR     REG32(0x40023C00)
#define PWC_LDOOV     REG32(0x40007000 + 0x10)

/* ---- USART ---- */
typedef struct {
    volatile uint32_t sts, dt, baudr, ctrl1, ctrl2, ctrl3, gdiv;
} usart_t;
#define USART1 ((usart_t *)0x40011000)
#define USART2 ((usart_t *)0x40004400)
#define USART3 ((usart_t *)0x40004800)
#define USART6 ((usart_t *)0x40011400)
#define USART_STS_TDBE (1u << 7)
#define USART_STS_RDBF (1u << 5)
#define USART_CTRL1_UEN (1u << 13)
#define USART_CTRL1_TEN (1u << 3)
#define USART_CTRL1_REN (1u << 2)

/* ---- SysTick / SCB ---- */
#define SYST_CSR  REG32(0xE000E010)
#define SYST_RVR  REG32(0xE000E014)
#define SYST_CVR  REG32(0xE000E018)
#define SCB_VTOR  REG32(0xE000ED08)
#define SCB_AIRCR REG32(0xE000ED0C)
#define SCB_CPACR REG32(0xE000ED88)

/* ---- SPI ---- */
typedef struct {
    volatile uint32_t ctrl1, ctrl2, sts, dt, cpoly, rcrc, tcrc, i2sctrl, i2sclk;
} spi_t;
#define SPI1 ((spi_t *)0x40013000)
#define SPI2 ((spi_t *)0x40003800)
#define SPI3 ((spi_t *)0x40003C00)
#define SPI_STS_RDBF (1u << 0)
#define SPI_STS_TDBE (1u << 1)
#define SPI_STS_BF   (1u << 7)

/* ---- DAC ---- */
#define DAC_CTRL    REG32(0x40007400)
#define DAC_D1DTH8R REG32(0x40007410)   /* canal 1, 8 bits alineado a la derecha */
#define DAC_D2DTH12R REG32(0x40007414)
#define DAC_D2DTH8R REG32(0x4000741C)

/* Arranque minimo para Cortex-M4F. Tabla de vectores en 0x08002800. */
#include <stdint.h>
#include "at32f435.h"

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;
int main(void);
void SysTick_Handler(void);

void Reset_Handler(void);
void Default_Handler(void) { for (;;) { } }
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void USART1_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));

/* 16 excepciones del nucleo + 104 IRQ (igual que el firmware original) */
__attribute__((section(".isr_vector"), used))
void (*const vector_table[16 + 104])(void) = {
    [0]  = (void (*)(void))&_estack,
    [1]  = Reset_Handler,
    [2]  = Default_Handler,   /* NMI */
    [3]  = HardFault_Handler,
    [4]  = Default_Handler, [5] = Default_Handler, [6] = Default_Handler,
    [11] = Default_Handler,   /* SVC */
    [14] = Default_Handler,   /* PendSV */
    [15] = SysTick_Handler,
    [16 + 37] = USART1_IRQHandler,
};

void Reset_Handler(void)
{
    uint32_t *src = &_sidata, *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;
    for (dst = &_sbss; dst < &_ebss; ) *dst++ = 0;
    SCB_CPACR |= 0xFu << 20;              /* habilitar FPU (CP10/CP11) */
    SCB_VTOR = (uint32_t)vector_table;    /* el bootloader deja VTOR apuntando a 0x08000000 */
    main();
    for (;;) { }
}

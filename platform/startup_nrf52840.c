#include <stdint.h>

/* Symbols provided by platform/nrf52840.ld */
extern uint32_t _estack;
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;

extern int main(void);

typedef void (*isr_t)(void);

void Reset_Handler(void);
void Default_Handler(void);

void NMI_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)        __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)     __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void GPIOTE_IRQHandler(void)  __attribute__((weak, alias("Default_Handler")));
void RTC1_IRQHandler(void)    __attribute__((weak, alias("Default_Handler")));

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    while (dst < &_edata) {
        *dst++ = *src++;
    }
    for (dst = &_sbss; dst < &_ebss; dst++) {
        *dst = 0;
    }

    main();

    for (;;) { }
}

void Default_Handler(void)
{
    for (;;) { }
}

/*
 * Vector table. Index 16 + n is IRQ n (nRF52840 numbering).
 * Interrupts not listed here stay 0. Add an entry before enabling them.
 */
__attribute__((section(".isr_vector"), used))
const isr_t g_vectors[64] = {
    [0]  = (isr_t)(uintptr_t)&_estack,
    [1]  = Reset_Handler,
    [2]  = NMI_Handler,
    [3]  = HardFault_Handler,
    [11] = SVC_Handler,
    [14] = PendSV_Handler,
    [15] = SysTick_Handler,
    [16 + 6]  = GPIOTE_IRQHandler,
    [16 + 17] = RTC1_IRQHandler,
};

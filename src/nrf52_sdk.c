#include "core/nrf52_sdk.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

/* ---- Peripheral base addresses and offsets (nRF52840 datasheet) ------ */

#define CLOCK_BASE                  0x40000000UL
#define CLOCK_TASKS_HFCLKSTART      (CLOCK_BASE + 0x000UL)
#define CLOCK_EVENTS_HFCLKSTARTED   (CLOCK_BASE + 0x100UL)
#define CLOCK_HFCLKSTAT             (CLOCK_BASE + 0x40CUL)
#define CLOCK_HFCLKSTAT_STATE_Msk   (1UL << 0)

#define GPIO_P0_BASE                0x50000000UL
#define GPIO_P1_BASE                0x50000300UL
#define GPIO_OUT_OFF                0x504UL
#define GPIO_OUTSET_OFF             0x508UL
#define GPIO_OUTCLR_OFF             0x50CUL
#define GPIO_IN_OFF                 0x510UL
#define GPIO_DIRSET_OFF             0x518UL
#define GPIO_DIRCLR_OFF             0x51CUL
#define GPIO_PIN_CNF_OFF            0x700UL

#define PIN_CNF_DIR_OUTPUT          (1UL << 0)
#define PIN_CNF_INPUT_DISCONNECT    (1UL << 1)
#define PIN_CNF_PULL_SHIFT          2

#define SYST_CSR   REG32(0xE000E010UL)
#define SYST_RVR   REG32(0xE000E014UL)
#define SYST_CVR   REG32(0xE000E018UL)

#define SYST_CSR_ENABLE     (1UL << 0)
#define SYST_CSR_TICKINT    (1UL << 1)
#define SYST_CSR_CLKSOURCE  (1UL << 2)

#define HFCLK_START_TIMEOUT 1000000UL

static volatile uint32_t g_millis;
static bool g_initialized;

/* ---- Internal helpers ------------------------------------------------- */

static inline uint32_t gpio_base_for_port(uint16_t port)
{
    return (port == 0) ? GPIO_P0_BASE : GPIO_P1_BASE;
}

static nrf52_status_t decode_pin(uint16_t pin, uint32_t *base, uint32_t *bit)
{
    uint16_t port = pin >> 5;
    uint32_t num = pin & 0x1FU;

    if (port > 1) {
        return NRF52_ERR_PARAM;
    }
#if !NRF52_HAS_P1
    if (port == 1) {
        return NRF52_ERR_PARAM;
    }
#endif

    *base = gpio_base_for_port(port);
    *bit = 1UL << num;
    return NRF52_OK;
}

/* ---- System ----------------------------------------------------------- */

nrf52_status_t nrf52_init(void)
{
    /* Start the 64 MHz crystal oscillator and wait until it is running. */
    REG32(CLOCK_EVENTS_HFCLKSTARTED) = 0;
    REG32(CLOCK_TASKS_HFCLKSTART) = 1;

    uint32_t timeout = HFCLK_START_TIMEOUT;
    while (REG32(CLOCK_EVENTS_HFCLKSTARTED) == 0) {
        if (--timeout == 0) {
            return NRF52_ERR_TIMEOUT;
        }
    }
    while ((REG32(CLOCK_HFCLKSTAT) & CLOCK_HFCLKSTAT_STATE_Msk) == 0) {
        if (--timeout == 0) {
            return NRF52_ERR_TIMEOUT;
        }
    }

    /* SysTick at 1 kHz, driven by the processor clock. */
    SYST_RVR = (NRF52_CPU_HZ / 1000UL) - 1UL;
    SYST_CVR = 0;
    SYST_CSR = SYST_CSR_CLKSOURCE | SYST_CSR_TICKINT | SYST_CSR_ENABLE;

    g_initialized = true;
    return NRF52_OK;
}

uint32_t nrf52_millis(void)
{
    return g_millis;
}

void nrf52_delay_ms(uint32_t ms)
{
    uint32_t start = g_millis;
    while ((uint32_t)(g_millis - start) < ms) {
        nrf52_wait_for_interrupt();
    }
}

void nrf52_wait_for_interrupt(void)
{
    __asm__ volatile("wfi");
}

void nrf52_irq_enable(uint8_t irqn, uint8_t priority)
{
    /* NVIC_IPR is byte-addressable; priority sits in the top 3 bits. */
    *(volatile uint8_t *)(0xE000E400UL + irqn) = (uint8_t)((priority & 0x7U) << 5);
    *(volatile uint32_t *)(0xE000E100UL + 4UL * (irqn / 32U)) = 1UL << (irqn % 32U);
}

/* SysTick interrupt. The startup file's vector table must point here. */
void SysTick_Handler(void)
{
    g_millis++;
}

/* ---- GPIO ------------------------------------------------------------- */

nrf52_status_t nrf52_gpio_config_input(uint16_t pin, nrf52_pull_t pull)
{
    uint32_t base, bit;
    if (decode_pin(pin, &base, &bit) != NRF52_OK) {
        return NRF52_ERR_PARAM;
    }

    uint32_t cnf_addr = base + GPIO_PIN_CNF_OFF + 4UL * (pin & 0x1FU);
    REG32(base + GPIO_DIRCLR_OFF) = bit;
    REG32(cnf_addr) = ((uint32_t)pull << PIN_CNF_PULL_SHIFT);
    return NRF52_OK;
}

nrf52_status_t nrf52_gpio_config_output(uint16_t pin, bool initial_high)
{
    uint32_t base, bit;
    if (decode_pin(pin, &base, &bit) != NRF52_OK) {
        return NRF52_ERR_PARAM;
    }

    uint32_t cnf_addr = base + GPIO_PIN_CNF_OFF + 4UL * (pin & 0x1FU);

    /* Set the level before enabling the output driver to avoid a glitch. */
    if (initial_high) {
        REG32(base + GPIO_OUTSET_OFF) = bit;
    } else {
        REG32(base + GPIO_OUTCLR_OFF) = bit;
    }

    REG32(cnf_addr) = PIN_CNF_DIR_OUTPUT | PIN_CNF_INPUT_DISCONNECT;
    REG32(base + GPIO_DIRSET_OFF) = bit;
    return NRF52_OK;
}

void nrf52_gpio_write(uint16_t pin, bool high)
{
    uint32_t base, bit;
    if (decode_pin(pin, &base, &bit) != NRF52_OK) {
        return;
    }

    if (high) {
        REG32(base + GPIO_OUTSET_OFF) = bit;
    } else {
        REG32(base + GPIO_OUTCLR_OFF) = bit;
    }
}

void nrf52_gpio_toggle(uint16_t pin)
{
    uint32_t base, bit;
    if (decode_pin(pin, &base, &bit) != NRF52_OK) {
        return;
    }

    uint32_t out = REG32(base + GPIO_OUT_OFF);
    if (out & bit) {
        REG32(base + GPIO_OUTCLR_OFF) = bit;
    } else {
        REG32(base + GPIO_OUTSET_OFF) = bit;
    }
}

bool nrf52_gpio_read(uint16_t pin)
{
    uint32_t base, bit;
    if (decode_pin(pin, &base, &bit) != NRF52_OK) {
        return false;
    }

    return (REG32(base + GPIO_IN_OFF) & bit) != 0;
}

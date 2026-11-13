#ifndef NRF52_SDK_H
#define NRF52_SDK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NRF52_SDK_VERSION_MAJOR 0
#define NRF52_SDK_VERSION_MINOR 1
#define NRF52_SDK_VERSION_PATCH 0

/* Set to 0 for nRF52832 / nRF52810 style parts that have only port 0. */
#ifndef NRF52_HAS_P1
#define NRF52_HAS_P1 1
#endif

/* CPU clock after reset. Used to derive the SysTick reload value. */
#ifndef NRF52_CPU_HZ
#define NRF52_CPU_HZ 64000000UL
#endif

typedef enum {
    NRF52_OK = 0,
    NRF52_ERR_PARAM,
    NRF52_ERR_TIMEOUT,
    NRF52_ERR_IO,
} nrf52_status_t;

typedef enum {
    NRF52_PULL_NONE = 0,
    NRF52_PULL_DOWN = 1,
    NRF52_PULL_UP   = 3,
} nrf52_pull_t;

/*
 * Pins are addressed as a single value: port * 32 + pin.
 * Use NRF52_PIN(0, 13) for P0.13, NRF52_PIN(1, 6) for P1.06.
 */
#define NRF52_PIN(port, pin) ((uint16_t)(((port) << 5) | ((pin) & 0x1FU)))

/* ---- System ---------------------------------------------------------- */

/* Starts the 64 MHz crystal oscillator and the 1 kHz system tick. */
nrf52_status_t nrf52_init(void);

/* Milliseconds since nrf52_init(). Wraps after ~49 days. */
uint32_t nrf52_millis(void);

/* Blocks for at least the given number of milliseconds. */
void nrf52_delay_ms(uint32_t ms);

/* Sleeps until the next interrupt (WFI). Call it at the end of the main loop. */
void nrf52_wait_for_interrupt(void);

/* Enables an NVIC interrupt. priority: 0 (highest) to 7 (lowest). */
void nrf52_irq_enable(uint8_t irqn, uint8_t priority);

/* ---- GPIO ------------------------------------------------------------ */

nrf52_status_t nrf52_gpio_config_input(uint16_t pin, nrf52_pull_t pull);
nrf52_status_t nrf52_gpio_config_output(uint16_t pin, bool initial_high);

void nrf52_gpio_write(uint16_t pin, bool high);
void nrf52_gpio_toggle(uint16_t pin);
bool nrf52_gpio_read(uint16_t pin);

#ifdef __cplusplus
}
#endif

#endif /* NRF52_SDK_H */

#include <stdbool.h>
#include <stdint.h>
#include "gpiote/nrf52_gpiote.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define GPIOTE_BASE             0x40006000UL
#define GPIOTE_EVENTS_IN(ch)    (GPIOTE_BASE + 0x100UL + 4UL * (ch))
#define GPIOTE_INTENSET         (GPIOTE_BASE + 0x304UL)
#define GPIOTE_INTENCLR         (GPIOTE_BASE + 0x308UL)
#define GPIOTE_CONFIG(ch)       (GPIOTE_BASE + 0x510UL + 4UL * (ch))

#define GPIOTE_MODE_EVENT       1UL
#define GPIOTE_CHANNELS         8U
#define GPIOTE_IRQN             6U
#define GPIOTE_IRQ_PRIORITY     3U

typedef struct {
    bool active;
    uint16_t pin;
    nrf52_gpio_irq_cb_t cb;
    void *ctx;
} gpiote_slot_t;

static gpiote_slot_t g_slots[GPIOTE_CHANNELS];

nrf52_status_t nrf52_gpiote_enable_irq(uint8_t ch, uint16_t pin,
                                       nrf52_pull_t pull, nrf52_edge_t edge,
                                       nrf52_gpio_irq_cb_t cb, void *ctx)
{
    if (ch >= GPIOTE_CHANNELS || !cb) {
        return NRF52_ERR_PARAM;
    }
    if (edge < NRF52_EDGE_RISING || edge > NRF52_EDGE_TOGGLE) {
        return NRF52_ERR_PARAM;
    }

    /* Validates the pin and sets it up as an input. */
    nrf52_status_t st = nrf52_gpio_config_input(pin, pull);
    if (st != NRF52_OK) {
        return st;
    }

    REG32(GPIOTE_INTENCLR) = 1UL << ch;

    g_slots[ch].active = true;
    g_slots[ch].pin = pin;
    g_slots[ch].cb = cb;
    g_slots[ch].ctx = ctx;

    REG32(GPIOTE_EVENTS_IN(ch)) = 0;
    REG32(GPIOTE_CONFIG(ch)) = GPIOTE_MODE_EVENT
                             | ((uint32_t)(pin & 0x1FU) << 8)
                             | ((uint32_t)((pin >> 5) & 1U) << 13)
                             | ((uint32_t)edge << 16);
    REG32(GPIOTE_INTENSET) = 1UL << ch;

    nrf52_irq_enable(GPIOTE_IRQN, GPIOTE_IRQ_PRIORITY);
    return NRF52_OK;
}

void nrf52_gpiote_disable_irq(uint8_t ch)
{
    if (ch >= GPIOTE_CHANNELS) {
        return;
    }
    REG32(GPIOTE_INTENCLR) = 1UL << ch;
    REG32(GPIOTE_CONFIG(ch)) = 0;
    g_slots[ch].active = false;
}

void GPIOTE_IRQHandler(void)
{
    for (uint8_t ch = 0; ch < GPIOTE_CHANNELS; ch++) {
        if (REG32(GPIOTE_EVENTS_IN(ch)) != 0) {
            REG32(GPIOTE_EVENTS_IN(ch)) = 0;
            if (g_slots[ch].active && g_slots[ch].cb) {
                g_slots[ch].cb(g_slots[ch].pin, g_slots[ch].ctx);
            }
        }
    }
}

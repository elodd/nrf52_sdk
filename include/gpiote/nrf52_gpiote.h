#ifndef NRF52_GPIOTE_H
#define NRF52_GPIOTE_H

#include <stdint.h>
#include "core/nrf52_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NRF52_EDGE_RISING  = 1,
    NRF52_EDGE_FALLING = 2,
    NRF52_EDGE_TOGGLE  = 3,
} nrf52_edge_t;

/* Runs in interrupt context. Keep it short: set flags, do not do I2C here. */
typedef void (*nrf52_gpio_irq_cb_t)(uint16_t pin, void *ctx);

/* channel 0..7. Configures the pin as input with the given pull. */
nrf52_status_t nrf52_gpiote_enable_irq(uint8_t channel, uint16_t pin,
                                       nrf52_pull_t pull, nrf52_edge_t edge,
                                       nrf52_gpio_irq_cb_t cb, void *ctx);
void nrf52_gpiote_disable_irq(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif /* NRF52_GPIOTE_H */

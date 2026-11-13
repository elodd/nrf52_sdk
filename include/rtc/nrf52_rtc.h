#ifndef NRF52_RTC_H
#define NRF52_RTC_H

#include <stdint.h>
#include "core/nrf52_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Runs in interrupt context. */
typedef void (*nrf52_periodic_cb_t)(void *ctx);

/*
 * Periodic callback on RTC1, clocked by the 32.768 kHz crystal (LFCLK).
 * Resolution is about 30 us. Maximum period is about 500 s.
 * RTC1 must not be used by another module (for example a SoftDevice).
 */
nrf52_status_t nrf52_periodic_start(uint32_t period_ms, nrf52_periodic_cb_t cb, void *ctx);
void nrf52_periodic_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* NRF52_RTC_H */

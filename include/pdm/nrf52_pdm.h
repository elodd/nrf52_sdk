#ifndef NRF52_PDM_H
#define NRF52_PDM_H

#include <stddef.h>
#include <stdint.h>
#include "core/nrf52_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Largest buffer one nrf52_pdm_read() call can fill (SAMPLE.MAXCNT is 15 bits). */
#define NRF52_PDM_MAX_SAMPLES   0x7FFFU

/*
 * PDM digital microphone on the PDM peripheral (EasyDMA).
 * Mono, 16-bit signed samples. Output rate: 1.032 MHz clock / 64, about 16.125 kHz.
 * Blocking API: nrf52_pdm_read() returns when the buffer is full.
 * The buffer must be in RAM (EasyDMA cannot read from flash).
 * Not thread-safe; call from one context only.
 */

/* Configures CLK (output) and DIN (input) and enables the peripheral. */
nrf52_status_t nrf52_pdm_init(uint16_t clk_pin, uint16_t din_pin);

/* Records count samples into samples. Returns NRF52_ERR_TIMEOUT if the transfer stalls. */
nrf52_status_t nrf52_pdm_read(int16_t *samples, size_t count);

/* Disables the peripheral. */
void nrf52_pdm_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* NRF52_PDM_H */

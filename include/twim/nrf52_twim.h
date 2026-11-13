#ifndef NRF52_TWIM_H
#define NRF52_TWIM_H

#include <stddef.h>
#include <stdint.h>
#include "core/nrf52_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * I2C master on TWIM0 (EasyDMA). Blocking API.
 * freq_hz must be 100000 or 400000.
 * All data buffers must be in RAM: EasyDMA cannot read from flash.
 * Not thread-safe; call from one context only.
 */
nrf52_status_t nrf52_twim_init(uint16_t scl_pin, uint16_t sda_pin, uint32_t freq_hz);

nrf52_status_t nrf52_twim_write(uint8_t addr, const uint8_t *data, size_t len);
nrf52_status_t nrf52_twim_read(uint8_t addr, uint8_t *data, size_t len);

/* Write then read with a repeated start (no stop between phases). */
nrf52_status_t nrf52_twim_write_read(uint8_t addr,
                                     const uint8_t *tx, size_t tx_len,
                                     uint8_t *rx, size_t rx_len);

#ifdef __cplusplus
}
#endif

#endif /* NRF52_TWIM_H */

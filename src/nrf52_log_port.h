#ifndef NRF52_LOG_PORT_H
#define NRF52_LOG_PORT_H

#include <stddef.h>
#include <stdint.h>
#include "core/nrf52_sdk.h"

/*
 * Output transport for src/nrf52_log.c. Private to the SDK: not in include/.
 *
 * Firmware: src/nrf52_log_uarte.c (UARTE0 registers).
 * Host tests: tests/mocks.c (captures the lines).
 * Simulator: sim/sim_hal.c (writes to stdout).
 */

/* Configures the transport on tx_pin. Logging stays disabled if this fails. */
nrf52_status_t nrf52_log_port_init(uint16_t tx_pin);

/* Sends one complete line, including the trailing "\r\n". Blocking. */
void nrf52_log_port_write(const uint8_t *data, size_t len);

#endif /* NRF52_LOG_PORT_H */

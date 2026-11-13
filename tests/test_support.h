#ifndef TEST_SUPPORT_H
#define TEST_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "core/nrf52_sdk.h"
#include "gpiote/nrf52_gpiote.h"

/* Test doubles for the hardware modules. The real src/ files are not linked. */

void mock_reset(void);

/* Time */
void mock_set_now(uint32_t ms);

/* Periodic timer (nrf52_periodic_start) */
bool mock_periodic_running(void);
uint32_t mock_periodic_period(void);
void mock_fire_periodic(void);

/* GPIO interrupt (nrf52_gpiote_enable_irq) */
bool mock_gpio_registered(void);
uint16_t mock_gpio_pin(void);
nrf52_edge_t mock_gpio_edge(void);
void mock_fire_gpio_irq(void);

/* I2C (nrf52_twim_write_read) */
void mock_set_i2c_response(const uint8_t *data, size_t len, nrf52_status_t status);
uint8_t mock_i2c_last_addr(void);
uint8_t mock_i2c_last_reg(void);
size_t mock_i2c_last_rx_len(void);

/*
 * Logging: the real src/nrf52_log.c runs on a fake transport (nrf52_log_port_*)
 * that captures each line. mock_reset() initializes the logger at level INFO.
 */
#define MOCK_LOG_MAX_LINES  16
#define MOCK_LOG_LINE_SIZE  200

void mock_set_log_port_status(nrf52_status_t status);   /* result of the next port init */
uint16_t mock_log_tx_pin(void);
int mock_log_line_count(void);          /* lines written, including any not stored */
const char *mock_log_line(int index);   /* NUL-terminated, includes "\r\n"; "" if out of range */
size_t mock_log_line_len(int index);    /* bytes passed to the transport */
int mock_warn_count(void);              /* stored lines at level W */

#endif /* TEST_SUPPORT_H */

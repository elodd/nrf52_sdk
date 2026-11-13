/* UARTE0 transport for the logger (src/nrf52_log.c). 115200 8N1, TX only. */
#include <stddef.h>
#include <stdint.h>
#include "nrf52_log_port.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define UARTE0_BASE             0x40002000UL
#define UARTE_TASKS_STARTTX     (UARTE0_BASE + 0x008UL)
#define UARTE_EVENTS_ENDTX      (UARTE0_BASE + 0x120UL)
#define UARTE_ENABLE            (UARTE0_BASE + 0x500UL)
#define UARTE_PSEL_TXD          (UARTE0_BASE + 0x50CUL)
#define UARTE_PSEL_RXD          (UARTE0_BASE + 0x514UL)
#define UARTE_BAUDRATE          (UARTE0_BASE + 0x524UL)
#define UARTE_CONFIG            (UARTE0_BASE + 0x56CUL)
#define UARTE_TXD_PTR           (UARTE0_BASE + 0x544UL)
#define UARTE_TXD_MAXCNT        (UARTE0_BASE + 0x548UL)

#define UARTE_ENABLE_VALUE      8UL
#define UARTE_BAUD_115200       0x01D7E000UL
#define UARTE_8N1_CONFIG        0UL
#define PSEL_DISCONNECTED       0xFFFFFFFFUL

#define TX_SPIN_LIMIT           1000000UL

nrf52_status_t nrf52_log_port_init(uint16_t tx_pin)
{
    nrf52_status_t st = nrf52_gpio_config_output(tx_pin, true);
    if (st != NRF52_OK) {
        return st;
    }

    REG32(UARTE_ENABLE) = 0;
    REG32(UARTE_PSEL_TXD) = tx_pin & 0x3FU;
    REG32(UARTE_PSEL_RXD) = PSEL_DISCONNECTED;
    REG32(UARTE_BAUDRATE) = UARTE_BAUD_115200;
    REG32(UARTE_CONFIG) = UARTE_8N1_CONFIG;
    REG32(UARTE_ENABLE) = UARTE_ENABLE_VALUE;
    return NRF52_OK;
}

void nrf52_log_port_write(const uint8_t *data, size_t len)
{
    REG32(UARTE_EVENTS_ENDTX) = 0;
    REG32(UARTE_TXD_PTR) = (uint32_t)(uintptr_t)data;
    REG32(UARTE_TXD_MAXCNT) = (uint32_t)len;
    REG32(UARTE_TASKS_STARTTX) = 1;

    for (uint32_t i = 0; i < TX_SPIN_LIMIT && REG32(UARTE_EVENTS_ENDTX) == 0; i++) {
    }
}

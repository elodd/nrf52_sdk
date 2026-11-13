#include <stdbool.h>
#include <stdint.h>
#include "twim/nrf52_twim.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define TWIM0_BASE              0x40003000UL
#define TWIM_TASKS_STARTRX      (TWIM0_BASE + 0x000UL)
#define TWIM_TASKS_STARTTX      (TWIM0_BASE + 0x008UL)
#define TWIM_TASKS_STOP         (TWIM0_BASE + 0x014UL)
#define TWIM_EVENTS_STOPPED     (TWIM0_BASE + 0x104UL)
#define TWIM_EVENTS_ERROR       (TWIM0_BASE + 0x124UL)
#define TWIM_SHORTS             (TWIM0_BASE + 0x200UL)
#define TWIM_ERRORSRC           (TWIM0_BASE + 0x4C4UL)
#define TWIM_ENABLE             (TWIM0_BASE + 0x500UL)
#define TWIM_PSEL_SCL           (TWIM0_BASE + 0x508UL)
#define TWIM_PSEL_SDA           (TWIM0_BASE + 0x50CUL)
#define TWIM_FREQUENCY          (TWIM0_BASE + 0x524UL)
#define TWIM_RXD_PTR            (TWIM0_BASE + 0x534UL)
#define TWIM_RXD_MAXCNT         (TWIM0_BASE + 0x538UL)
#define TWIM_TXD_PTR            (TWIM0_BASE + 0x544UL)
#define TWIM_TXD_MAXCNT         (TWIM0_BASE + 0x548UL)
#define TWIM_ADDRESS            (TWIM0_BASE + 0x588UL)

#define TWIM_ENABLE_VALUE       6UL
#define TWIM_FREQ_100K          0x01980000UL
#define TWIM_FREQ_400K          0x06400000UL

#define SHORT_LASTTX_STARTRX    (1UL << 7)
#define SHORT_LASTTX_STOP       (1UL << 9)
#define SHORT_LASTRX_STOP       (1UL << 12)

#define ERRORSRC_CLEAR_ALL      0x7UL
#define PSEL_DISCONNECTED       0xFFFFFFFFUL
#define TWIM_TIMEOUT_MS         10UL
#define SPIN_LIMIT              100000UL

#define GPIO_P0_BASE            0x50000000UL
#define GPIO_P1_BASE            0x50000300UL
#define GPIO_PIN_CNF_OFF        0x700UL
#define PIN_CNF_PULLUP          (3UL << 2)
#define PIN_CNF_S0D1            (6UL << 8)

static bool g_initialized;

static bool pin_valid(uint16_t pin)
{
    uint16_t port = pin >> 5;
    if (port == 0) {
        return true;
    }
#if NRF52_HAS_P1
    return port == 1;
#else
    return false;
#endif
}

/* Open-drain with pull-up, input buffer connected: standard I2C pin setup. */
static void configure_bus_pin(uint16_t pin)
{
    uint32_t base = (pin >> 5) ? GPIO_P1_BASE : GPIO_P0_BASE;
    REG32(base + GPIO_PIN_CNF_OFF + 4UL * (pin & 0x1FU)) = PIN_CNF_PULLUP | PIN_CNF_S0D1;
}

static void stop_bus(void)
{
    REG32(TWIM_TASKS_STOP) = 1;
    for (uint32_t i = 0; i < SPIN_LIMIT && REG32(TWIM_EVENTS_STOPPED) == 0; i++) {
    }
}

nrf52_status_t nrf52_twim_init(uint16_t scl_pin, uint16_t sda_pin, uint32_t freq_hz)
{
    uint32_t freq_reg;

    if (!pin_valid(scl_pin) || !pin_valid(sda_pin) || scl_pin == sda_pin) {
        return NRF52_ERR_PARAM;
    }
    if (freq_hz == 100000UL) {
        freq_reg = TWIM_FREQ_100K;
    } else if (freq_hz == 400000UL) {
        freq_reg = TWIM_FREQ_400K;
    } else {
        return NRF52_ERR_PARAM;
    }

    configure_bus_pin(scl_pin);
    configure_bus_pin(sda_pin);

    REG32(TWIM_ENABLE) = 0;
    REG32(TWIM_PSEL_SCL) = scl_pin & 0x3FU;
    REG32(TWIM_PSEL_SDA) = sda_pin & 0x3FU;
    REG32(TWIM_FREQUENCY) = freq_reg;
    REG32(TWIM_SHORTS) = 0;
    REG32(TWIM_ENABLE) = TWIM_ENABLE_VALUE;

    g_initialized = true;
    return NRF52_OK;
}

static nrf52_status_t transfer(uint8_t addr,
                               const uint8_t *tx, size_t tx_len,
                               uint8_t *rx, size_t rx_len)
{
    if (!g_initialized || addr > 0x7FU) {
        return NRF52_ERR_PARAM;
    }
    if (tx_len == 0 && rx_len == 0) {
        return NRF52_ERR_PARAM;
    }
    if (tx_len > 0xFFFFU || rx_len > 0xFFFFU) {
        return NRF52_ERR_PARAM;
    }
    if ((tx_len && !tx) || (rx_len && !rx)) {
        return NRF52_ERR_PARAM;
    }

    REG32(TWIM_EVENTS_STOPPED) = 0;
    REG32(TWIM_EVENTS_ERROR) = 0;
    REG32(TWIM_ERRORSRC) = ERRORSRC_CLEAR_ALL;
    REG32(TWIM_ADDRESS) = addr;

    if (tx_len) {
        REG32(TWIM_TXD_PTR) = (uint32_t)(uintptr_t)tx;
        REG32(TWIM_TXD_MAXCNT) = (uint32_t)tx_len;
    }
    if (rx_len) {
        REG32(TWIM_RXD_PTR) = (uint32_t)(uintptr_t)rx;
        REG32(TWIM_RXD_MAXCNT) = (uint32_t)rx_len;
    }

    if (tx_len && rx_len) {
        /* TX -> repeated start -> RX -> stop */
        REG32(TWIM_SHORTS) = SHORT_LASTTX_STARTRX | SHORT_LASTRX_STOP;
        REG32(TWIM_TASKS_STARTTX) = 1;
    } else if (tx_len) {
        REG32(TWIM_SHORTS) = SHORT_LASTTX_STOP;
        REG32(TWIM_TASKS_STARTTX) = 1;
    } else {
        REG32(TWIM_SHORTS) = SHORT_LASTRX_STOP;
        REG32(TWIM_TASKS_STARTRX) = 1;
    }

    uint32_t start = nrf52_millis();
    while (REG32(TWIM_EVENTS_STOPPED) == 0 && REG32(TWIM_EVENTS_ERROR) == 0) {
        if ((uint32_t)(nrf52_millis() - start) >= TWIM_TIMEOUT_MS) {
            REG32(TWIM_SHORTS) = 0;
            stop_bus();
            return NRF52_ERR_TIMEOUT;
        }
    }

    REG32(TWIM_SHORTS) = 0;

    if (REG32(TWIM_EVENTS_ERROR)) {
        /* Typical causes: NACK from address or data, or bus overrun. */
        stop_bus();
        return NRF52_ERR_IO;
    }
    return NRF52_OK;
}

nrf52_status_t nrf52_twim_write(uint8_t addr, const uint8_t *data, size_t len)
{
    return transfer(addr, data, len, NULL, 0);
}

nrf52_status_t nrf52_twim_read(uint8_t addr, uint8_t *data, size_t len)
{
    return transfer(addr, NULL, 0, data, len);
}

nrf52_status_t nrf52_twim_write_read(uint8_t addr,
                                     const uint8_t *tx, size_t tx_len,
                                     uint8_t *rx, size_t rx_len)
{
    return transfer(addr, tx, tx_len, rx, rx_len);
}

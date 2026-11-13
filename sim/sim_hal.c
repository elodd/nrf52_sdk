/*
 * Host simulation of the SDK hardware layer.
 *
 * Replaces the register-level modules in src/ (core, GPIO, TWIM, GPIOTE, RTC and the
 * UART log transport) so an application from apps/ runs unchanged on a desktop
 * computer. The sensor scheduler, the logger and the drivers are the real code.
 *
 * - Time is simulated: each nrf52_wait_for_interrupt() advances the clock by 1 ms
 *   and fires the periodic timer when it is due. The run stops after SIM_SECONDS.
 * - Log lines from the real src/nrf52_log.c go to stdout instead of the UART.
 * - The I2C bus has one fake device (see SIM_SENSOR_*). Other addresses NACK.
 *
 * Environment variables:
 *   SIM_SECONDS=<n>   simulated run time in seconds (default 10)
 *   SIM_REALTIME=1    pace the clock to wall time instead of running at full speed
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "core/nrf52_sdk.h"
#include "gpiote/nrf52_gpiote.h"
#include "log/nrf52_log.h"
#include "rtc/nrf52_rtc.h"
#include "twim/nrf52_twim.h"

#define SIM_PIN_COUNT        48U      /* P0.00-P0.31, P1.00-P1.15 */
#define SIM_DEFAULT_SECONDS  10UL

/* Fake I2C sensor: 2-byte big-endian value that drifts slowly over time. */
#define SIM_SENSOR_ADDR      0x44U
#define SIM_SENSOR_REG       0x00U
#define SIM_SENSOR_BASE      2150     /* e.g. 21.50 degC in centi-degrees */

static uint32_t s_now_ms;
static uint32_t s_stop_ms;
static bool s_realtime;
static bool s_started;

static nrf52_periodic_cb_t s_periodic_cb;
static void *s_periodic_ctx;
static uint32_t s_periodic_period;
static uint32_t s_periodic_next;

static bool s_pin_level[SIM_PIN_COUNT];
static uint32_t s_pin_toggles[SIM_PIN_COUNT];

static void print_summary(void)
{
    fprintf(stderr, "sim: stopped at %lu ms\n", (unsigned long)s_now_ms);
    for (uint16_t pin = 0; pin < SIM_PIN_COUNT; pin++) {
        if (s_pin_toggles[pin] > 0) {
            fprintf(stderr, "sim: P%u.%02u toggled %lu times\n",
                    pin >> 5, pin & 0x1FU, (unsigned long)s_pin_toggles[pin]);
        }
    }
}

static void sim_start(void)
{
    if (s_started) {
        return;
    }
    s_started = true;

    const char *secs = getenv("SIM_SECONDS");
    unsigned long seconds = secs ? strtoul(secs, NULL, 10) : SIM_DEFAULT_SECONDS;
    if (seconds == 0) {
        seconds = SIM_DEFAULT_SECONDS;
    }
    s_stop_ms = (uint32_t)(seconds * 1000UL);

    const char *rt = getenv("SIM_REALTIME");
    s_realtime = rt && rt[0] == '1';

    setvbuf(stdout, NULL, _IOLBF, 0);   /* one line at a time, also when piped */
    atexit(print_summary);
}

/* Advances the simulated clock by 1 ms and runs the timer "interrupt" if due. */
static void sim_tick(void)
{
    sim_start();
    if (s_now_ms >= s_stop_ms) {
        exit(0);
    }
    if (s_realtime) {
        struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000L };
        nanosleep(&ts, NULL);
    }
    s_now_ms++;

    if (s_periodic_cb && s_now_ms >= s_periodic_next) {
        s_periodic_next += s_periodic_period;
        s_periodic_cb(s_periodic_ctx);
    }
}

/* ---- Core (nrf52_sdk.h) ---------------------------------------------- */

nrf52_status_t nrf52_init(void)
{
    sim_start();
    s_now_ms = 0;
    return NRF52_OK;
}

uint32_t nrf52_millis(void)
{
    return s_now_ms;
}

void nrf52_delay_ms(uint32_t ms)
{
    uint32_t start = s_now_ms;
    while ((uint32_t)(s_now_ms - start) < ms) {
        sim_tick();
    }
}

void nrf52_wait_for_interrupt(void)
{
    sim_tick();
}

void nrf52_irq_enable(uint8_t irqn, uint8_t priority)
{
    (void)irqn;
    (void)priority;
}

/* ---- GPIO ------------------------------------------------------------ */

static bool pin_valid(uint16_t pin)
{
    return pin < SIM_PIN_COUNT;
}

nrf52_status_t nrf52_gpio_config_input(uint16_t pin, nrf52_pull_t pull)
{
    if (!pin_valid(pin)) {
        return NRF52_ERR_PARAM;
    }
    s_pin_level[pin] = (pull == NRF52_PULL_UP);
    return NRF52_OK;
}

nrf52_status_t nrf52_gpio_config_output(uint16_t pin, bool initial_high)
{
    if (!pin_valid(pin)) {
        return NRF52_ERR_PARAM;
    }
    s_pin_level[pin] = initial_high;
    return NRF52_OK;
}

void nrf52_gpio_write(uint16_t pin, bool high)
{
    if (pin_valid(pin)) {
        s_pin_level[pin] = high;
    }
}

void nrf52_gpio_toggle(uint16_t pin)
{
    if (pin_valid(pin)) {
        s_pin_level[pin] = !s_pin_level[pin];
        s_pin_toggles[pin]++;
    }
}

bool nrf52_gpio_read(uint16_t pin)
{
    return pin_valid(pin) && s_pin_level[pin];
}

/* ---- I2C (nrf52_twim.h) ---------------------------------------------- */

nrf52_status_t nrf52_twim_init(uint16_t scl_pin, uint16_t sda_pin, uint32_t freq_hz)
{
    if (!pin_valid(scl_pin) || !pin_valid(sda_pin) ||
        (freq_hz != 100000UL && freq_hz != 400000UL)) {
        return NRF52_ERR_PARAM;
    }
    return NRF52_OK;
}

nrf52_status_t nrf52_twim_write(uint8_t addr, const uint8_t *data, size_t len)
{
    (void)data;
    (void)len;
    return (addr == SIM_SENSOR_ADDR) ? NRF52_OK : NRF52_ERR_IO;
}

nrf52_status_t nrf52_twim_read(uint8_t addr, uint8_t *data, size_t len)
{
    return nrf52_twim_write_read(addr, NULL, 0, data, len);
}

nrf52_status_t nrf52_twim_write_read(uint8_t addr,
                                     const uint8_t *tx, size_t tx_len,
                                     uint8_t *rx, size_t rx_len)
{
    if (addr != SIM_SENSOR_ADDR) {
        return NRF52_ERR_IO;                       /* no device: address NACK */
    }
    uint8_t reg = (tx_len > 0) ? tx[0] : SIM_SENSOR_REG;
    int32_t value = 0;
    if (reg == SIM_SENSOR_REG) {
        /* Slow sawtooth: +5 per second, wraps every 20 s. */
        value = SIM_SENSOR_BASE + (int32_t)((s_now_ms / 1000U) % 20U) * 5;
    }
    for (size_t i = 0; i < rx_len; i++) {
        size_t shift = (rx_len - 1U - i) * 8U;     /* big-endian */
        rx[i] = (shift < 32U) ? (uint8_t)(value >> shift) : 0;
    }
    return NRF52_OK;
}

/* ---- Pin interrupts (nrf52_gpiote.h) --------------------------------- */

nrf52_status_t nrf52_gpiote_enable_irq(uint8_t channel, uint16_t pin,
                                       nrf52_pull_t pull, nrf52_edge_t edge,
                                       nrf52_gpio_irq_cb_t cb, void *ctx)
{
    (void)edge;
    (void)ctx;
    if (channel > 7U || !cb) {
        return NRF52_ERR_PARAM;
    }
    /* Accepted but never fired: the simulation has no external pin events. */
    return nrf52_gpio_config_input(pin, pull);
}

void nrf52_gpiote_disable_irq(uint8_t channel)
{
    (void)channel;
}

/* ---- Periodic timer (nrf52_rtc.h) ------------------------------------ */

nrf52_status_t nrf52_periodic_start(uint32_t period_ms, nrf52_periodic_cb_t cb, void *ctx)
{
    if (period_ms == 0 || !cb) {
        return NRF52_ERR_PARAM;
    }
    s_periodic_cb = cb;
    s_periodic_ctx = ctx;
    s_periodic_period = period_ms;
    s_periodic_next = s_now_ms + period_ms;
    return NRF52_OK;
}

void nrf52_periodic_stop(void)
{
    s_periodic_cb = NULL;
}

/* ---- Log transport (src/nrf52_log_port.h) ---------------------------- */
/* The real src/nrf52_log.c formats the lines; this writes them to stdout.   */

nrf52_status_t nrf52_log_port_init(uint16_t tx_pin)
{
    return nrf52_gpio_config_output(tx_pin, true);
}

void nrf52_log_port_write(const uint8_t *data, size_t len)
{
    /* Drop the UART's "\r" so saved logs have plain "\n" line endings. */
    if (len >= 2U && data[len - 2U] == '\r') {
        fwrite(data, 1, len - 2U, stdout);
        fputc('\n', stdout);
    } else {
        fwrite(data, 1, len, stdout);
    }
}

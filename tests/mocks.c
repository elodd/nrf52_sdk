#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include "test_support.h"
#include "rtc/nrf52_rtc.h"
#include "twim/nrf52_twim.h"
#include "log/nrf52_log.h"
#include "nrf52_log_port.h"

static uint32_t s_now;

static bool s_periodic_running;
static uint32_t s_periodic_period;
static nrf52_periodic_cb_t s_periodic_cb;
static void *s_periodic_ctx;

static bool s_gpio_registered;
static uint16_t s_gpio_pin;
static nrf52_edge_t s_gpio_edge;
static nrf52_gpio_irq_cb_t s_gpio_cb;
static void *s_gpio_ctx;

static uint8_t s_i2c_resp[4];
static size_t s_i2c_resp_len;
static nrf52_status_t s_i2c_status;
static uint8_t s_i2c_last_addr;
static uint8_t s_i2c_last_reg;
static size_t s_i2c_last_rx_len;

static nrf52_status_t s_log_port_status;
static uint16_t s_log_tx_pin;
static int s_log_count;
static char s_log_lines[MOCK_LOG_MAX_LINES][MOCK_LOG_LINE_SIZE];
static size_t s_log_lens[MOCK_LOG_MAX_LINES];

void mock_reset(void)
{
    s_now = 0;
    s_periodic_running = false;
    s_periodic_period = 0;
    s_periodic_cb = NULL;
    s_periodic_ctx = NULL;
    s_gpio_registered = false;
    s_gpio_pin = 0;
    s_gpio_edge = NRF52_EDGE_RISING;
    s_gpio_cb = NULL;
    s_gpio_ctx = NULL;
    memset(s_i2c_resp, 0, sizeof(s_i2c_resp));
    s_i2c_resp_len = 0;
    s_i2c_status = NRF52_OK;
    s_i2c_last_addr = 0;
    s_i2c_last_reg = 0;
    s_i2c_last_rx_len = 0;
    s_log_port_status = NRF52_OK;
    s_log_tx_pin = 0;
    s_log_count = 0;
    memset(s_log_lines, 0, sizeof(s_log_lines));
    memset(s_log_lens, 0, sizeof(s_log_lens));
    nrf52_log_init(0);
    nrf52_log_set_level(NRF52_LOG_INFO);
}

void mock_set_now(uint32_t ms) { s_now = ms; }

bool mock_periodic_running(void) { return s_periodic_running; }
uint32_t mock_periodic_period(void) { return s_periodic_period; }
void mock_fire_periodic(void)
{
    if (s_periodic_cb) {
        s_periodic_cb(s_periodic_ctx);
    }
}

bool mock_gpio_registered(void) { return s_gpio_registered; }
uint16_t mock_gpio_pin(void) { return s_gpio_pin; }
nrf52_edge_t mock_gpio_edge(void) { return s_gpio_edge; }
void mock_fire_gpio_irq(void)
{
    if (s_gpio_cb) {
        s_gpio_cb(s_gpio_pin, s_gpio_ctx);
    }
}

void mock_set_i2c_response(const uint8_t *data, size_t len, nrf52_status_t status)
{
    s_i2c_resp_len = (len > sizeof(s_i2c_resp)) ? sizeof(s_i2c_resp) : len;
    if (data && len) {
        memcpy(s_i2c_resp, data, s_i2c_resp_len);
    }
    s_i2c_status = status;
}
uint8_t mock_i2c_last_addr(void) { return s_i2c_last_addr; }
uint8_t mock_i2c_last_reg(void) { return s_i2c_last_reg; }
size_t mock_i2c_last_rx_len(void) { return s_i2c_last_rx_len; }

void mock_set_log_port_status(nrf52_status_t status) { s_log_port_status = status; }
uint16_t mock_log_tx_pin(void) { return s_log_tx_pin; }
int mock_log_line_count(void) { return s_log_count; }

const char *mock_log_line(int index)
{
    return (index >= 0 && index < s_log_count && index < MOCK_LOG_MAX_LINES)
        ? s_log_lines[index] : "";
}

size_t mock_log_line_len(int index)
{
    return (index >= 0 && index < s_log_count && index < MOCK_LOG_MAX_LINES)
        ? s_log_lens[index] : 0;
}

int mock_warn_count(void)
{
    int n = 0;
    for (int i = 0; i < s_log_count && i < MOCK_LOG_MAX_LINES; i++) {
        const char *p = strchr(s_log_lines[i], ']');
        if (p && p[1] == ' ' && p[2] == 'W') {
            n++;
        }
    }
    return n;
}

/* ---- Replacements for SDK hardware functions ---- */

uint32_t nrf52_millis(void)
{
    return s_now;
}

nrf52_status_t nrf52_periodic_start(uint32_t period_ms, nrf52_periodic_cb_t cb, void *ctx)
{
    if (period_ms == 0 || !cb) {
        return NRF52_ERR_PARAM;
    }
    s_periodic_running = true;
    s_periodic_period = period_ms;
    s_periodic_cb = cb;
    s_periodic_ctx = ctx;
    return NRF52_OK;
}

void nrf52_periodic_stop(void)
{
    s_periodic_running = false;
    s_periodic_cb = NULL;
}

nrf52_status_t nrf52_gpiote_enable_irq(uint8_t ch, uint16_t pin, nrf52_pull_t pull,
                                       nrf52_edge_t edge, nrf52_gpio_irq_cb_t cb, void *ctx)
{
    (void)ch;
    (void)pull;
    if (!cb) {
        return NRF52_ERR_PARAM;
    }
    s_gpio_registered = true;
    s_gpio_pin = pin;
    s_gpio_edge = edge;
    s_gpio_cb = cb;
    s_gpio_ctx = ctx;
    return NRF52_OK;
}

nrf52_status_t nrf52_twim_write_read(uint8_t addr, const uint8_t *tx, size_t tx_len,
                                     uint8_t *rx, size_t rx_len)
{
    s_i2c_last_addr = addr;
    s_i2c_last_reg = (tx_len > 0) ? tx[0] : 0xFF;
    s_i2c_last_rx_len = rx_len;

    if (s_i2c_status != NRF52_OK) {
        return s_i2c_status;
    }
    for (size_t i = 0; i < rx_len; i++) {
        rx[i] = (i < s_i2c_resp_len) ? s_i2c_resp[i] : 0;
    }
    return NRF52_OK;
}

/* Fake log transport for the real src/nrf52_log.c. */
nrf52_status_t nrf52_log_port_init(uint16_t tx_pin)
{
    s_log_tx_pin = tx_pin;
    return s_log_port_status;
}

void nrf52_log_port_write(const uint8_t *data, size_t len)
{
    if (s_log_count < MOCK_LOG_MAX_LINES) {
        size_t n = (len < MOCK_LOG_LINE_SIZE - 1U) ? len : MOCK_LOG_LINE_SIZE - 1U;
        memcpy(s_log_lines[s_log_count], data, n);
        s_log_lines[s_log_count][n] = '\0';
        s_log_lens[s_log_count] = len;
    }
    s_log_count++;
}

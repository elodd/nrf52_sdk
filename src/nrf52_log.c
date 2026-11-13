/*
 * Logger: level filtering and line formatting. Hardware independent; the output
 * goes through nrf52_log_port_write() (src/nrf52_log_port.h).
 */
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "log/nrf52_log.h"
#include "nrf52_log_port.h"

#define LOG_MSG_SIZE            160U

static nrf52_log_level_t g_level = NRF52_LOG_INFO;
static bool g_ready;

nrf52_status_t nrf52_log_init(uint16_t tx_pin)
{
    g_ready = false;   /* a failed (re)init leaves logging off */

    nrf52_status_t st = nrf52_log_port_init(tx_pin);
    if (st != NRF52_OK) {
        return st;
    }
    g_ready = true;
    return NRF52_OK;
}

void nrf52_log_set_level(nrf52_log_level_t level)
{
    g_level = level;
}

void nrf52_log(nrf52_log_level_t level, const char *tag, const char *fmt, ...)
{
    static const char level_char[] = "EWID";
    char msg[LOG_MSG_SIZE];

    if (!g_ready || level > g_level) {
        return;
    }

    int n = snprintf(msg, sizeof(msg), "[%lu] %c %s: ",
                     (unsigned long)nrf52_millis(), level_char[level], tag);
    if (n < 0) {
        return;
    }

    size_t used = ((size_t)n < sizeof(msg)) ? (size_t)n : sizeof(msg) - 1U;

    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(msg + used, sizeof(msg) - used, fmt, ap);
    va_end(ap);

    if (m > 0) {
        used += (size_t)m;
    }
    if (used > sizeof(msg) - 3U) {
        used = sizeof(msg) - 3U;   /* truncate, leave room for CRLF */
    }

    msg[used++] = '\r';
    msg[used++] = '\n';
    nrf52_log_port_write((const uint8_t *)msg, used);
}

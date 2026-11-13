#include <stdbool.h>
#include <stdint.h>
#include "rtc/nrf52_rtc.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define CLOCK_BASE                  0x40000000UL
#define CLOCK_TASKS_LFCLKSTART      (CLOCK_BASE + 0x008UL)
#define CLOCK_LFCLKSTAT             (CLOCK_BASE + 0x418UL)
#define CLOCK_LFCLKSRC              (CLOCK_BASE + 0x518UL)
#define CLOCK_LFCLKSTAT_STATE       (1UL << 16)
#define LFCLK_SRC_XTAL              1UL

#define RTC1_BASE                   0x40011000UL
#define RTC_TASKS_START             (RTC1_BASE + 0x000UL)
#define RTC_TASKS_STOP              (RTC1_BASE + 0x004UL)
#define RTC_TASKS_CLEAR             (RTC1_BASE + 0x008UL)
#define RTC_EVENTS_COMPARE0         (RTC1_BASE + 0x140UL)
#define RTC_INTENSET                (RTC1_BASE + 0x304UL)
#define RTC_INTENCLR                (RTC1_BASE + 0x308UL)
#define RTC_COUNTER                 (RTC1_BASE + 0x504UL)
#define RTC_PRESCALER               (RTC1_BASE + 0x508UL)
#define RTC_CC0                     (RTC1_BASE + 0x540UL)

#define RTC_IRQN                    17U
#define RTC_IRQ_PRIORITY            2U
#define RTC_HZ                      32768UL
#define RTC_COUNTER_MASK            0xFFFFFFUL
#define LFCLK_TIMEOUT               1000000UL

static nrf52_periodic_cb_t g_cb;
static void *g_ctx;
static uint32_t g_ticks;

static nrf52_status_t lfclk_start(void)
{
    if (REG32(CLOCK_LFCLKSTAT) & CLOCK_LFCLKSTAT_STATE) {
        return NRF52_OK;
    }
    REG32(CLOCK_LFCLKSRC) = LFCLK_SRC_XTAL;
    REG32(CLOCK_TASKS_LFCLKSTART) = 1;

    for (uint32_t i = 0; i < LFCLK_TIMEOUT; i++) {
        if (REG32(CLOCK_LFCLKSTAT) & CLOCK_LFCLKSTAT_STATE) {
            return NRF52_OK;
        }
    }
    return NRF52_ERR_TIMEOUT;
}

void nrf52_periodic_stop(void)
{
    REG32(RTC_INTENCLR) = 1UL;
    REG32(RTC_TASKS_STOP) = 1;
    g_cb = 0;
}

nrf52_status_t nrf52_periodic_start(uint32_t period_ms, nrf52_periodic_cb_t cb, void *ctx)
{
    if (period_ms == 0 || !cb) {
        return NRF52_ERR_PARAM;
    }

    uint64_t ticks = ((uint64_t)period_ms * RTC_HZ) / 1000ULL;
    if (ticks == 0) {
        ticks = 1;
    }
    if (ticks > (RTC_COUNTER_MASK / 2U)) {
        return NRF52_ERR_PARAM;
    }

    nrf52_status_t st = lfclk_start();
    if (st != NRF52_OK) {
        return st;
    }

    nrf52_periodic_stop();

    g_cb = cb;
    g_ctx = ctx;
    g_ticks = (uint32_t)ticks;

    REG32(RTC_PRESCALER) = 0;          /* 32768 Hz */
    REG32(RTC_TASKS_CLEAR) = 1;
    REG32(RTC_EVENTS_COMPARE0) = 0;
    REG32(RTC_CC0) = (REG32(RTC_COUNTER) + g_ticks) & RTC_COUNTER_MASK;
    REG32(RTC_INTENSET) = 1UL;

    nrf52_irq_enable(RTC_IRQN, RTC_IRQ_PRIORITY);
    REG32(RTC_TASKS_START) = 1;
    return NRF52_OK;
}

void RTC1_IRQHandler(void)
{
    if (REG32(RTC_EVENTS_COMPARE0) != 0) {
        REG32(RTC_EVENTS_COMPARE0) = 0;

        /* Advance by a fixed step so the period does not drift. */
        REG32(RTC_CC0) = (REG32(RTC_CC0) + g_ticks) & RTC_COUNTER_MASK;

        if (g_cb) {
            g_cb(g_ctx);
        }
    }
}

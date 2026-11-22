/* PDM microphone (EasyDMA) for the nRF52840. Register-level code. */
#include <stdbool.h>
#include <stdint.h>
#include "pdm/nrf52_pdm.h"

#define REG32(addr) (*(volatile uint32_t *)(addr))

#define PDM_BASE                0x40026000UL
#define PDM_TASKS_START         (PDM_BASE + 0x000UL)
#define PDM_TASKS_STOP          (PDM_BASE + 0x004UL)
#define PDM_EVENTS_END          (PDM_BASE + 0x108UL)
#define PDM_ENABLE              (PDM_BASE + 0x500UL)
#define PDM_PDMCLKCTRL          (PDM_BASE + 0x504UL)
#define PDM_MODE                (PDM_BASE + 0x508UL)
#define PDM_RATIO               (PDM_BASE + 0x520UL)
#define PDM_GAINL               (PDM_BASE + 0x518UL)
#define PDM_GAINR               (PDM_BASE + 0x51CUL)
#define PDM_PSEL_CLK            (PDM_BASE + 0x540UL)
#define PDM_PSEL_DIN            (PDM_BASE + 0x544UL)
#define PDM_SAMPLE_PTR          (PDM_BASE + 0x560UL)
#define PDM_SAMPLE_MAXCNT       (PDM_BASE + 0x564UL)

#define PDM_ENABLE_VALUE        1UL
#define PDM_CLK_1032K           0x08400000UL   /* 1.032 MHz */
#define PDM_MODE_MONO           1UL            /* OPERATION=Mono, EDGE=LeftFalling */
#define PDM_RATIO_64            0UL
#define PDM_GAIN_DEFAULT        0x28UL         /* 0 dB */
#define PSEL_DISCONNECTED       0xFFFFFFFFUL

#define PDM_SPIN_LIMIT          20000000UL

static bool g_ready;

nrf52_status_t nrf52_pdm_init(uint16_t clk_pin, uint16_t din_pin)
{
    if (clk_pin == din_pin) {
        return NRF52_ERR_PARAM;
    }

    g_ready = false;
    nrf52_status_t st = nrf52_gpio_config_output(clk_pin, false);
    if (st != NRF52_OK) {
        return st;
    }
    st = nrf52_gpio_config_input(din_pin, NRF52_PULL_NONE);
    if (st != NRF52_OK) {
        return st;
    }

    REG32(PDM_ENABLE) = 0;
    REG32(PDM_PDMCLKCTRL) = PDM_CLK_1032K;
    REG32(PDM_MODE) = PDM_MODE_MONO;
    REG32(PDM_RATIO) = PDM_RATIO_64;
    REG32(PDM_GAINL) = PDM_GAIN_DEFAULT;
    REG32(PDM_GAINR) = PDM_GAIN_DEFAULT;
    REG32(PDM_PSEL_CLK) = clk_pin & 0x3FU;
    REG32(PDM_PSEL_DIN) = din_pin & 0x3FU;
    REG32(PDM_ENABLE) = PDM_ENABLE_VALUE;

    g_ready = true;
    return NRF52_OK;
}

nrf52_status_t nrf52_pdm_read(int16_t *samples, size_t count)
{
    if (!g_ready) {
        return NRF52_ERR_IO;
    }
    if (samples == NULL || count == 0 || count > NRF52_PDM_MAX_SAMPLES) {
        return NRF52_ERR_PARAM;
    }

    REG32(PDM_EVENTS_END) = 0;
    REG32(PDM_SAMPLE_PTR) = (uint32_t)(uintptr_t)samples;
    REG32(PDM_SAMPLE_MAXCNT) = (uint32_t)count;
    REG32(PDM_TASKS_START) = 1;

    for (uint32_t i = 0; i < PDM_SPIN_LIMIT && REG32(PDM_EVENTS_END) == 0; i++) {
    }

    bool done = REG32(PDM_EVENTS_END) != 0;
    REG32(PDM_TASKS_STOP) = 1;
    return done ? NRF52_OK : NRF52_ERR_TIMEOUT;
}

void nrf52_pdm_deinit(void)
{
    REG32(PDM_TASKS_STOP) = 1;
    REG32(PDM_ENABLE) = 0;
    REG32(PDM_PSEL_CLK) = PSEL_DISCONNECTED;
    REG32(PDM_PSEL_DIN) = PSEL_DISCONNECTED;
    g_ready = false;
}

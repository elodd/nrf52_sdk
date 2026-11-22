/*
 * Microphone application: records PDM microphone samples and logs amplitude
 * statistics over UART every 250 ms. Each line contains "mic:", which is what
 * the Yocto receiver in yoctolayer/ filters on.
 *
 * Pins are for the nRF52840 DK. Change them to match your wiring.
 */
#include <stdint.h>
#include "core/nrf52_sdk.h"
#include "log/nrf52_log.h"
#include "pdm/nrf52_pdm.h"

#define APP_LOG_TX_PIN   NRF52_PIN(0, 6)
#define APP_PDM_CLK_PIN  NRF52_PIN(0, 25)
#define APP_PDM_DIN_PIN  NRF52_PIN(0, 24)
#define APP_CHUNK        256U        /* samples per DMA transfer, about 16 ms */
#define APP_REPORT_MS    250UL       /* one log line per report interval */
#define APP_RETRY_MS     100UL       /* back-off after a failed transfer */

typedef struct {
    uint32_t peak;                   /* largest |sample| in the window */
    uint64_t sum_abs;                /* sum of |sample| over the window */
    uint32_t count;                  /* samples in the window */
    int16_t  last[4];                /* first raw samples of the latest chunk */
} mic_stats_t;

static int16_t chunk[APP_CHUNK];
static mic_stats_t stats;

static void stats_add(mic_stats_t *st, const int16_t *s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        int32_t v = s[i];
        uint32_t a = (v < 0) ? (uint32_t)(-v) : (uint32_t)v;
        if (a > st->peak) {
            st->peak = a;
        }
        st->sum_abs += a;
    }
    st->count += (uint32_t)n;
    for (size_t i = 0; i < 4U && i < n; i++) {
        st->last[i] = s[i];
    }
}

static void stats_report(const mic_stats_t *st, uint32_t now_ms)
{
    uint32_t avg = (uint32_t)(st->sum_abs / st->count);
    NRF52_LOGI("mic", "t=%lu ms n=%lu peak=%lu avg=%lu raw=%d,%d,%d,%d",
               (unsigned long)now_ms, (unsigned long)st->count,
               (unsigned long)st->peak, (unsigned long)avg,
               st->last[0], st->last[1], st->last[2], st->last[3]);
}

int main(void)
{
    if (nrf52_init() != NRF52_OK) {
        for (;;) { }
    }

    nrf52_log_init(APP_LOG_TX_PIN);
    if (nrf52_pdm_init(APP_PDM_CLK_PIN, APP_PDM_DIN_PIN) != NRF52_OK) {
        NRF52_LOGE("app", "pdm init failed");
        for (;;) { }
    }

    NRF52_LOGI("app", "mic app started");
    uint32_t window_start = nrf52_millis();

    for (;;) {
        if (nrf52_pdm_read(chunk, APP_CHUNK) == NRF52_OK) {
            stats_add(&stats, chunk, APP_CHUNK);
        } else {
            NRF52_LOGW("app", "pdm read failed");
            nrf52_delay_ms(APP_RETRY_MS);
        }

        uint32_t now = nrf52_millis();
        if ((uint32_t)(now - window_start) >= APP_REPORT_MS && stats.count > 0U) {
            stats_report(&stats, now);
            stats = (mic_stats_t){0};
            window_start = now;
        }
        nrf52_wait_for_interrupt();
    }
}

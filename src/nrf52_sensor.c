#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "sensor/nrf52_sensor.h"
#include "log/nrf52_log.h"
#include "rtc/nrf52_rtc.h"
#include <string.h>

#if defined(__arm__)  /* 32-bit ARM only; arm64 hosts also define __ARM_ARCH */
#define IRQ_DISABLE() __asm__ volatile("cpsid i" ::: "memory")
#define IRQ_ENABLE()  __asm__ volatile("cpsie i" ::: "memory")
#else
/* Host builds (unit tests) have no interrupts to mask. */
#define IRQ_DISABLE() do { } while (0)
#define IRQ_ENABLE()  do { } while (0)
#endif

typedef struct {
    nrf52_sensor_t *sensor;
    uint32_t elapsed_ms;
    volatile uint8_t pending;
} sensor_slot_t;

static sensor_slot_t g_slots[NRF52_SENSOR_MAX_SENSORS];
static uint8_t g_count;
static uint32_t g_tick_ms;

static sensor_slot_t *find_slot(const nrf52_sensor_t *s)
{
    for (uint8_t i = 0; i < g_count; i++) {
        if (g_slots[i].sensor == s) {
            return &g_slots[i];
        }
    }
    return NULL;
}

/* Interrupt context: only marks sensors that are due. */
static void scheduler_tick(void *ctx)
{
    (void)ctx;
    for (uint8_t i = 0; i < g_count; i++) {
        sensor_slot_t *slot = &g_slots[i];
        if (slot->sensor->period_ms == 0) {
            continue;
        }
        slot->elapsed_ms += g_tick_ms;
        if (slot->elapsed_ms >= slot->sensor->period_ms) {
            slot->elapsed_ms -= slot->sensor->period_ms;
            slot->pending = 1;
        }
    }
}

/* Interrupt context: data-ready pin triggered. */
static void data_ready_isr(uint16_t pin, void *ctx)
{
    (void)pin;
    ((sensor_slot_t *)ctx)->pending = 1;
}

void nrf52_sensor_deinit(void)
{
    nrf52_periodic_stop();
    memset(g_slots, 0, sizeof(g_slots));
    g_count = 0;
    g_tick_ms = 0;
}

nrf52_status_t nrf52_sensor_init(uint32_t tick_ms)
{
    if (tick_ms == 0) {
        return NRF52_ERR_PARAM;
    }
    g_tick_ms = tick_ms;
    return nrf52_periodic_start(tick_ms, scheduler_tick, NULL);
}

nrf52_status_t nrf52_sensor_register(nrf52_sensor_t *s)
{
    if (!s || !s->name || !s->read || g_count >= NRF52_SENSOR_MAX_SENSORS) {
        return NRF52_ERR_PARAM;
    }
    if (find_slot(s)) {
        return NRF52_OK;
    }

    sensor_slot_t *slot = &g_slots[g_count++];
    slot->sensor = s;
    slot->elapsed_ms = 0;
    slot->pending = 0;
    return NRF52_OK;
}

nrf52_status_t nrf52_sensor_attach_irq(nrf52_sensor_t *s, uint8_t ch,
                                       uint16_t pin, nrf52_pull_t pull, nrf52_edge_t edge)
{
    sensor_slot_t *slot = find_slot(s);
    if (!slot) {
        return NRF52_ERR_PARAM;
    }
    return nrf52_gpiote_enable_irq(ch, pin, pull, edge, data_ready_isr, slot);
}

void nrf52_sensor_request(nrf52_sensor_t *s)
{
    sensor_slot_t *slot = find_slot(s);
    if (slot) {
        slot->pending = 1;
    }
}

void nrf52_sensor_poll(void)
{
    for (uint8_t i = 0; i < g_count; i++) {
        sensor_slot_t *slot = &g_slots[i];

        /* Read and clear the flag with interrupts masked to avoid losing an event. */
        IRQ_DISABLE();
        uint8_t pending = slot->pending;
        slot->pending = 0;
        IRQ_ENABLE();

        if (!pending) {
            continue;
        }

        nrf52_sensor_t *s = slot->sensor;
        nrf52_sensor_sample_t sample = {0};
        sample.timestamp_ms = nrf52_millis();

        nrf52_status_t st = s->read(s->dev, &sample);
        if (st != NRF52_OK) {
            NRF52_LOGW("sensor", "%s read failed (status %d)", s->name, (int)st);
            continue;
        }
        if (s->on_sample) {
            s->on_sample(s, &sample, s->cb_ctx);
        }
    }
}

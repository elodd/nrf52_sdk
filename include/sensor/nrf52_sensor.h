#ifndef NRF52_SENSOR_H
#define NRF52_SENSOR_H

#include <stdint.h>
#include "core/nrf52_sdk.h"
#include "gpiote/nrf52_gpiote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NRF52_SENSOR_MAX_CHANNELS   3
#define NRF52_SENSOR_MAX_SENSORS    4

typedef struct {
    uint32_t timestamp_ms;
    uint8_t  channels;
    int32_t  value[NRF52_SENSOR_MAX_CHANNELS];
} nrf52_sensor_sample_t;

typedef struct nrf52_sensor nrf52_sensor_t;

/* Called from the main loop (not from an interrupt). Blocking I/O is fine. */
typedef nrf52_status_t (*nrf52_sensor_read_fn)(void *dev, nrf52_sensor_sample_t *out);
typedef void (*nrf52_sensor_cb_t)(const nrf52_sensor_t *sensor,
                                  const nrf52_sensor_sample_t *sample,
                                  void *ctx);

struct nrf52_sensor {
    const char *name;
    void *dev;                  /* driver-private data, may be NULL */
    nrf52_sensor_read_fn read;  /* required */
    nrf52_sensor_cb_t on_sample;
    void *cb_ctx;
    uint32_t period_ms;         /* 0 = only sampled on request or data-ready IRQ */
};

/* Stops the scheduler and removes all registered sensors. */
void nrf52_sensor_deinit(void);

/* Starts the scheduler. tick_ms is the base resolution for periods. */
nrf52_status_t nrf52_sensor_init(uint32_t tick_ms);

nrf52_status_t nrf52_sensor_register(nrf52_sensor_t *sensor);

/* Data-ready interrupt on a GPIO pin triggers a sample. */
nrf52_status_t nrf52_sensor_attach_irq(nrf52_sensor_t *sensor, uint8_t gpiote_channel,
                                       uint16_t pin, nrf52_pull_t pull, nrf52_edge_t edge);

/* Requests one sample on the next poll. Safe to call from an interrupt. */
void nrf52_sensor_request(nrf52_sensor_t *sensor);

/* Reads all pending sensors and calls their callbacks. Call from the main loop. */
void nrf52_sensor_poll(void);

#ifdef __cplusplus
}
#endif

#endif /* NRF52_SENSOR_H */

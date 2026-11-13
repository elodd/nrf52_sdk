#ifndef NRF52_LOG_H
#define NRF52_LOG_H

#include <stdint.h>
#include "core/nrf52_sdk.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NRF52_LOG_ERROR = 0,
    NRF52_LOG_WARN,
    NRF52_LOG_INFO,
    NRF52_LOG_DEBUG,
} nrf52_log_level_t;

/* UARTE0, 115200 8N1, TX only. Blocking. Default level: INFO. */
nrf52_status_t nrf52_log_init(uint16_t tx_pin);
void nrf52_log_set_level(nrf52_log_level_t level);

/* Output: "[uptime_ms] L tag: message\r\n". Integer formats only. */
void nrf52_log(nrf52_log_level_t level, const char *tag, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#define NRF52_LOGE(tag, ...) nrf52_log(NRF52_LOG_ERROR, (tag), __VA_ARGS__)
#define NRF52_LOGW(tag, ...) nrf52_log(NRF52_LOG_WARN,  (tag), __VA_ARGS__)
#define NRF52_LOGI(tag, ...) nrf52_log(NRF52_LOG_INFO,  (tag), __VA_ARGS__)
#define NRF52_LOGD(tag, ...) nrf52_log(NRF52_LOG_DEBUG, (tag), __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* NRF52_LOG_H */

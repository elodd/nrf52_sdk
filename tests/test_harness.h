#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stddef.h>
#include <stdio.h>
#include "sensor/nrf52_sensor.h"
#include "test_support.h"

/* Shared test infrastructure. Defined in test_main.c. */

typedef void (*test_fn_t)(void);

typedef struct {
    const char *name;
    test_fn_t fn;
} test_case_t;

extern int g_checks;
extern int g_failed;
extern int g_samples;
extern nrf52_sensor_sample_t g_last_sample;

#define CHECK(cond) do {                                                       \
        g_checks++;                                                            \
        if (!(cond)) {                                                         \
            g_failed++;                                                        \
            fprintf(stderr, "  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                                      \
    } while (0)

#define TEST_COUNT(a) (sizeof(a) / sizeof((a)[0]))

void test_on_sample(const nrf52_sensor_t *s, const nrf52_sensor_sample_t *smp, void *ctx);
nrf52_status_t test_dummy_read(void *dev, nrf52_sensor_sample_t *out);
nrf52_sensor_t test_make_sensor(const char *name, uint32_t period_ms);

/* Test cases of each suite, defined in test_driver.c, test_sensor.c and test_log.c. */
extern const test_case_t g_driver_tests[];
extern const size_t g_driver_test_count;
extern const test_case_t g_sensor_tests[];
extern const size_t g_sensor_test_count;
extern const test_case_t g_log_tests[];
extern const size_t g_log_test_count;

#endif /* TEST_HARNESS_H */

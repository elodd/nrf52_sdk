/*
 * Test runner. Runs each suite with a clean SDK state before every test.
 * Suites: test_driver.c (I2C register driver), test_sensor.c (scheduler and registry),
 * test_log.c (logger).
 */
#include <string.h>
#include "test_harness.h"

int g_checks;
int g_failed;
int g_samples;
nrf52_sensor_sample_t g_last_sample;

void test_on_sample(const nrf52_sensor_t *s, const nrf52_sensor_sample_t *smp, void *ctx)
{
    (void)s;
    (void)ctx;
    g_samples++;
    g_last_sample = *smp;
}

nrf52_status_t test_dummy_read(void *dev, nrf52_sensor_sample_t *out)
{
    (void)dev;
    out->channels = 1;
    out->value[0] = 42;
    return NRF52_OK;
}

nrf52_sensor_t test_make_sensor(const char *name, uint32_t period_ms)
{
    nrf52_sensor_t s;
    memset(&s, 0, sizeof(s));
    s.name = name;
    s.read = test_dummy_read;
    s.on_sample = test_on_sample;
    s.period_ms = period_ms;
    return s;
}

static void setup(void)
{
    nrf52_sensor_deinit();
    mock_reset();
    g_samples = 0;
    memset(&g_last_sample, 0, sizeof(g_last_sample));
}

static void run_suite(const char *suite, const test_case_t *cases, size_t n)
{
    printf("[%s]\n", suite);
    for (size_t i = 0; i < n; i++) {
        setup();
        int before = g_failed;
        cases[i].fn();
        printf("  %-34s %s\n", cases[i].name, (g_failed == before) ? "ok" : "FAILED");
    }
    printf("\n");
}

int main(void)
{
    run_suite("driver", g_driver_tests, g_driver_test_count);
    run_suite("sensor", g_sensor_tests, g_sensor_test_count);
    run_suite("log", g_log_tests, g_log_test_count);

    nrf52_sensor_deinit();
    printf("%d checks, %d failed\n", g_checks, g_failed);
    return (g_failed == 0) ? 0 : 1;
}

/* Tests for the sensor scheduler, registry and data-ready triggers (src/nrf52_sensor.c). */
#include <string.h>
#include "test_harness.h"

static void test_register_rejects_invalid(void)
{
    nrf52_sensor_t bad = test_make_sensor("bad", 0);
    bad.read = NULL;

    CHECK(nrf52_sensor_register(NULL) == NRF52_ERR_PARAM);
    CHECK(nrf52_sensor_register(&bad) == NRF52_ERR_PARAM);
}

static void test_register_limit_and_duplicates(void)
{
    nrf52_sensor_t a = test_make_sensor("a", 0);
    nrf52_sensor_t b = test_make_sensor("b", 0);
    nrf52_sensor_t c = test_make_sensor("c", 0);
    nrf52_sensor_t d = test_make_sensor("d", 0);
    nrf52_sensor_t e = test_make_sensor("e", 0);

    CHECK(nrf52_sensor_register(&a) == NRF52_OK);
    CHECK(nrf52_sensor_register(&a) == NRF52_OK);   /* duplicate is a no-op */
    CHECK(nrf52_sensor_register(&b) == NRF52_OK);
    CHECK(nrf52_sensor_register(&c) == NRF52_OK);
    CHECK(nrf52_sensor_register(&d) == NRF52_OK);   /* 4 sensors total */
    CHECK(nrf52_sensor_register(&e) == NRF52_ERR_PARAM);
}

static void test_deinit_stops_scheduler(void)
{
    CHECK(nrf52_sensor_init(10) == NRF52_OK);
    CHECK(mock_periodic_running());
    nrf52_sensor_deinit();
    CHECK(!mock_periodic_running());
}

static void test_init_rejects_zero_tick(void)
{
    CHECK(nrf52_sensor_init(0) == NRF52_ERR_PARAM);
    CHECK(!mock_periodic_running());
}

static void test_init_starts_tick(void)
{
    CHECK(nrf52_sensor_init(10) == NRF52_OK);
    CHECK(mock_periodic_running());
    CHECK(mock_periodic_period() == 10);
}

static void test_periodic_sampling_phase(void)
{
    nrf52_sensor_t s = test_make_sensor("p", 30);

    CHECK(nrf52_sensor_init(10) == NRF52_OK);
    CHECK(nrf52_sensor_register(&s) == NRF52_OK);

    /* Period 30 ms with a 10 ms tick: one sample every third tick. */
    for (int tick = 1; tick <= 9; tick++) {
        mock_fire_periodic();
        nrf52_sensor_poll();
        CHECK(g_samples == tick / 3);
    }
}

static void test_zero_period_is_not_periodic(void)
{
    nrf52_sensor_t s = test_make_sensor("z", 0);

    CHECK(nrf52_sensor_init(10) == NRF52_OK);
    CHECK(nrf52_sensor_register(&s) == NRF52_OK);

    for (int i = 0; i < 10; i++) {
        mock_fire_periodic();
    }
    nrf52_sensor_poll();
    CHECK(g_samples == 0);
}

static void test_request_is_one_shot(void)
{
    nrf52_sensor_t s = test_make_sensor("r", 0);

    CHECK(nrf52_sensor_register(&s) == NRF52_OK);
    nrf52_sensor_request(&s);
    nrf52_sensor_poll();
    nrf52_sensor_poll();
    CHECK(g_samples == 1);
}

static void test_idle_poll_does_nothing(void)
{
    nrf52_sensor_t s = test_make_sensor("i", 0);

    CHECK(nrf52_sensor_register(&s) == NRF52_OK);
    nrf52_sensor_poll();
    CHECK(g_samples == 0);
}

static void test_timestamp_is_taken_at_poll(void)
{
    nrf52_sensor_t s = test_make_sensor("t", 0);

    CHECK(nrf52_sensor_register(&s) == NRF52_OK);
    mock_set_now(500);
    nrf52_sensor_request(&s);
    nrf52_sensor_poll();
    CHECK(g_last_sample.timestamp_ms == 500);
}

static void test_data_ready_irq_triggers_sample(void)
{
    nrf52_sensor_t s = test_make_sensor("drdy", 0);
    uint16_t pin = NRF52_PIN(0, 11);

    CHECK(nrf52_sensor_register(&s) == NRF52_OK);
    CHECK(nrf52_sensor_attach_irq(&s, 0, pin, NRF52_PULL_UP, NRF52_EDGE_FALLING) == NRF52_OK);
    CHECK(mock_gpio_registered());
    CHECK(mock_gpio_pin() == pin);
    CHECK(mock_gpio_edge() == NRF52_EDGE_FALLING);

    mock_fire_gpio_irq();
    nrf52_sensor_poll();
    CHECK(g_samples == 1);
}

static void test_attach_irq_unknown_sensor(void)
{
    nrf52_sensor_t s = test_make_sensor("missing", 0);

    CHECK(nrf52_sensor_attach_irq(&s, 0, NRF52_PIN(0, 11), NRF52_PULL_UP,
                                  NRF52_EDGE_FALLING) == NRF52_ERR_PARAM);
    CHECK(!mock_gpio_registered());
}

const test_case_t g_sensor_tests[] = {
    { "register_rejects_invalid",       test_register_rejects_invalid },
    { "register_limit_and_duplicates",  test_register_limit_and_duplicates },
    { "deinit_stops_scheduler",         test_deinit_stops_scheduler },
    { "init_rejects_zero_tick",         test_init_rejects_zero_tick },
    { "init_starts_tick",               test_init_starts_tick },
    { "periodic_sampling_phase",        test_periodic_sampling_phase },
    { "zero_period_is_not_periodic",    test_zero_period_is_not_periodic },
    { "request_is_one_shot",            test_request_is_one_shot },
    { "idle_poll_does_nothing",         test_idle_poll_does_nothing },
    { "timestamp_is_taken_at_poll",     test_timestamp_is_taken_at_poll },
    { "data_ready_irq_triggers_sample", test_data_ready_irq_triggers_sample },
    { "attach_irq_unknown_sensor",      test_attach_irq_unknown_sensor },
};
const size_t g_sensor_test_count = TEST_COUNT(g_sensor_tests);

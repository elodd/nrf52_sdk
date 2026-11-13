/* Tests for the I2C register sensor driver (drivers/nrf52_i2c_reg_sensor.c). */
#include <string.h>
#include "drivers/nrf52_i2c_reg_sensor.h"
#include "test_harness.h"

static void test_rejects_bad_config(void)
{
    nrf52_i2c_reg_dev_t dev;
    nrf52_sensor_t sensor;
    nrf52_i2c_reg_cfg_t cfg = { .addr = 0x44, .reg = 0x00, .length = 2 };

    CHECK(nrf52_i2c_reg_sensor_init(NULL, &cfg, &sensor, "t", 100) == NRF52_ERR_PARAM);
    CHECK(nrf52_i2c_reg_sensor_init(&dev, NULL, &sensor, "t", 100) == NRF52_ERR_PARAM);
    CHECK(nrf52_i2c_reg_sensor_init(&dev, &cfg, NULL, "t", 100) == NRF52_ERR_PARAM);
    CHECK(nrf52_i2c_reg_sensor_init(&dev, &cfg, &sensor, NULL, 100) == NRF52_ERR_PARAM);

    cfg.addr = 0x80;                 /* 7-bit address must be <= 0x7F */
    CHECK(nrf52_i2c_reg_sensor_init(&dev, &cfg, &sensor, "t", 100) == NRF52_ERR_PARAM);
    cfg.addr = 0x44;
    cfg.length = 0;
    CHECK(nrf52_i2c_reg_sensor_init(&dev, &cfg, &sensor, "t", 100) == NRF52_ERR_PARAM);
    cfg.length = 3;
    CHECK(nrf52_i2c_reg_sensor_init(&dev, &cfg, &sensor, "t", 100) == NRF52_ERR_PARAM);
}

static void test_fills_sensor_fields(void)
{
    nrf52_i2c_reg_dev_t dev;
    nrf52_sensor_t sensor;
    nrf52_i2c_reg_cfg_t cfg = { .addr = 0x44, .reg = 0x05, .length = 2 };

    CHECK(nrf52_i2c_reg_sensor_init(&dev, &cfg, &sensor, "temp", 250) == NRF52_OK);
    CHECK(strcmp(sensor.name, "temp") == 0);
    CHECK(sensor.dev == &dev);
    CHECK(sensor.read != NULL);
    CHECK(sensor.period_ms == 250);
    CHECK(dev.cfg.addr == 0x44 && dev.cfg.reg == 0x05 && dev.cfg.length == 2);
}

static void test_reads_16bit_big_endian(void)
{
    const uint8_t data[2] = { 0x12, 0x34 };
    nrf52_i2c_reg_dev_t dev;
    nrf52_sensor_t sensor;
    nrf52_i2c_reg_cfg_t cfg = { .addr = 0x44, .reg = 0x00, .length = 2 };

    mock_set_i2c_response(data, sizeof(data), NRF52_OK);
    CHECK(nrf52_i2c_reg_sensor_init(&dev, &cfg, &sensor, "temp", 0) == NRF52_OK);
    sensor.on_sample = test_on_sample;
    CHECK(nrf52_sensor_register(&sensor) == NRF52_OK);

    nrf52_sensor_request(&sensor);
    nrf52_sensor_poll();

    CHECK(g_samples == 1);
    CHECK(g_last_sample.channels == 1);
    CHECK(g_last_sample.value[0] == 0x1234);
    CHECK(mock_i2c_last_addr() == 0x44);
    CHECK(mock_i2c_last_reg() == 0x00);
    CHECK(mock_i2c_last_rx_len() == 2);
}

static void test_reads_8bit(void)
{
    const uint8_t data[1] = { 0xAB };
    nrf52_i2c_reg_dev_t dev;
    nrf52_sensor_t sensor;
    nrf52_i2c_reg_cfg_t cfg = { .addr = 0x10, .reg = 0x01, .length = 1 };

    mock_set_i2c_response(data, sizeof(data), NRF52_OK);
    CHECK(nrf52_i2c_reg_sensor_init(&dev, &cfg, &sensor, "one", 0) == NRF52_OK);
    sensor.on_sample = test_on_sample;
    CHECK(nrf52_sensor_register(&sensor) == NRF52_OK);

    nrf52_sensor_request(&sensor);
    nrf52_sensor_poll();

    CHECK(g_samples == 1);
    CHECK(g_last_sample.value[0] == 0xAB);
    CHECK(mock_i2c_last_rx_len() == 1);
}

static void test_bus_error_skips_callback(void)
{
    nrf52_i2c_reg_dev_t dev;
    nrf52_sensor_t sensor;
    nrf52_i2c_reg_cfg_t cfg = { .addr = 0x44, .reg = 0x00, .length = 2 };

    mock_set_i2c_response(NULL, 0, NRF52_ERR_IO);
    CHECK(nrf52_i2c_reg_sensor_init(&dev, &cfg, &sensor, "temp", 0) == NRF52_OK);
    sensor.on_sample = test_on_sample;
    CHECK(nrf52_sensor_register(&sensor) == NRF52_OK);

    nrf52_sensor_request(&sensor);
    nrf52_sensor_poll();

    CHECK(g_samples == 0);
    CHECK(mock_warn_count() == 1);
}

const test_case_t g_driver_tests[] = {
    { "rejects_bad_config",       test_rejects_bad_config },
    { "fills_sensor_fields",      test_fills_sensor_fields },
    { "reads_16bit_big_endian",   test_reads_16bit_big_endian },
    { "reads_8bit",               test_reads_8bit },
    { "bus_error_skips_callback", test_bus_error_skips_callback },
};
const size_t g_driver_test_count = TEST_COUNT(g_driver_tests);

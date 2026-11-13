/*
 * Example application: reads an I2C sensor once per second, logs the value
 * over UART and toggles an LED. The application contains no register or
 * I2C code. It only uses SDK and driver APIs.
 *
 * Pins and the sensor address are placeholders for the nRF52840 DK.
 * Change them to match your board and sensor datasheet.
 */
#include "core/nrf52_sdk.h"
#include "log/nrf52_log.h"
#include "twim/nrf52_twim.h"
#include "sensor/nrf52_sensor.h"
#include "drivers/nrf52_i2c_reg_sensor.h"

#define APP_LED_PIN     NRF52_PIN(0, 13)
#define APP_LOG_TX_PIN  NRF52_PIN(0, 6)
#define APP_I2C_SCL     NRF52_PIN(0, 27)
#define APP_I2C_SDA     NRF52_PIN(0, 26)
#define APP_SAMPLE_MS   1000UL

static const nrf52_i2c_reg_cfg_t TEMP_CFG = {
    .addr   = 0x44,
    .reg    = 0x00,
    .length = 2,
};

static nrf52_i2c_reg_dev_t temp_dev;
static nrf52_sensor_t temp_sensor;

static void on_temp_sample(const nrf52_sensor_t *sensor,
                           const nrf52_sensor_sample_t *sample,
                           void *ctx)
{
    (void)ctx;
    nrf52_gpio_toggle(APP_LED_PIN);
    NRF52_LOGI(sensor->name, "t=%lu ms value=%ld",
               (unsigned long)sample->timestamp_ms, (long)sample->value[0]);
}

int main(void)
{
    if (nrf52_init() != NRF52_OK) {
        for (;;) { }
    }

    nrf52_log_init(APP_LOG_TX_PIN);
    nrf52_gpio_config_output(APP_LED_PIN, false);
    nrf52_twim_init(APP_I2C_SCL, APP_I2C_SDA, 400000UL);

    nrf52_i2c_reg_sensor_init(&temp_dev, &TEMP_CFG, &temp_sensor, "temp", APP_SAMPLE_MS);
    temp_sensor.on_sample = on_temp_sample;

    nrf52_sensor_init(10);              /* scheduler resolution: 10 ms */
    nrf52_sensor_register(&temp_sensor);

    NRF52_LOGI("app", "sensor app started");

    for (;;) {
        nrf52_sensor_poll();
        nrf52_wait_for_interrupt();
    }
}

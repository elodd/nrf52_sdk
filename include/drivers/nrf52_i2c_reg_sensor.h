#ifndef NRF52_I2C_REG_SENSOR_H
#define NRF52_I2C_REG_SENSOR_H

#include <stdint.h>
#include "core/nrf52_sdk.h"
#include "sensor/nrf52_sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Driver for an I2C sensor that returns its value from one register.
 * Reads 1 or 2 bytes, big-endian, and reports them as channel 0.
 * Use it as-is for simple sensors, or copy it as a starting point for a
 * chip-specific driver with conversion and configuration.
 */
typedef struct {
    uint8_t addr;     /* 7-bit I2C address */
    uint8_t reg;      /* register holding the value */
    uint8_t length;   /* 1 or 2 bytes */
} nrf52_i2c_reg_cfg_t;

typedef struct {
    nrf52_i2c_reg_cfg_t cfg;
} nrf52_i2c_reg_dev_t;

/*
 * Fills in `sensor` (name, dev, read, period_ms). The caller still sets
 * on_sample/cb_ctx and calls nrf52_sensor_register().
 * `dev` and `sensor` must stay valid for the program lifetime.
 */
nrf52_status_t nrf52_i2c_reg_sensor_init(nrf52_i2c_reg_dev_t *dev,
                                         const nrf52_i2c_reg_cfg_t *cfg,
                                         nrf52_sensor_t *sensor,
                                         const char *name,
                                         uint32_t period_ms);

#ifdef __cplusplus
}
#endif

#endif /* NRF52_I2C_REG_SENSOR_H */

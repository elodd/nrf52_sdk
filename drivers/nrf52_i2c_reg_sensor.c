#include <stddef.h>
#include "drivers/nrf52_i2c_reg_sensor.h"
#include "twim/nrf52_twim.h"

static nrf52_status_t read_register(void *p, nrf52_sensor_sample_t *out)
{
    nrf52_i2c_reg_dev_t *dev = (nrf52_i2c_reg_dev_t *)p;
    uint8_t reg = dev->cfg.reg;
    uint8_t buf[2] = {0, 0};

    nrf52_status_t st = nrf52_twim_write_read(dev->cfg.addr, &reg, 1, buf, dev->cfg.length);
    if (st != NRF52_OK) {
        return st;
    }

    uint32_t value = 0;
    for (uint8_t i = 0; i < dev->cfg.length; i++) {
        value = (value << 8) | buf[i];
    }

    out->channels = 1;
    out->value[0] = (int32_t)value;
    return NRF52_OK;
}

nrf52_status_t nrf52_i2c_reg_sensor_init(nrf52_i2c_reg_dev_t *dev,
                                         const nrf52_i2c_reg_cfg_t *cfg,
                                         nrf52_sensor_t *sensor,
                                         const char *name,
                                         uint32_t period_ms)
{
    if (!dev || !cfg || !sensor || !name) {
        return NRF52_ERR_PARAM;
    }
    if (cfg->addr > 0x7FU || cfg->length < 1 || cfg->length > 2) {
        return NRF52_ERR_PARAM;
    }

    dev->cfg = *cfg;

    sensor->name = name;
    sensor->dev = dev;
    sensor->read = read_register;
    sensor->period_ms = period_ms;
    return NRF52_OK;
}

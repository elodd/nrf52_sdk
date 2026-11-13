# nRF52 SDK API reference

All public headers are in `include/`. Applications include only these headers.
Functions that return `nrf52_status_t` return `NRF52_OK` on success.

## Common types (`core/nrf52_sdk.h`)

| Name | Description |
|---|---|
| `nrf52_status_t` | `NRF52_OK`, `NRF52_ERR_PARAM`, `NRF52_ERR_TIMEOUT`, `NRF52_ERR_IO` |
| `nrf52_pull_t` | `NRF52_PULL_NONE`, `NRF52_PULL_DOWN`, `NRF52_PULL_UP` |
| `NRF52_PIN(port, pin)` | Encodes a pin as `port * 32 + pin`, for example `NRF52_PIN(1, 6)` |
| `NRF52_SDK_VERSION_MAJOR/MINOR/PATCH` | SDK version |
| `NRF52_HAS_P1` | `1` on nRF52840 (default), `0` for port-0-only parts |
| `NRF52_CPU_HZ` | CPU clock used for SysTick, default 64 MHz |

## System and GPIO (`core/nrf52_sdk.h`)

| Function | Description |
|---|---|
| `nrf52_status_t nrf52_init(void)` | Starts the 64 MHz crystal and the 1 kHz SysTick. Call first. |
| `uint32_t nrf52_millis(void)` | Milliseconds since `nrf52_init()`. Wraps after about 49 days. |
| `void nrf52_delay_ms(uint32_t ms)` | Blocks for at least `ms`, using `wfi` between ticks. |
| `void nrf52_wait_for_interrupt(void)` | Sleeps until the next interrupt (`wfi`). Call it at the end of the main loop. |
| `void nrf52_irq_enable(uint8_t irqn, uint8_t priority)` | Enables an NVIC interrupt. Priority 0 (highest) to 7. |
| `nrf52_status_t nrf52_gpio_config_input(uint16_t pin, nrf52_pull_t pull)` | Configures an input with a pull. |
| `nrf52_status_t nrf52_gpio_config_output(uint16_t pin, bool initial_high)` | Configures an output and sets its level. |
| `void nrf52_gpio_write(uint16_t pin, bool high)` | Sets an output level. |
| `void nrf52_gpio_toggle(uint16_t pin)` | Inverts an output. |
| `bool nrf52_gpio_read(uint16_t pin)` | Reads the input level. Returns `false` for an invalid pin. |

## I2C master (`twim/nrf52_twim.h`)

Uses TWIM0 with EasyDMA. Blocking, with a 10 ms timeout per transfer.

| Function | Description |
|---|---|
| `nrf52_status_t nrf52_twim_init(uint16_t scl, uint16_t sda, uint32_t freq_hz)` | `freq_hz` must be `100000` or `400000`. Pins are open-drain with internal pull-ups. |
| `nrf52_status_t nrf52_twim_write(uint8_t addr, const uint8_t *data, size_t len)` | Writes `len` bytes to a 7-bit address. |
| `nrf52_status_t nrf52_twim_read(uint8_t addr, uint8_t *data, size_t len)` | Reads `len` bytes. |
| `nrf52_status_t nrf52_twim_write_read(uint8_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len)` | Write, then read with a repeated start. |

Buffers must be in RAM. A `static const` table in flash will not work with EasyDMA.
Returns `NRF52_ERR_IO` on NACK or bus error and `NRF52_ERR_TIMEOUT` if the transfer does not finish.

## Logging (`log/nrf52_log.h`)

UARTE0 at 115200 8N1, transmit only, blocking.

| Function / macro | Description |
|---|---|
| `nrf52_status_t nrf52_log_init(uint16_t tx_pin)` | Configures the UART and the TX pin. |
| `void nrf52_log_set_level(nrf52_log_level_t level)` | Sets the maximum level printed. Default `NRF52_LOG_INFO`. |
| `void nrf52_log(level, tag, fmt, ...)` | Prints `[uptime_ms] L tag: message`. Lines longer than about 150 characters are truncated. |
| `NRF52_LOGE / LOGW / LOGI / LOGD (tag, fmt, ...)` | Shortcuts for each level. |

Levels: `NRF52_LOG_ERROR`, `NRF52_LOG_WARN`, `NRF52_LOG_INFO`, `NRF52_LOG_DEBUG`.
Use integer formats (`%d`, `%lu`, `%ld`). Floating point is not supported.

## Pin interrupts (`gpiote/nrf52_gpiote.h`)

Eight channels (0 to 7), each able to watch one pin.

| Function | Description |
|---|---|
| `nrf52_status_t nrf52_gpiote_enable_irq(uint8_t ch, uint16_t pin, nrf52_pull_t pull, nrf52_edge_t edge, nrf52_gpio_irq_cb_t cb, void *ctx)` | Configures the pin as input and enables the interrupt. |
| `void nrf52_gpiote_disable_irq(uint8_t ch)` | Disables a channel. |

`nrf52_edge_t`: `NRF52_EDGE_RISING`, `NRF52_EDGE_FALLING`, `NRF52_EDGE_TOGGLE`.

The callback `void cb(uint16_t pin, void *ctx)` runs in interrupt context. Set flags and
return. Do not perform I2C or logging with long waits in it.

## Periodic timer (`rtc/nrf52_rtc.h`)

RTC1 clocked by the 32.768 kHz crystal. Resolution is about 30 µs, maximum period about 500 s.

| Function | Description |
|---|---|
| `nrf52_status_t nrf52_periodic_start(uint32_t period_ms, nrf52_periodic_cb_t cb, void *ctx)` | Starts a periodic callback. The callback runs in interrupt context. |
| `void nrf52_periodic_stop(void)` | Stops the timer. |

RTC1 is used by the sensor scheduler. Do not use it elsewhere.

## Sensor framework (`sensor/nrf52_sensor.h`)

Sampling is driven by a periodic tick, a data-ready interrupt, or an explicit request.
Reads always run in `nrf52_sensor_poll()`, called from the main loop.

### Types

```c
typedef struct {
    uint32_t timestamp_ms;
    uint8_t  channels;                        /* number of valid values */
    int32_t  value[NRF52_SENSOR_MAX_CHANNELS]; /* up to 3 */
} nrf52_sensor_sample_t;

struct nrf52_sensor {
    const char *name;
    void *dev;                    /* driver data, passed to read() */
    nrf52_sensor_read_fn read;    /* required */
    nrf52_sensor_cb_t on_sample;  /* optional */
    void *cb_ctx;
    uint32_t period_ms;           /* 0 = no periodic sampling */
};
```

`read` has the signature `nrf52_status_t read(void *dev, nrf52_sensor_sample_t *out)`.
`on_sample` has the signature `void on_sample(const nrf52_sensor_t *s, const nrf52_sensor_sample_t *sample, void *ctx)`.

### Functions

| Function | Description |
|---|---|
| `nrf52_status_t nrf52_sensor_init(uint32_t tick_ms)` | Starts the scheduler. `tick_ms` is the base resolution for `period_ms`. |
| `nrf52_status_t nrf52_sensor_register(nrf52_sensor_t *s)` | Adds a sensor. Up to 4. The struct must stay valid while the program runs. |
| `nrf52_status_t nrf52_sensor_attach_irq(nrf52_sensor_t *s, uint8_t ch, uint16_t pin, nrf52_pull_t pull, nrf52_edge_t edge)` | Triggers a sample on a data-ready pin. |
| `void nrf52_sensor_request(nrf52_sensor_t *s)` | Requests one sample on the next poll. Safe from interrupts. |
| `void nrf52_sensor_poll(void)` | Runs pending reads and callbacks. Call from the main loop. |

A failed read is logged as a warning and skipped. The next sample is taken on schedule.

## Generic I2C register sensor (`drivers/nrf52_i2c_reg_sensor.h`)

A driver for sensors whose value is held in one register. It reads 1 or 2 bytes, big-endian,
and reports them as channel 0.

```c
typedef struct { uint8_t addr; uint8_t reg; uint8_t length; } nrf52_i2c_reg_cfg_t;
typedef struct { nrf52_i2c_reg_cfg_t cfg; } nrf52_i2c_reg_dev_t;

nrf52_status_t nrf52_i2c_reg_sensor_init(nrf52_i2c_reg_dev_t *dev,
                                         const nrf52_i2c_reg_cfg_t *cfg,
                                         nrf52_sensor_t *sensor,
                                         const char *name,
                                         uint32_t period_ms);
```

`init` fills in `sensor` but does not register it. Set `sensor.on_sample`, then call
`nrf52_sensor_register()`. `dev` and `sensor` must stay valid for the program lifetime.

For a chip with conversion or configuration steps, write a new driver that follows the same pattern.

## Example

See `apps/sensor_app/main.c`. It uses only the public headers above.

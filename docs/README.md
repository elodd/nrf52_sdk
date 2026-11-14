# nRF52 SDK

A bare-metal C SDK for the nRF52840. It provides the building blocks for
sensor applications: system timing, GPIO, I2C, UART logging, pin interrupts,
periodic timers, and a sensor scheduler. It uses the chip's registers
directly, so it has no dependency on nrfx or the Nordic SDK.

## Architecture

The application talks only to the SDK's public API. It never touches
registers.

```
Application           apps/sensor_app/main.c
        │  uses only public APIs
Public API            include/*.h
        │
Drivers               drivers/        (sensor-specific code)
        │
HAL / modules         src/            (register-level code)
        │
nRF52840 hardware
```

| Module | Header | Purpose |
|---|---|---|
| Core | `nrf52_sdk.h` | Init, millisecond tick, delays, GPIO, NVIC |
| TWIM | `nrf52_twim.h` | I2C master (100 kHz / 400 kHz) |
| Log | `nrf52_log.h` | UART logging with levels |
| GPIOTE | `nrf52_gpiote.h` | Pin-change interrupts |
| RTC | `nrf52_rtc.h` | Periodic timer (32 kHz clock) |
| Sensor | `nrf52_sensor.h` | Sensor scheduler and data-ready triggers |
| Driver | `nrf52_i2c_reg_sensor.h` | Generic I2C register sensor |

## Folder layout

```
nrf52_sdk/
├── include/        public headers (the only files applications include), one subfolder per module
├── src/            SDK implementation (HAL and modules)
├── drivers/        sensor drivers
├── platform/       startup code and linker script (nRF52840: 1 MB flash, 256 KB RAM)
├── apps/           firmware applications
├── tests/          host unit tests (make test)
├── sim/            host simulation of the hardware (make sim)
├── docs/           documentation (this folder)
├── config/         prompt template for AI coding sessions
├── .github/        CI workflow (host tests, firmware build)
├── Makefile
└── .gitignore
```

## Requirements

- `arm-none-eabi-gcc` (GNU Arm Embedded Toolchain 10 or newer)
- `make`
- `nrfjprog` (Nordic command-line tool), only for `make flash`
- An nRF52840 board or DK, with a debugger connected

## Build

```sh
make                # builds build/sensor_app.hex, linked against build/libnrf52sdk.a
make lib            # builds only the SDK static library
make size           # shows flash and RAM usage
make flash          # programs the board with nrfjprog
make clean
```

To build a different application, set `APP`:

```sh
make APP=apps/my_app
```

Every `.c` file in `src/`, `drivers/`, `platform/`, and the chosen `APP`
directory is compiled.

## Quick start

```c
#include "core/nrf52_sdk.h"
#include "log/nrf52_log.h"

int main(void)
{
    nrf52_init();
    nrf52_log_init(NRF52_PIN(0, 6));
    nrf52_gpio_config_output(NRF52_PIN(0, 13), false);

    for (;;) {
        nrf52_gpio_toggle(NRF52_PIN(0, 13));
        NRF52_LOGI("main", "tick at %lu ms", (unsigned long)nrf52_millis());
        nrf52_delay_ms(500);
    }
}
```

## Using a sensor

The sensor framework separates three concerns:

1. **Driver** reads the hardware and returns a sample.
2. **Scheduler** decides when to read: a fixed period, a data-ready pin, or an explicit request.
3. **Application** receives the sample in a callback.

```c
static nrf52_i2c_reg_dev_t dev;
static nrf52_sensor_t sensor;
static const nrf52_i2c_reg_cfg_t cfg = { .addr = 0x44, .reg = 0x00, .length = 2 };

static void on_sample(const nrf52_sensor_t *s, const nrf52_sensor_sample_t *smp, void *ctx)
{
    NRF52_LOGI(s->name, "value=%ld", (long)smp->value[0]);
}

void setup(void)
{
    nrf52_twim_init(NRF52_PIN(0, 27), NRF52_PIN(0, 26), 400000UL);
    nrf52_i2c_reg_sensor_init(&dev, &cfg, &sensor, "temp", 1000);
    sensor.on_sample = on_sample;

    nrf52_sensor_init(10);
    nrf52_sensor_register(&sensor);
}

void loop(void)
{
    nrf52_sensor_poll();                /* call from the main loop */
}
```

## Execution model

- **Interrupts** (SysTick, RTC1, GPIOTE) only set flags or do short work.
- **Main loop** (`nrf52_sensor_poll()`) performs all I2C reads and calls the
  application callbacks. Blocking I/O is only allowed in the main loop.
- Logging is blocking and short. It is safe to call from the main loop.

## Adding a sensor driver

1. Add `include/drivers/nrf52_<chip>.h` with an `init` function that fills an `nrf52_sensor_t`:
   set `name`, `dev`, `read`, and `period_ms`.
2. Add `drivers/nrf52_<chip>.c` with the `read` function. It uses `nrf52_twim_*`
   and returns an `nrf52_sensor_sample_t`.
3. Use the driver from the application. The application does not need to know
   about I2C.

See `drivers/nrf52_i2c_reg_sensor.c` for a complete example.

## Pin and resource notes

- RTC1 is used by the sensor scheduler. Do not use it from another module, or from a SoftDevice.
- TWIM0, UARTE0, and GPIOTE channels 0–7 are used by the SDK. The sensor app uses
  GPIOTE channel 0 only if you enable the data-ready interrupt.
- SysTick provides `nrf52_millis()`. It must not be reconfigured.
- Pin numbers use `NRF52_PIN(port, pin)`. P1 is available on the nRF52840 only.

## Limitations

- Not thread-safe. Call the SDK from the main loop and from interrupts only as documented.
- The TWIM and UART drivers block with a timeout. A bus or sensor fault returns an error instead of hanging.
- No low-power management is implemented yet. The main loop uses `wfi` between polls.
- Only one I2C bus (TWIM0) and one UART (UARTE0) are supported.
- The build has not been run on hardware yet. Verify the register offsets and TWIM SHORTS bit positions against the nRF52840 datasheet before production use.

See [API.md](API.md) for the full reference, [STRUCTURE.md](STRUCTURE.md) for the layering rules, and [BUILD.md](BUILD.md) for step-by-step build, flash, log, and test instructions.

## Tests

```sh
make test
```

The host tests cover the sensor scheduler, the sensor registry, and the I2C register driver,
with the hardware replaced by test doubles. They use AddressSanitizer and UBSan. Register-level
modules are verified on hardware only.

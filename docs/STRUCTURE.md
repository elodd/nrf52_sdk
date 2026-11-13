# SDK structure

Bare-metal C SDK for the nRF52840. Strict layers: apps → drivers → public API → register code → boot.

```
nrf52_sdk/
├── include/          # public headers only (one folder per module)
│   ├── core/ twim/ log/ gpiote/ rtc/ sensor/ drivers/
├── src/              # register-level .c files (flat)
├── drivers/          # sensor drivers (public API only)
├── platform/         # startup_nrf52840.c + nrf52840.ld
├── apps/             # applications (default: sensor_app)
├── tests/            # host unit tests
├── sim/              # host simulator
└── docs/ Makefile
```

**Rules**  
- Apps include only `include/`.  
- Drivers use public API only.  
- Interrupts only set flags; main loop does I2C.

## Folder tree


```
nrf52_sdk/
├── include/                     Public API. The only headers applications include.
│   ├── core/
│   │   └── nrf52_sdk.h          Core: init, millis, delays, GPIO, NVIC helper, status codes
│   ├── twim/
│   │   └── nrf52_twim.h         I2C master
│   ├── log/
│   │   └── nrf52_log.h          UART logging
│   ├── gpiote/
│   │   └── nrf52_gpiote.h       Pin-change interrupts
│   ├── rtc/
│   │   └── nrf52_rtc.h          Periodic timer
│   ├── sensor/
│   │   └── nrf52_sensor.h       Sensor scheduler and registry
│   └── drivers/
│       └── nrf52_i2c_reg_sensor.h   Driver: I2C sensor with a value register
├── src/                         Implementation of the modules (register-level code)
│   ├── nrf52_sdk.c
│   ├── nrf52_twim.c
│   ├── nrf52_log.c              Logger: level filter and line format (hardware independent)
│   ├── nrf52_log_uarte.c        Log transport: UARTE0 registers
│   ├── nrf52_log_port.h         Private transport interface (firmware, tests, sim)
│   ├── nrf52_gpiote.c
│   ├── nrf52_rtc.c
│   └── nrf52_sensor.c
├── drivers/                     Sensor drivers (use the public API only)
│   └── nrf52_i2c_reg_sensor.c
├── platform/                    Boot and memory layout for nRF52840
│   ├── startup_nrf52840.c       Vector table, reset handler, data/bss init
│   └── nrf52840.ld              Memory map: 1 MB flash, 256 KB RAM
├── apps/
│   └── sensor_app/main.c        Sensor application (default build)
├── tests/                       Host unit tests (no hardware)
│   ├── test_harness.h           Shared CHECK macro, test-case type, helpers
│   ├── test_support.h           Test-double control API
│   ├── mocks.c                  Replacements for hardware functions
│   ├── test_main.c              Runner: clean state before each test, summary
│   ├── test_driver.c            Suite: I2C register driver
│   ├── test_sensor.c            Suite: scheduler, registry, data-ready triggers
│   ├── test_log.c               Suite: log format, levels, truncation
│   └── Makefile                 make test (add SANITIZE=1 for ASan/UBSan)
├── sim/
│   ├── sim_hal.c                Host simulation of the hardware layer (make sim)
│   └── README.md                How the simulator runs
├── docs/
│   ├── README.md                Overview, build, quick start, limitations
│   ├── API.md                   API reference
│   └── STRUCTURE.md             This file
├── Makefile                     Firmware build (ARM); make test runs tests/, make sim runs sim/
├── README.md                    Entry point
└── .gitignore
```

## Layers and dependency rules

```
apps/            application code
    │
drivers/         sensor-specific code
    │
include/         public API (the contract)
    │
src/             module implementation (registers)
    │
platform/        boot and memory layout
```

Rules:

1. An application includes only headers from `include/`. It never touches registers.
2. A driver uses `include/` only. It does not call `src/` modules directly.
3. `src/` modules do not depend on drivers or applications.
4. Each module's header declares its whole public surface. Anything not in
   `include/` is private to its `.c` file.

## Static library

`make` archives `src/` and `drivers/` into `build/libnrf52sdk.a` and links each app against
it. `make lib` builds only the library. `build/libnrf52sdk.a` plus `include/` is the
complete SDK for another project; that project also needs `platform/startup_nrf52840.c`
and `platform/nrf52840.ld`, which stay out of the library:

```
apps/<app>/*.o  ─┐
startup .o  ─────┼─▶ link: -Lbuild -lnrf52sdk ─▶ <app>.elf ─▶ <app>.hex
libnrf52sdk.a  ──┘   (only the modules the app uses)
```

A new `.c` file in `src/` or `drivers/` goes into the library automatically.

## Module map

| Module | Public header | Implementation | Hardware | Used by |
|---|---|---|---|---|
| Core | `include/core/nrf52_sdk.h` | `src/nrf52_sdk.c` | SysTick, HFCLK, GPIO, NVIC | everything |
| TWIM | `include/twim/nrf52_twim.h` | `src/nrf52_twim.c` | TWIM0 | drivers |
| Log | `include/log/nrf52_log.h` | `src/nrf52_log.c`, transport `src/nrf52_log_uarte.c` | UARTE0 | apps, sensor |
| GPIOTE | `include/gpiote/nrf52_gpiote.h` | `src/nrf52_gpiote.c` | GPIOTE | sensor (data-ready) |
| RTC | `include/rtc/nrf52_rtc.h` | `src/nrf52_rtc.c` | RTC1, LFCLK | sensor (scheduler) |
| Sensor | `include/sensor/nrf52_sensor.h` | `src/nrf52_sensor.c` | none directly | apps, drivers |
| I2C register driver | `include/drivers/nrf52_i2c_reg_sensor.h` | `drivers/nrf52_i2c_reg_sensor.c` | via TWIM | apps |

## Interrupt ownership

| Vector | Owner | Work done in the interrupt |
|---|---|---|
| SysTick | core | increments the millisecond counter |
| GPIOTE | gpiote | reads the event, calls the registered callback |
| RTC1 | rtc | reschedules the compare, calls the callback |

Sensor reads never run in an interrupt. Interrupts set a pending flag, and
`nrf52_sensor_poll()` in the main loop performs the I2C transfer.

## Naming conventions

- Public functions: `nrf52_<module>_<action>()`, for example `nrf52_twim_write()`.
- Public types: `nrf52_<name>_t`. Enums use upper-case values with the type prefix.
- Module-private helpers: `static`, with no prefix.
- Register addresses: `<PERIPH>_<REGISTER>` in the `.c` file, with the datasheet offset.
- Status codes: every fallible call returns `nrf52_status_t`.

## Adding a module

1. Add `include/<module>/nrf52_<module>.h` with the public API and a short comment for each function.
2. Add `src/nrf52_<module>.c` with the registers defined locally.
3. If the module uses an interrupt, add its handler (for example `FOO_IRQHandler`) to
   `platform/startup_nrf52840.c` and register it with `nrf52_irq_enable()`.
4. Add a host test suite as `tests/test_<module>.c`, register its cases in `test_main.c`, and add
   a double in `mocks.c` for each hardware call the module makes.
5. Document the new functions in `docs/API.md` and add the module to the table above.

## Adding a driver

1. Add `include/drivers/nrf52_<chip>.h` with a config struct and an `init` function. `init` fills an
   `nrf52_sensor_t` and does not register it.
2. Add `drivers/nrf52_<chip>.c`. The `read` function uses only `include/` APIs.
3. Test the conversion logic on the host by mocking the bus calls, as in
   `tests/test_driver.c`.

## Tests

`make test` builds `tests/` with the host compiler and runs them. The tests link the real
sensor scheduler, logger and I2C driver against doubles in `tests/mocks.c` for the timer,
GPIO interrupt, I2C bus and log transport.

| Suite | Code under test | Doubles used |
|---|---|---|
| `test_driver.c` | `drivers/nrf52_i2c_reg_sensor.c` | I2C bus, log transport |
| `test_sensor.c` | `src/nrf52_sensor.c` | Clock, periodic timer, GPIO interrupt, log transport |
| `test_log.c` | `src/nrf52_log.c` | Clock, log transport (captures each line) |

The logger's transport is the private interface in `src/nrf52_log_port.h`. On the board it
is `src/nrf52_log_uarte.c`; in the tests, `mocks.c` stores each line so a test can compare
it with `mock_log_line()`. `mock_reset()` initializes the logger at level INFO before every
test.

Register-level code (`src/nrf52_twim.c`, `src/nrf52_gpiote.c`, `src/nrf52_rtc.c`,
`src/nrf52_log_uarte.c`, `src/nrf52_sdk.c`) is not covered by host tests and must be
checked on hardware.

## Simulator

`make sim` links the app, the sensor scheduler, the logger and the drivers with `sim/sim_hal.c`, which
replaces every register-level module in `src/` with a host version. A new module that
touches hardware needs a matching host version in `sim/sim_hal.c`, or `make sim` fails to
link. See [../sim/README.md](../sim/README.md).

Applications must not use inline assembly or registers, so they build for both targets.
Use `nrf52_wait_for_interrupt()` instead of `wfi`.

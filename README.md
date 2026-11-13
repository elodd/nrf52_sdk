# nRF52 SDK

[![CI](https://github.com/elodd/nrf52_sdk/actions/workflows/ci.yml/badge.svg)](https://github.com/elodd/nrf52_sdk/actions/workflows/ci.yml)

Bare-metal C SDK for the nRF52840: I2C, UART logging, pin interrupts, a periodic timer, and a sensor scheduler.
It is the base library for the applications in `apps/`: it builds as the static library
`build/libnrf52sdk.a`, and each app links against it. It uses no vendor SDK, only the
C library from the ARM toolchain (newlib-nano).

- Documentation: [docs/README.md](docs/README.md)
- API reference: [docs/API.md](docs/API.md)
- Structure and layering rules: [docs/STRUCTURE.md](docs/STRUCTURE.md)
- Full build, flash, log, and test guide: [docs/BUILD.md](docs/BUILD.md)
- Sensor application: [apps/sensor_app](apps/sensor_app)

## Folders

Listed from the top layer down. Each layer depends only on the layers below it.

| Folder | Contents |
|---|---|
| [`apps/`](apps) | Applications. `sensor_app/main.c` reads an I2C sensor once per second, logs the value over UART and toggles an LED, using only SDK and driver APIs. `make` builds it by default. |
| [`drivers/`](drivers) | Sensor-specific code. `nrf52_i2c_reg_sensor.c` supports an I2C sensor that exposes its value in a register. Drivers use only the public API in `include/`. |
| [`include/`](include) | The public API, one subfolder per module. The only headers applications include: core (init, millis, delays, GPIO, status codes), I2C (TWIM), UART logging, pin-change interrupts (GPIOTE), periodic timer (RTC), the sensor scheduler, and the driver header. |
| [`src/`](src) | Register-level implementation of the modules declared in `include/`. With `drivers/`, archived into `build/libnrf52sdk.a`. |
| [`platform/`](platform) | Startup and linker script for nRF52840: `startup_nrf52840.c` (vector table, reset handler, data/bss init) and `nrf52840.ld` (1 MB flash at `0x00000000`, 256 KB RAM at `0x20000000`). |
| [`tests/`](tests) | Host unit tests that run without hardware. They cover the sensor scheduler, the logger and the I2C driver, with hardware calls replaced by doubles in `mocks.c`. Register-level code in `src/` is not covered and must be checked on a board. |
| [`sim/`](sim) | Host simulation of the hardware layer. `make sim` runs the app on your computer and prints its log to the terminal. See [sim/README.md](sim/README.md). |
| [`docs/`](docs) | Overview, API reference, build guide, and structure and layering rules. |
| `build/` | Generated output: object files, the SDK library `libnrf52sdk.a`, `sensor_app.elf`, `.hex`, `.map`, and the simulator binary in `build/sim/`. `make clean` removes it, and git ignores it. |

See [docs/STRUCTURE.md](docs/STRUCTURE.md) for the module map, interrupt ownership, naming conventions, and how to add a module or driver.

## Requirements

- `arm-none-eabi-gcc` for the firmware build
- `make`
- `nrfjprog` (Nordic nRF Command Line Tools), only for flashing
- A native `cc` (gcc or clang) for the host tests

## Build

```sh
make                          # builds build/sensor_app.hex
make lib                      # builds only the SDK library, build/libnrf52sdk.a
make size                     # memory usage
make APP=apps/my_app          # build a different application
make clean
```

### How the app is linked

```
src/*.c, drivers/*.c ─▶ build/obj/*.o ─▶ build/libnrf52sdk.a ──┐
platform/startup_nrf52840.c ─▶ startup .o ─────────────────────┼─▶ sensor_app.elf ─▶ .hex
apps/sensor_app/*.c ─▶ main.o ─────────────────────────────────┘
```

- The linker takes only the library modules the app uses, so an unused module adds no code.
- The startup code is linked directly, not from the library. Nothing references the
  vector table, so the linker would leave it out of an archive.
- To use the SDK in another project, take `build/libnrf52sdk.a`, `include/` and `platform/`,
  and link with `-L<dir> -lnrf52sdk` after your own object files.

See [docs/BUILD.md](docs/BUILD.md#2-build-the-firmware) for details.

## Run

Connect the nRF52840 DK over USB, then:

```sh
make flash                    # programs build/sensor_app.hex with nrfjprog
```

Read the log at 115200 8N1. On the nRF52840 DK, use the virtual COM port. On other boards,
connect a USB-to-serial adapter to pin P0.06 (`APP_LOG_TX_PIN`). Expected output:

```
[0] I app: sensor app started
[1003] I temp: t=1003 ms value=...
```

The LED on P0.13 toggles with each sample.

## Test

The tests run on your computer. They do not need the ARM toolchain.

```sh
make test                     # 110 checks across driver, sensor and log suites
make test SANITIZE=1          # also runs AddressSanitizer and UBSan
```

A passing run ends with `110 checks, 0 failed` and exits with 0.

## Simulate

Run the app on your computer with simulated hardware. The log prints to the terminal
in the same format as the board's UART output. No board or ARM toolchain needed.

```sh
make sim                      # 10 simulated seconds
make sim SIM_REALTIME=1       # paced to wall time, like the board
make sim SIM_SECONDS=60       # longer run
make -s sim > sensor.log      # save the log
```

Expected output:

```
[0] I app: sensor app started
[1000] I temp: t=1000 ms value=2155
...
[10000] I temp: t=10000 ms value=2200
sim: stopped at 10000 ms
sim: P0.13 toggled 10 times
```

The two `sim:` lines are a summary on stderr, so they are not written to a saved log.

Time is simulated, so 10 seconds finish almost instantly unless `SIM_REALTIME=1` is set.
The app, the sensor scheduler and the drivers are the real code; `sim/sim_hal.c` replaces
the register-level modules. Register-level code is not covered, so check it on a board.

See [sim/README.md](sim/README.md) for how the simulator builds and executes the app,
the fake I2C device, limitations, and how to add hardware to it.

## Troubleshooting

See the table in [docs/BUILD.md](docs/BUILD.md#6-troubleshooting), covering missing tools, missing log output,
and sensor I2C errors.

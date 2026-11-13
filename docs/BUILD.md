# Build, run, and test

This guide covers building the firmware, flashing it to an nRF52840, reading the
log output, and running the host unit tests.

## 1. Install the tools

| Tool | Used for | Install |
|---|---|---|
| `arm-none-eabi-gcc`, `arm-none-eabi-size`, `arm-none-eabi-objcopy` | firmware build | Ubuntu/Debian: `sudo apt install gcc-arm-none-eabi`. macOS: `brew install --cask gcc-arm-embedded`. Windows: GNU Arm Embedded Toolchain installer. |
| `make` | build | Preinstalled on most systems. |
| `cc` (gcc or clang) | host unit tests | Preinstalled on most systems. |
| `nrfjprog` | flashing (`make flash`) | Nordic nRF Command Line Tools. |
| Serial terminal | reading logs | `screen`, `minicom`, PuTTY, or any terminal at 115200 8N1. |

Check the installation:

```sh
arm-none-eabi-gcc --version
make --version
```

## 2. Build the firmware

From the `nrf52_sdk/` folder:

```sh
make
```

This builds in three stages:

1. Compiles the SDK (`src/`, `drivers/`) and archives it into the static library
   `build/libnrf52sdk.a`.
2. Compiles the startup code (`platform/startup_nrf52840.c`) and `apps/sensor_app`.
3. Links the app and startup objects against the library (`-Lbuild -lnrf52sdk`).

The output is:

- `build/libnrf52sdk.a`: the SDK as a static library
- `build/sensor_app.elf`: debug image with symbols
- `build/sensor_app.hex`: image to flash
- `build/sensor_app.map`: memory map

The linker takes only the library modules the app uses. For example, an app that never
calls the GPIOTE functions does not include `nrf52_gpiote.o`, and its interrupt slot uses
the default handler. The startup code is linked directly, not through the library,
because nothing references the vector table and the linker would leave it out.

To build only the library, for use in another project together with `include/`:

```sh
make lib
```

Show memory use:

```sh
make size
```

Build a different application (any folder with `.c` files):

```sh
make APP=apps/my_app
```

Start over:

```sh
make clean
```

## 3. Flash the board

1. Connect the nRF52840 DK (or your board with a debugger) over USB.
2. Flash the firmware:

```sh
make flash
```

This runs `nrfjprog --program build/sensor_app.hex --chiperase --verify --reset`.
You can also flash manually:

```sh
nrfjprog --program build/sensor_app.hex --chiperase --verify --reset
```

If `nrfjprog` reports that no device is found, check the USB cable and that the
debugger is powered. Run `nrfjprog --ids` to list the connected boards.

## 4. Read the log output

The sensor app writes its log over UARTE0 at 115200 8N1, on pin P0.06 (`APP_LOG_TX_PIN`
in `apps/sensor_app/main.c`).

- **nRF52840 DK**: the virtual COM port (VCOM) on the USB connector carries this pin.
  Open the new serial port that appears, for example `/dev/ttyACM0` on Linux or
  `/dev/cu.usbmodem*` on macOS, at 115200 8N1.
- **Other boards**: connect a USB-to-serial adapter's RX to P0.06 and a common ground.

Expected output after reset:

```
[0] I app: sensor app started
[1003] I temp: t=1003 ms value=...
[2003] I temp: t=2003 ms value=...
```

The LED on P0.13 toggles with each sample. The `value` depends on your sensor. The sensor
app uses address `0x44` and register `0x00` as placeholders, so you may see a
read-failure warning until you set the correct sensor values:

```
[...] W sensor: temp read failed (status 3)
```

Status 3 is `NRF52_ERR_IO`, which means the sensor did not acknowledge the I2C address.

Change the pins or sensor address in `apps/sensor_app/main.c`, then rebuild and flash.

### Without a board: the simulator

`make sim` builds the app for your computer with simulated hardware and prints the log
to the terminal, in the same format as the UART output. It needs only `cc`, not the
ARM toolchain or a board.

```sh
make sim                              # 10 simulated seconds, runs instantly
make sim SIM_SECONDS=60               # longer run
make sim SIM_REALTIME=1               # one log line per real second, like the board
make -s sim > sensor.log              # save the log to a file
```

See [../sim/README.md](../sim/README.md) for how it executes, what is simulated, and its
limitations.

## 5. Run the host unit tests

The tests run on your computer, not on the board. They do not need the ARM toolchain.

```sh
make test
```

Expected output:

```
[driver]
  rejects_bad_config                 ok
  ...
[sensor]
  register_rejects_invalid           ok
  ...
[log]
  init_uses_tx_pin                   ok
  ...
110 checks, 0 failed
```

The exit code is 0 when all checks pass, and 1 otherwise.

To also check for memory errors and undefined behavior (uses AddressSanitizer and UBSan):

```sh
make test SANITIZE=1
```

To remove the test output:

```sh
cd tests && make clean
```

To run one suite's tests while developing, add a new file to `tests/` and list it in
`SRCS` in `tests/Makefile`, as described in [STRUCTURE.md](STRUCTURE.md).

## 6. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `arm-none-eabi-gcc: command not found` | toolchain not installed or not on `PATH` | Install it (step 1) and check `which arm-none-eabi-gcc`. |
| Link warnings about `_read`, `_write`, `_close`, `_lseek` | newlib stubs for file I/O, linked by `nano.specs` | Harmless. The SDK does not use file I/O. |
| `nrfjprog: command not found` | Nordic tools not installed | Install the nRF Command Line Tools, or flash with another tool. |
| No log output | wrong serial port or baud rate, or log pin not connected | Use 115200 8N1 and check the port. Confirm the board's log pin is P0.06. |
| `read failed (status 2)` | sensor not responding at that address | Check wiring, pull-ups, and the address in `main.c`. |
| `make test` prints "Permission denied" | the binary was built on a filesystem that does not keep the execute bit | Build on a local disk, or run `./build/test_sdk` from a copy in `/tmp`. |

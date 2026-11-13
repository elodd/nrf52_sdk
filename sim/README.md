# Simulator

`make sim` runs an application from `apps/` on your computer with simulated hardware.
It prints the log to the terminal in the same format as the board's UART output.
It needs only the host compiler (`cc`): no board and no ARM toolchain.

## Run

From the `nrf52_sdk/` folder:

```sh
make sim                          # 10 simulated seconds, finishes almost instantly
make sim SIM_SECONDS=60           # longer run
make sim SIM_REALTIME=1           # paced to wall time: one sample per real second
make sim APP=apps/my_app          # simulate a different application
make -s sim > sensor.log          # save the log (-s hides make's own command lines)
```

Or run the built binary directly:

```sh
SIM_SECONDS=5 ./build/sim/sensor_app
```

| Variable | Default | Effect |
|---|---|---|
| `SIM_SECONDS` | `10` | Simulated run time. The program exits with 0 when it is reached. |
| `SIM_REALTIME` | `0` | `1` sleeps 1 ms of real time per simulated millisecond. |

Expected output:

```
[0] I app: sensor app started
[1000] I temp: t=1000 ms value=2155
[2000] I temp: t=2000 ms value=2160
...
[10000] I temp: t=10000 ms value=2200
sim: stopped at 10000 ms
sim: P0.13 toggled 10 times
```

Log lines go to stdout. The two `sim:` summary lines go to stderr, so a redirected log
file contains only the log.

## Execution

### 1. Build

The `sim` target in the top-level `Makefile` compiles with the host compiler:

```
cc -DNRF52_HAS_P1=1 -Iinclude -std=gnu11 -O1 -g -Wall -Wextra -Werror \
   -Isrc sim/sim_hal.c src/nrf52_sensor.c src/nrf52_log.c drivers/*.c apps/sensor_app/*.c \
   -o build/sim/sensor_app
```

| Linked | Left out |
|---|---|
| `apps/<app>/*.c` (the application) | `src/nrf52_sdk.c`, `src/nrf52_twim.c`, `src/nrf52_log_uarte.c`, `src/nrf52_gpiote.c`, `src/nrf52_rtc.c` (registers) |
| `src/nrf52_sensor.c` (sensor scheduler), `src/nrf52_log.c` (logger) | `platform/startup_nrf52840.c` (vector table, reset handler) |
| `drivers/*.c` (sensor drivers) | `platform/nrf52840.ld` (memory map) |
| `sim/sim_hal.c` (simulated hardware) | |

`sim/sim_hal.c` defines every function the left-out modules declare in `include/`, plus
the log transport from `src/nrf52_log_port.h`, so
the application links against it without changes. `make` rebuilds the binary only when
one of these sources or a header in `include/` changes.

### 2. Start

On the board, the CPU starts at `Reset_Handler`, which sets up RAM and calls `main()`.
In the simulator, the operating system calls the application's `main()` directly.

The application initializes as usual. Each call goes to the simulated version:

| Call | On the board | In the simulator |
|---|---|---|
| `nrf52_init()` | Starts the crystal and the 1 kHz SysTick | Resets the simulated clock to 0 and reads `SIM_SECONDS`/`SIM_REALTIME` |
| `nrf52_log_init(pin)` | Real logger; transport configures UARTE0 | Real logger; transport writes to stdout, without the `\r` |
| `nrf52_gpio_config_output(pin, ...)` | Sets the pin registers | Records the pin level |
| `nrf52_twim_init(...)` | Configures TWIM0 | Checks the arguments |
| `nrf52_sensor_init(10)` | Real scheduler; starts RTC1 every 10 ms | Real scheduler; stores the timer callback, due every 10 ms |

### 3. Main loop

```c
for (;;) {
    nrf52_sensor_poll();          /* real scheduler code */
    nrf52_wait_for_interrupt();   /* board: wfi.  sim: advance the clock */
}
```

On the board, `nrf52_wait_for_interrupt()` executes `wfi` and the CPU sleeps until an
interrupt arrives. In the simulator, each call:

1. Exits the program if the clock has reached `SIM_SECONDS`.
2. With `SIM_REALTIME=1`, sleeps 1 ms of real time.
3. Advances the simulated clock by 1 ms. `nrf52_millis()` returns this value.
4. Calls the periodic timer callback if it is due, as the RTC1 interrupt would.

`nrf52_delay_ms()` advances the clock the same way, one millisecond at a time.

### 4. Sampling

Every 10 ms the timer callback runs `scheduler_tick()`. When a sensor's period
(1000 ms in `sensor_app`) has elapsed, the scheduler marks it pending. The next
`nrf52_sensor_poll()` then runs the real driver code:

```
nrf52_sensor_poll()                     src/nrf52_sensor.c
  └─ read_register()                    drivers/nrf52_i2c_reg_sensor.c
       └─ nrf52_twim_write_read(0x44)   sim/sim_hal.c: fake I2C device
  └─ on_temp_sample()                   apps/sensor_app/main.c
       ├─ nrf52_gpio_toggle(LED)        sim: counts the toggle
       └─ NRF52_LOGI(...)               real logger; sim transport prints to stdout
```

The fake I2C bus has one device:

| Address | Register | Returns |
|---|---|---|
| `0x44` | `0x00` | Big-endian value: 2150 + 5 per elapsed second, wrapping every 20 s |
| `0x44` | other | 0 |
| any other | — | `NRF52_ERR_IO` (address not acknowledged) |

A wrong address in the application therefore shows the same warning as on hardware:

```
[1000] W sensor: temp read failed (status 3)
```

### 5. Stop

When the clock reaches `SIM_SECONDS`, the next `nrf52_wait_for_interrupt()` calls
`exit(0)`. An `atexit` handler prints the stop time and how often each GPIO pin
toggled, to stderr.

## Limitations

- Register-level code in `src/` is not run. Check it on a board.
- GPIO data-ready interrupts (`nrf52_gpiote_enable_irq`) are accepted but never fire.
- Interrupts run synchronously inside `nrf52_wait_for_interrupt()`, never in the middle of
  other code, so the simulator does not find race conditions.
- Timing is exact to 1 ms. The board's RTC has about 30 µs resolution, and I2C and UART
  transfers take real time.

## Adding hardware to the simulator

When you add a module in `src/` that touches registers, add a host version of each of its
public functions to `sim/sim_hal.c`. Otherwise `make sim` fails to link with an
undefined symbol. To simulate a new I2C sensor, add its address and registers to
`nrf52_twim_write_read()` in `sim/sim_hal.c`.

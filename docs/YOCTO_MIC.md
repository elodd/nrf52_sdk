# Microphone data → Yocto

## nRF52 side (this SDK)

A new module `pdm/` and application `apps/mic_app` capture samples from a PDM digital microphone and log amplitude statistics (plus a few raw samples) over UART.

Build and flash:

```sh
make APP=apps/mic_app
make APP=apps/mic_app flash
```

Connect the UART TX pin (default P0.06) to a serial port on the Yocto Linux system.

## Yocto side

The layer `yoctolayer/` (in this SDK repo) provides a userspace receiver:

- `yoctolayer/conf/layer.conf` (collection `mic-stream`, depends on `core`, series `scarthgap`)
- `yoctolayer/recipes-apps/mic-receiver/` (recipe and `files/mic-receiver.c`)

To use it in a Yocto build:

1. Add the layer to `bblayers.conf`:
   ```
   BBLAYERS += "/path/to/nrf52_sdk/yoctolayer"
   ```
2. Add the package to your image:
   ```
   IMAGE_INSTALL:append = " mic-receiver"
   ```
3. On the target:
   ```
   mic-receiver /dev/ttyUSB0
   ```

The receiver filters and prints lines containing `mic:` so you can see the live amplitude data arriving from the nRF52840.

## Notes

- PDM pins (CLK P0.25, DIN P0.24) and the 0 dB gain are defaults; adjust them in `apps/mic_app/main.c` and `src/nrf52_pdm.c` for your hardware.
- The simulator (`make sim APP=apps/mic_app`) uses a synthetic triangle wave, not real audio.
- The current implementation polls the END event, so samples between two reads are lost. For production use an interrupt on EVENTS_END and double-buffering.
- Raw binary PCM streaming can be added by writing the buffer directly to the UARTE TX EasyDMA (the existing log module is text-oriented).
- Register addresses follow the nRF52840 Product Specification; verify against the latest PS if you change clock or mode settings.

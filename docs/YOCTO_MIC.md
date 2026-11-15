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

A minimal layer `meta-mic-stream/` provides a userspace receiver:

- `conf/layer.conf`
- `recipes-apps/mic-receiver/`

To use it in a Yocto build:

1. Add the layer to `bblayers.conf`:
   ```
   BBLAYERS += "/path/to/meta-mic-stream"
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

- PDM pin defaults and gain are placeholders; adjust in `apps/mic_app/main.c` and the driver for your hardware.
- The current implementation polls the END event. For production use an interrupt on EVENTS_END and double-buffering.
- Raw binary PCM streaming can be added by writing the buffer directly to the UARTE TX EasyDMA (the existing log module is text-oriented).
- Register addresses follow the nRF52840 Product Specification; verify against the latest PS if you change clock or mode settings.

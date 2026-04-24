# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Single-purpose firmware for a Freenove ESP32-S3-WROOM (FNK0084) dev board with an OV3660 camera: connects to Wi-Fi and serves an MJPEG stream at `http://<ip>/stream` (with a minimal HTML viewer at `/`). All logic lives in `src/main.cpp`.

Before flashing, `WIFI_SSID` / `WIFI_PASS` in `src/main.cpp` must be edited — they are hardcoded constants, not config.

## Build / flash / monitor

PlatformIO CLI is **not** on PATH on this machine — always call it by absolute path:

```
/home/sunil/.platformio/penv/bin/pio run                      # build
/home/sunil/.platformio/penv/bin/pio run -t upload            # build + flash
/home/sunil/.platformio/penv/bin/pio device monitor           # serial monitor (115200)
/home/sunil/.platformio/penv/bin/pio run -t clean
```

Target env is `esp32-s3` (only env defined in `platformio.ini`). Upload/monitor port is `/dev/ttyACM0`.

## Architecture notes worth knowing

- **PSRAM is required.** `platformio.ini` sets `board_build.arduino.memory_type = qio_opi` and `-DBOARD_HAS_PSRAM`; `initCamera()` allocates frame buffers with `CAMERA_FB_IN_PSRAM` and `fb_count = 2`. Changing the board variant or dropping PSRAM will break camera init.
- **Stream format is fixed at QXGA (2048×1536) JPEG.** The OV3660 caps at ~15 fps at that resolution; the comment in `main.cpp` captures this. If lowering resolution, also reconsider `jpeg_quality` and `fb_count`.
- **`streamHandler` is a blocking infinite loop per client** — it only exits when `httpd_resp_send_chunk` returns non-OK (client disconnect). The default `esp_http_server` config handles one client at a time; `cfg.stack_size = 8192` is bumped from the default for the streaming workload.
- **Camera sensor tuning in `initCamera()` is OV3660-specific.** `aec2` and `lenc` are OV3660-only knobs; they will be silently ignored on OV2640 but the pinout constants also assume OV3660/FNK0084 wiring — swapping sensors means revisiting both pin map and tuning.
- **USB-CDC serial is enabled** via `-DARDUINO_USB_CDC_ON_BOOT=1`, which is why `/dev/ttyACM0` (not `/dev/ttyUSB*`) is the right port.

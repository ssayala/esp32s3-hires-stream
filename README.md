# esp32s3-hires-stream

Minimal MJPEG streamer for an **ESP32-S3-WROOM** + **OV3660** camera. The board connects to Wi-Fi and serves a live stream over HTTP — point a browser at the board's IP and you get the feed full-width.

Built for the [Freenove ESP32-S3-WROOM (FNK0084)](https://store.freenove.com/products/fnk0084) kit, which ships with the OV3660 sensor. Other ESP32-S3 + OV3660 boards should work if you update the pin map in `src/main.cpp`.

## Hardware

- ESP32-S3-WROOM-1 (N8R8 — 8 MB flash, 8 MB OPI PSRAM). **PSRAM is required** for QXGA/UXGA frame buffers.
- OV3660 camera module (wiring per `PIN_*` constants in `src/main.cpp`).
- USB-C cable into the board's **native USB** port (the one that enumerates as `/dev/ttyACM*`), not the UART bridge.

## Build & flash

Uses [PlatformIO](https://platformio.org/). Install the CLI, then:

```sh
cp src/secrets.h.example src/secrets.h    # then edit with your SSID / password
pio run                                    # build
pio run -t upload                          # flash
pio device monitor                         # serial logs (115200)
```

`src/secrets.h` is gitignored so credentials stay out of the repo.

On boot the serial log prints the assigned IP:

```
IP: http://192.168.1.42/
```

Open that in a browser.

## Tuning

Two knobs at the top of `src/main.cpp` control the quality/bandwidth tradeoff:

| Constant | Default | Notes |
| --- | --- | --- |
| `STREAM_FRAMESIZE` | `FRAMESIZE_UXGA` (1600×1200) | `FRAMESIZE_QXGA` (2048×1536) is the OV3660 max but tends to saturate 2.4 GHz Wi-Fi and go choppy. `FRAMESIZE_SXGA` / `HD` are safer over weak links. |
| `STREAM_JPEG_QUALITY` | `12` | Lower number = better quality *and* larger frames. 10 is visibly similar to 12 but ~40% bigger. |

If the stream is choppy, drop resolution before anything else — the ESP32-S3 radio is the bottleneck, not the sensor.

## How it works

Single source file (`src/main.cpp`), roughly:

1. `initCamera()` configures the OV3660 at UXGA JPEG with 2 PSRAM frame buffers and `CAMERA_GRAB_LATEST` so stale frames are dropped rather than queued.
2. `connectWifi()` joins the AP in STA mode with Wi-Fi sleep disabled (latency over power).
3. `startServer()` registers two routes on the built-in `esp_http_server`:
   - `GET /` — one-page HTML viewer
   - `GET /stream` — `multipart/x-mixed-replace` MJPEG stream
4. `streamHandler()` loops: grab frame → send part header + JPEG → return buffer, exiting only when the client disconnects.

Some OV3660-specific sensor tuning (`aec2`, `lenc`, higher `gainceiling`) is applied in `initCamera()` for better low-light behavior; those knobs don't exist on OV2640 and will be silently ignored there.

## License

MIT — do what you want.

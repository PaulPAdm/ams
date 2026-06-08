# Firmware embedded-light ESP32-S3

`embeded-light-esps3` is a separate ESP-IDF port of the light firmware. It does not modify the existing Pico 2 W `embeded-light` project.

## Scope

This project keeps the light firmware behavior and removes the same hardware blocks as `embeded-light`: no INA219 energy monitor and no display.

Implemented:

- ESP-IDF `v6.0.1` project for `esp32s3`.
- NVS-backed device config for Wi-Fi, server host/IP, device ID, and audio calibration.
- Startup console menu, diagnostics mode, and audio calibration mode.
- Adafruit ICS-43434 I2S microphone capture.
- 48 kHz stereo I2S input, 3:1 FIR downsample to 16 kHz PCM16LE.
- Audio stream queue and DSP frame queue with dropped/depth counters.
- Adafruit XTSD 2 GB over SDSPI as a raw SD circular buffer.
- Full sound detector path: calibrated spectral bands when calibration is present, peak/noise fallback otherwise.
- Sound event metadata upload and streaming audio upload from SD snapshot.
- Time sync with the AMS backend and UTC event timestamps.
- Health report with INA219/power fields sent as `unknown`.

The spectral analyzer is self-contained for ESP32-S3 and uses the firmware's internal spectrum backend.

## Configure

First boot can be configured from the startup console. Press Enter during the startup window to open the menu.

Compile-time defaults live in [device_runtime_config.h](main/include/device_runtime_config.h):

```c
#define DEVICE_WIFI_SSID "your-ssid"
#define DEVICE_WIFI_PASSWORD "your-password"
#define DEVICE_SERVER_HOST "192.168.1.10"
#define DEVICE_SERVER_PORT 80u
#define DEVICE_ID "esp32s3-light-001"
```

Backend paths match the light firmware:

```text
GET  /api/devices/{device_id}/time/sync
GET  /api/devices/{device_id}/health/report
POST /api/devices/{device_id}/sound-events
POST /api/devices/{device_id}/sound-events/{event_id}/audio
```

Power telemetry is intentionally unavailable:

```text
ina219_online=unknown
bus_voltage_v=unknown
current_ma=unknown
power_mw=unknown
computed_power_mw=unknown
```

## Build

From this folder:

```powershell
idf.py build
```

On this local machine the verified build command was:

```powershell
$env:IDF_PATH='C:\esp\v6.0.1\esp-idf'
$env:ESP_ROM_ELF_DIR='C:\Espressif\tools\esp-rom-elfs\20241011'
$env:PATH='C:\Espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin;' + $env:PATH
cmake --build build
```

Flash and monitor:

```powershell
idf.py flash monitor
```

## Hardware

See [PINOUT.md](PINOUT.md).

Short version:

- ICS-43434: `BCLK=GPIO4`, `LRCLK=GPIO5`, `DOUT=GPIO6`, `SEL=GPIO7`.
- Adafruit XTSD 2 GB: `CS=GPIO10`, `MOSI=GPIO11`, `SCK=GPIO12`, `MISO=GPIO13`.
- XTSD power: `VIN -> 3V3`, `3V3 -> not connected`, `GND -> GND`.

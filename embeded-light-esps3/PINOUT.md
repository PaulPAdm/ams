# ESP32-S3 light pinout

Проект: `embeded-light-esps3`.

Цель: отдельная ESP32-S3 версия light firmware. Текущий `embeded-light` для Pico 2 W не меняется.

Пины ниже выбраны для ESP32-S3 devboard с большим количеством GPIO. Не используются USB pins `GPIO19/GPIO20`, strapping pins и типичные flash/PSRAM линии.

## Adafruit ICS-43434 I2S microphone

| Пин ICS-43434 | ESP32-S3 | Направление | Назначение |
| --- | --- | --- | --- |
| `3V` | `3V3` | питание | Питание микрофона 3.3 V |
| `GND` | `GND` | земля | Общая земля |
| `BCLK` | `GPIO4` | ESP32-S3 -> mic | I2S bit clock |
| `LRCLK` / `WS` | `GPIO5` | ESP32-S3 -> mic | I2S word select |
| `DOUT` | `GPIO6` | mic -> ESP32-S3 | I2S data |
| `SEL` | `GPIO7` | ESP32-S3 -> mic | Firmware держит `0`, микрофон передаёт left channel |

Прошивка читает stereo I2S frames и берёт left slot. MCLK не используется.

## Adafruit SPI Flash SD Card - XTSD 2 GB

| Пин Adafruit XTSD | ESP32-S3 | Направление | Назначение |
| --- | --- | --- | --- |
| `VIN` | `3V3` | питание | Питание reader и уровень SPI-логики 3.3 V |
| `3V3` | не подключать | выход питания | Это выход регулятора на модуле |
| `GND` | `GND` | земля | Общая земля |
| `SCK` | `GPIO12` | ESP32-S3 -> reader | SPI clock |
| `MISO` | `GPIO13` | reader -> ESP32-S3 | SPI data от SD |
| `MOSI` | `GPIO11` | ESP32-S3 -> reader | SPI data в SD |
| `CS` | `GPIO10` | ESP32-S3 -> reader | Chip select |

SD используется как raw circular buffer, без FAT. Startup self-test пишет и читает блок `32768`, как в light firmware.

## Источники в коде

- `main/include/device_runtime_config.h`
- `main/microphone.c`
- `main/sd_card_buffer.c`
- `main/server_health.c`

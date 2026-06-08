# Текущая распиновка

Плата в проекте: `pico2_w` (`CMakeLists.txt`).

Сейчас прошивка использует I2S-микрофон, INA219, SD card reader и e-paper дисплей Pimoroni `Pico Inky Pack` 2.9" (`296x128`).

Важно: таблицы ниже описывают GPIO, а не физические номера ножек Pico.

## I2S-микрофон

| GPIO | Сигнал | Направление | Назначение |
| --- | --- | --- | --- |
| `GPIO0` | `DIN` / `DOUT` микрофона | вход | Данные от MEMS-микрофона в Pico |
| `GPIO1` | `BCK` | выход | I2S bit clock |
| `GPIO2` | `LRCK` | выход | I2S word/frame clock |
| `GPIO3` | `SEL` | выход | Прошивка выставляет `1`; фактический активный слот в текущей I2S-библиотеке читается как левый |
| `GPIO12` | `DOUT` dummy | выход / конфликт | Формально передаётся в I2S-библиотеку и настраивается PIO как выход; на штатном `Pico Inky Pack` этот GPIO занят кнопкой `A` |

## INA219 wattmeter

| GPIO | Сигнал | Направление | Назначение |
| --- | --- | --- | --- |
| `GPIO6` | `SDA` | двунаправленный | I2C data для INA219 на `I2C1` |
| `GPIO7` | `SCL` | выход | I2C clock для INA219 на `I2C1` |

## SD card reader

| GPIO | Сигнал | Направление | Назначение |
| --- | --- | --- | --- |
| `GPIO8` | `MISO` | вход | SPI data от SD card reader на `SPI1` |
| `GPIO9` | `CS` | выход | Chip select SD card reader на `SPI1` |
| `GPIO10` | `SCK` | выход | SPI clock SD card reader на `SPI1` |
| `GPIO11` | `MOSI` | выход | SPI data в SD card reader на `SPI1` |

SD-карта сейчас используется как простой raw circular buffer, а не как FAT-файловая система. Прошивка пишет последние 5 секунд `PCM16 mono 16 kHz` в фиксированную тестовую область карты и постоянно перезаписывает её по кругу. Этот слой изолирован в `sd_card_buffer` и позже должен быть заменён на нормальный FatFs-backed storage.

## E-paper дисплей Pimoroni 2.9"

| GPIO | Сигнал | Направление | Назначение |
| --- | --- | --- | --- |
| `GPIO16` | `MISO` | вход | SPI data от дисплея/pack; текущий драйвер `UC8151` передаёт `PIN_UNUSED`, но линия физически занята pack |
| `GPIO17` | `CS` | выход | Chip select дисплея |
| `GPIO18` | `SCK` | выход | SPI clock |
| `GPIO19` | `MOSI` | выход | SPI data |
| `GPIO20` | `DC` | выход | Data/command |
| `GPIO21` | `RESET` | выход | Сброс дисплея |
| `GPIO26` | `BUSY` | вход | Busy/status дисплея |

## Кнопки Pico Inky Pack

| GPIO | Сигнал | Направление | Назначение |
| --- | --- | --- | --- |
| `GPIO12` | `SWA` / `A` | вход | Кнопка A на `Pico Inky Pack`; конфликтует с текущим I2S `DOUT` dummy |
| `GPIO13` | `SWB` / `B` | вход | Кнопка B на `Pico Inky Pack`; текущая прошивка её не читает |
| `GPIO14` | `SWC` / `C` | вход | Кнопка C на `Pico Inky Pack`; текущая прошивка её не читает |

## Что важно

- `GPIO1` и `GPIO2` идут парой из `clock_pin_base = 1`.
- Порядок линий для входного I2S в используемой библиотеке: `DIN`, `BCK`, `LRCK`, поэтому:
  - `GPIO0` = `DIN` со стороны Pico = `DOUT` со стороны микрофона
  - `GPIO1` = `BCK`
  - `GPIO2` = `LRCK`
- `GPIO3` инициализируется как выход `1`, то есть логически соответствует посадке `SEL` в `3V3`.
- Текущий рабочий захват аудио идёт из слота `0`: `AUDIO_STREAM_QUEUE_CAPTURE_CHANNEL_INDEX = 0`. По диагностике именно этот слот несёт данные при `SEL=1`.
- `SCK` / master clock в текущей конфигурации отключён: `mic_cfg.sck_enable = false`. Если микрофону нужен `MCLK`, эта прошивка его не выдаёт.
- `GPIO12` не нужен как реальный аудиосигнал, но оставлен в конфигурации из-за `i2s_program_start_synched(...)`. С подключённым `Pico Inky Pack` он уже не является неподключённым: это кнопка `A`, поэтому текущая конфигурация I2S конфликтует с аппаратной распиновкой pack.
- `GPIO6` и `GPIO7` зарезервированы под INA219, поэтому их нельзя использовать как dummy-линии для I2S.
- `GPIO8` зарезервирован под `MISO` SD card reader, поэтому его нельзя использовать как dummy-линию для I2S.
- `GPIO16` лучше считать занятым `Pico Inky Pack`, даже если текущий `UC8151` код не читает MISO.
- Дисплей подключён через официальный драйвер Pimoroni `uc8151` и `pico_graphics`.
- Вывод логов включён через `USB`, а не через `UART`:
  - `pico_enable_stdio_uart(embeded 0)`
  - `pico_enable_stdio_usb(embeded 1)`

## Источник

- `src/microphone.c`
- `src/inky_status_display.cpp`
- `build/_deps/pimoroni_pico-src/micropython/examples/pico_inky/button_test.py`
- `build/_deps/pimoroni_pico-src/examples/inky_pack/inky_pack_demo.cpp`
- `build/_deps/rp2040_i2s_example-src/README.md`
- `build/_deps/rp2040_i2s_example-src/i2s.pio`

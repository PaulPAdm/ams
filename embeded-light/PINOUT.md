# Текущая распиновка `embeded-light`

Плата в проекте: `pico2_w` (`CMakeLists.txt`).

Эта версия прошивки использует I2S-микрофон и SD card reader. Блоки INA219 и e-paper дисплея полностью вырезаны.

Важно: таблицы ниже описывают GPIO, а не физические номера ножек Pico.

## I2S-микрофон

| GPIO | Сигнал | Направление | Назначение |
| --- | --- | --- | --- |
| `GPIO0` | `DIN` / `DOUT` микрофона | вход | Данные от MEMS-микрофона в Pico |
| `GPIO1` | `BCK` | выход | I2S bit clock |
| `GPIO2` | `LRCK` | выход | I2S word/frame clock |
| `GPIO3` | `SEL` | выход | Прошивка выставляет `1`; рабочий захват идёт из I2S slot `0` |
| `GPIO12` | `DOUT` dummy | выход / не подключать | Требуется выбранной I2S-библиотеке для `i2s_program_start_synched(...)`; к микрофону не подключается |

## Adafruit SPI Flash SD Card - XTSD 2 GB

Используемый reader: Adafruit `SPI Flash SD Card - XTSD 2 GB` в режиме обычной SD-карты по SPI. Логика прошивки остаётся прежней: raw circular buffer поверх SD block read/write, без FAT.

| Пин Adafruit | Куда подключать на Pico 2 W | Направление | Назначение |
| --- | --- | --- | --- |
| `VIN` | `3V3(OUT)` | питание | Питание reader и уровень SPI-логики 3.3 V |
| `3.3V` | не подключать | выход питания | Это выход встроенного регулятора reader, не вход питания для этой схемы |
| `GND` | любой `GND` | земля | Общая земля Pico и reader |
| `SCK` | `GPIO10` / `SPI1 SCK` | Pico -> reader | SPI clock |
| `MISO` | `GPIO8` / `SPI1 RX` | reader -> Pico | SPI data от SD в Pico |
| `MOSI` | `GPIO11` / `SPI1 TX` | Pico -> reader | SPI data из Pico в SD |
| `CS` | `GPIO9` | Pico -> reader | Chip select, активный низкий |

SD-карта используется как простой raw circular buffer, а не как FAT-файловая система. Прошивка пишет последние 5 секунд `PCM16 mono 16 kHz` в фиксированную область карты и постоянно перезаписывает её по кругу.

## Что важно

- `GPIO1` и `GPIO2` идут парой из `clock_pin_base = 1`.
- Порядок линий для входного I2S в используемой библиотеке: `DIN`, `BCK`, `LRCK`, поэтому:
  - `GPIO0` = `DIN` со стороны Pico = `DOUT` со стороны микрофона
  - `GPIO1` = `BCK`
  - `GPIO2` = `LRCK`
- `GPIO3` инициализируется как выход `1`, то есть логически соответствует посадке `SEL` в `3V3`.
- `SCK` / master clock в текущей конфигурации отключён: `mic_cfg.sck_enable = false`. Если микрофону нужен `MCLK`, эта прошивка его не выдаёт.
- `GPIO12` не нужен как реальный аудиосигнал, но PIO настраивает его как выход; держать неподключённым.
- Для Adafruit XTSD подключать питание именно в `VIN` от `3V3(OUT)` Pico. Пин `3.3V` на reader оставить неподключённым; не соединять одновременно `VIN` и `3.3V`.
- INA219 отсутствует, поэтому `GPIO6` и `GPIO7` свободны.
- Pico Inky Pack / e-paper дисплей отсутствует, поэтому `GPIO16`-`GPIO21` и `GPIO26` не используются этой прошивкой.
- Вывод логов включён через `USB`, а не через `UART`:
  - `pico_enable_stdio_uart(embeded_light 0)`
  - `pico_enable_stdio_usb(embeded_light 1)`
- В health report power/INA219-поля отправляются как `unknown`; backend сохраняет их как `null`, frontend показывает `Power: Unknown`.

## Источник

- `src/microphone.c`
- `src/sd_card_buffer.c`
- `src/server_health.c`
- <https://learn.adafruit.com/adafruit-spi-flash-sd-card/pinouts>
- `build/_deps/rp2040_i2s_example-src/README.md`
- `build/_deps/rp2040_i2s_example-src/i2s.pio`

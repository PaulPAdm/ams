# Firmware embedded-light

Katalog `embeded-light` zawiera wariant firmware dla Raspberry Pi Pico 2 W bez monitora energii INA219 i bez wyświetlacza e-paper. Logika sieci, synchronizacji czasu, detekcji audio, bufora SD, kalibracji przez USB i wysyłki zdarzeń zostaje taka sama jak w `embeded`.

## Platforma

- Raspberry Pi Pico 2 W
- Pico SDK 2.2.0
- CMake
- kompilator Arm `arm-none-eabi`
- I2S microphone przez bibliotekę `rp2040_i2s_example`
- Adafruit SPI Flash SD Card - XTSD 2 GB jako bufor audio
- CMSIS-DSP

## Różnice względem `embeded`

- Brak `ina219_wattmeter` i `power_meter_service`.
- Brak `inky_status_display`, Pimoroni `uc8151` i `pico_graphics`.
- Brak zależności `hardware_i2c`, bo firmware nie używa już INA219.
- Health report wysyła `unknown` dla pól INA219/power. Backend zapisuje te wartości jako `null`, a frontend pokazuje `Power: Unknown`.
- Setup, diagnostyka i kalibracja są dostępne przez USB console.

## Struktura

```text
embeded-light/
├── include/              # pliki nagłówkowe modułów
├── src/                  # implementacja firmware
├── CMakeLists.txt
├── PINOUT.md             # aktualna rozpiska pinów
└── pico_sdk_import.cmake
```

Najważniejsze moduły:

- `main.c` - orkiestracja startu i pętli głównej.
- `device_config` - konfiguracja urządzenia.
- `network_runtime` i `wifi_service` - połączenie Wi-Fi i oszczędzanie energii.
- `time_sync_service` i `time_sync_runtime` - synchronizacja zegara z backendem.
- `server_health` i `health_runtime` - raporty stanu urządzenia.
- `microphone`, `audio_stream_queue`, `sound_event_detector`, `acoustic_runtime` - tor audio i detekcja zdarzeń.
- `sd_card_buffer` - bufor ostatnich próbek audio na Adafruit XTSD 2 GB.
- `sound_event_uploader` - wysyłka metadanych i fragmentu audio do backendu.

## Budowanie

Typowy przepływ z katalogu `embeded-light`:

```powershell
cmake -S . -B build
cmake --build build
```

Wynikowy plik UF2 powinien powstać w:

```text
build/embeded_light.uf2
```

## Połączenia sprzętowe

Aktualna rozpiska pinów jest w [PINOUT.md](PINOUT.md). Najważniejsze bloki:

- mikrofon I2S: `GPIO0`, `GPIO1`, `GPIO2`, `GPIO3`, dummy `GPIO12`,
- Adafruit XTSD 2 GB: `VIN` do `3V3(OUT)`, `GND` do `GND`, `MISO`=`GPIO8`, `CS`=`GPIO9`, `SCK`=`GPIO10`, `MOSI`=`GPIO11`; pin `3.3V` na module zostaje niepodłączony.

## Uwagi

- Logi są wysyłane przez USB, nie przez UART.
- Wi-Fi jest aktywowane tylko wtedy, gdy urządzenie potrzebuje komunikacji z backendem.
- Bufor SD jest obecnie prostym buforem kołowym, a nie pełnym systemem plików FAT.
- Power telemetry jest świadomie niedostępne w tym wariancie.

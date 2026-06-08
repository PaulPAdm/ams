# Firmware embedded

Katalog `embeded` zawiera firmware dla urządzenia pomiarowego opartego o Raspberry Pi Pico 2 W. Urządzenie lokalnie analizuje dźwięk, synchronizuje czas z backendem, raportuje stan i przygotowuje dane zdarzeń akustycznych.

## Platforma

- Raspberry Pi Pico 2 W
- Pico SDK 2.2.0
- CMake
- kompilator Arm `arm-none-eabi`
- I2S microphone przez bibliotekę `rp2040_i2s_example`
- Pimoroni Pico Inky Pack 2.9"
- INA219 do pomiaru napięcia i prądu
- SD card reader jako bufor audio
- CMSIS-DSP

## Struktura

```text
embeded/
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
- `power_meter_service`, `ina219_wattmeter` - pomiar zasilania.
- `sd_card_buffer` - bufor ostatnich próbek audio.
- `inky_status_display` - status na wyświetlaczu e-paper.

## Logika działania

1. Firmware startuje, inicjalizuje stdio i pomiar zasilania.
2. Sprawdza konsolę USB i wczytuje konfigurację urządzenia.
3. Uruchamia diagnostykę oraz wyświetlacz e-paper.
4. Łączy się z Wi-Fi.
5. Synchronizuje czas z backendem na VPS.
6. Usypia Wi-Fi, aby ograniczyć zużycie energii.
7. Uruchamia mikrofon I2S i lokalną detekcję zdarzeń.
8. W pętli głównej analizuje próbki audio, aktualizuje status, synchronizuje czas i wysyła health reports.
9. Po wykryciu impulsu przygotowuje metadane zdarzenia i fragment audio do wysłania do backendu.

Schemat tej logiki znajduje się w:

```text
../docs/diagrams/02_logika_urzadzenia_embedded.svg
```

## Budowanie

Typowy przepływ z katalogu `embeded`:

```powershell
cmake -S . -B build
cmake --build build
```

Wynikowy plik UF2 powinien powstać w:

```text
build/embeded.uf2
```

Po zmianach platformy, toolchaina lub zależności Pico warto usunąć katalog `build` i zbudować projekt od zera.

## Połączenia sprzętowe

Aktualna rozpiska pinów jest w [PINOUT.md](PINOUT.md). Najważniejsze bloki:

- mikrofon I2S: `GPIO0`, `GPIO1`, `GPIO2`, `GPIO3`,
- INA219: `GPIO6`, `GPIO7`,
- SD card reader: `GPIO8`-`GPIO11`,
- e-paper Pimoroni: `GPIO17`-`GPIO21`, `GPIO26`.

## Konfiguracja runtime

Stałe czasu są w `include/device_runtime_config.h`:

- synchronizacja czasu: kilka prób, okresowo co kilka minut,
- health report: cykliczny raport stanu,
- post-capture audio: krótkie okno po wykryciu zdarzenia.
- audio calibration: tryb USB wybiera 6 dominujących pasm częstotliwości i zapisuje progi detekcji.

Konfiguracja urządzenia musi zawierać między innymi:

- identyfikator urządzenia,
- dane sieci Wi-Fi,
- host lub adres IP serwera backend/VPS,
- kalibrację pasm audio dla detekcji impulsów.

## Kalibracja audio

Po podłączeniu USB menu startowe ma tryb `Start Audio Calibration`. Firmware słucha mikrofonu przez podany czas, liczy widmo przez CMSIS-DSP FFT i wybiera 6 dominujących pasm. Dla każdego pasma zapisywana jest energia bazowa oraz mnożnik progu, domyślnie `2.00x`. Po kalibracji można wydrukować tabelę, zmienić mnożnik dla każdego pasma i zapisać ustawienia do flash.

## Komunikacja z backendem

Firmware korzysta z endpointów:

- `/api/devices/{device_id}/time/sync`
- `/api/devices/{device_id}/health/report`
- `/api/devices/{device_id}/sound-events`
- `/api/devices/{device_id}/sound-events/{event_id}/audio`

Synchronizacja czasu jest kluczowa dla triangulacji, ponieważ backend porównuje czasy dotarcia tego samego impulsu do różnych urządzeń.

## Uwagi

- Logi są wysyłane przez USB, nie przez UART.
- Wi-Fi jest aktywowane tylko wtedy, gdy urządzenie potrzebuje komunikacji z backendem.
- Bufor SD jest obecnie prostym buforem kołowym, a nie pełnym systemem plików FAT.

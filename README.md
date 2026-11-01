
<h1>MeshCore Firmware+ HYDRA optimization esp32+E22P</h1>

## Hardware

Target board: `hydraE22P` (`variants/esp32/diy/hydra_e22p`)

- MCU: ESP32-D0WD-V3 rev 3.1 (ESP32-WROOM-32D DevKit, `esp32doit-devkit-v1`)
- Flash: 4 MB
- SRAM: 320 KB (327 680 B)
- PSRAM: none (`MAX_NUM_NODES=80` keeps RAM headroom)
- LoRa: Ebyte E22P-868M30S (EU_868, 30 dBm PA, `-D USE_EBYTE_E22P`)
- Build usage (current build): RAM 30.4 %, flash 92.5 % of the 2 424 832 B app partition

# Hydra E22P: opis zmian w płytce

Zmiany tylko w wariancie `variants/hydra_e22p/` (Hydra = ESP32 + Ebyte E22P-868M30S). Wersje firmware: `v1.18.0-hydra-N`.

- v1.18.0-hydra-1 - pierwszy wariant Hydra E22P: GPIO 14 stale HIGH, reset radia GPIO 23, TCXO 1,8 V, moc 22 dBm.
- v1.18.0-hydra-2 - moc TX 20 dBm (jak w Meshtastic) i diagnostyka braku Wi-Fi w `start ota`.
- v1.18.0-hydra-3 - repeater łączy się z domowym Wi-Fi, OTA przez ElegantOTA z hasłem, `/version`. Pierwszy test OTA przez Wi-Fi.
- v1.18.0-hydra-4 - monitoring przez Wi-Fi: `/cli`, `/stats`, `/log`, strażnik Wi-Fi. Lista węzłów z ogłoszeń na `/` i `/nodes`.
- v1.18.0-hydra-5 - podgląd kanałów Public (`/kanal`, `/channels`), zegar z NTP.
- v1.18.0-hydra-6 - zakładki kanałów na `/kanal`.
- v1.18.0-hydra-7 - lista 64 popularnych kanałów `#` i statystyka wg ID kanału.
- v1.18.0-hydra-8 - zapasowa sieć Wi-Fi (`WIFI_SSID2`), przełączanie po 60 s bez połączenia.
- v1.18.0-hydra-9 - sieć główna Meshtastic (AP RPi), zapasowa VVV, hasło AP z env.
- v1.18.0-hydra-10 do v1.18.0-hydra-12 - bufory pakietów `/grp` i `/paths`, zapasowa sieć wyłączona. Dziennik nie podaje, który numer builda zawiera którą zmianę.
- v1.18.0-hydra-13 - zapasowa sieć Wi-Fi tylko z zmiennych `HYDRA_WIFI_SSID2` / `HYDRA_WIFI_PSK2` w `kompiluj.sh`, bez nich brak sieci zapasowej.
- v1.18.0-hydra-14 - Wi-Fi jak w Meshtastic: bez oszczędzania energii, bez auto-reconnect, czyszczenie danych sieci przed `begin`, log przyczyny rozłączenia.
- v1.18.0-hydra-15 - historia RSSI Wi-Fi w RAM (`/rssi`, próbka co 60 s przez 8 h).
- v1.18.0-hydra-16 - przycinanie nazwy węzła po pełnych znakach UTF-8, żeby `/nodes` zawsze zwracało poprawny JSON.

Ostatni wgrany build: v1.18.0-hydra-16. Commity na gałęzi `hydra`: `fafe81aa` (Wi-Fi jak w Meshtastic, RSSI), `9c75faf3` (UTF-8 w nazwie węzła).

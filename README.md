
<h1>MeshCore Firmware+ HYDRA optimization esp32+E22P</h1>

## Hardware

Target board: `hydraE22P` (`variants/esp32/diy/hydra_e22p`)

- MCU: ESP32-D0WD-V3 rev 3.1 (ESP32-WROOM-32D DevKit, `esp32doit-devkit-v1`)
- Flash: 4 MB
- SRAM: 320 KB (327 680 B)
- PSRAM: none (`MAX_NUM_NODES=80` keeps RAM headroom)
- LoRa: Ebyte E22P-868M30S (EU_868, 30 dBm PA, `-D USE_EBYTE_E22P`)
- Build usage (current build): RAM 30.4 %, flash 92.5 % of the 2 424 832 B app partition

# Hydra E22P: board change log

Changes only in the `variants/hydra_e22p/` variant (Hydra = ESP32 + Ebyte E22P-868M30S). Firmware versions: `v1.18.0-hydra-N`.

- v1.18.0-hydra-1 - first Hydra E22P variant: GPIO 14 held HIGH, radio reset on GPIO 23, TCXO 1.8 V, 22 dBm power.
- v1.18.0-hydra-2 - TX power 20 dBm (as in Meshtastic) and Wi-Fi diagnostics in `start ota`.
- v1.18.0-hydra-3 - repeater connects to the home Wi-Fi, OTA via ElegantOTA with a password, `/version`. First OTA test over Wi-Fi.
- v1.18.0-hydra-4 - monitoring over Wi-Fi: `/cli`, `/stats`, `/log`, Wi-Fi watchdog. Node list from adverts at `/` and `/nodes`.
- v1.18.0-hydra-5 - view of the Public and ` channels (`/kanal`, `/channels`), clock from NTP.
- v1.18.0-hydra-6 - channel tabs on `/kanal`.
- v1.18.0-hydra-7 - list of 64 popular `#` channels and statistics per channel ID.
- v1.18.0-hydra-8 - backup Wi-Fi network (`WIFI_SSID2`), switching after 60 s without a connection.
- v1.18.0-hydra-9 - main network Meshtastic (RPi AP), backup VVV, AP password from env.
- v1.18.0-hydra-10 to v1.18.0-hydra-12 - packet buffers `/grp` and `/paths`, backup network disabled. The journal does not record which build number contains which change.
- v1.18.0-hydra-13 - backup Wi-Fi only from the variables `HYDRA_WIFI_SSID2` / `HYDRA_WIFI_PSK2` in `kompiluj.sh`; without them there is no backup network.
- v1.18.0-hydra-14 - Wi-Fi as in Meshtastic: no power saving, no auto-reconnect, network data cleared before `begin`, log of the disconnect reason.
- v1.18.0-hydra-15 - Wi-Fi RSSI history in RAM (`/rssi`, one sample every 60 s for 8 h).
- v1.18.0-hydra-16 - node name cut at complete UTF-8 characters, so `/nodes` always returns valid JSON.
- v1.18.0-hydra-20261102-1709 - UDP multicast: repeater sends every packet from radio and its own packets to 239.77.67.1:5600.
- v1.18.0-hydra-20261102-1721 - `hydra send <key> <text>` CLI command for group messages.
- v1.18.0-hydra-20261102-1832 - UDP receive (`HYDRA_UDP_RX`): frames from other boards on the LAN go into the router, without retransmission over LoRa.

Last flashed build: v1.18.0-hydra-20261102-1832. Commit on the `hydra-e22p` branch: 77c7ba21 (UDP link, hydra send, Wi-Fi and monitoring).

## Credits

- MeshCore firmware (original project): [meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore), MIT license (see `license.txt`).
- Board designs: [NanoVHF/Meshtastic-DIY](https://github.com/NanoVHF/Meshtastic-DIY).

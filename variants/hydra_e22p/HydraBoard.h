#pragma once

#include <helpers/ESP32Board.h>

// Hydra: ESP32Board plus firmware update and monitoring over the home Wi-Fi, like Meshtastic on the Hydras.
// With HYDRA_WIFI_OTA the board joins WIFI_SSID at boot and serves (auth: admin / HYDRA_OTA_PASSWORD where noted):
//   /update  ElegantOTA (auth)          /version  running version
//   /cli     repeater CLI command (auth), e.g. /cli?cmd=stats-core, run in the main loop
//   /stats   uptime, reset reason, heap, Wi-Fi and radio counters (JSON)
//   /rssi    Wi-Fi signal history, a sample a minute for 8 h (JSON), ?since=<uptime_s> for newer samples only
//   /log     packet log file (after "log start")
class HydraBoard : public ESP32Board {
public:
#ifdef HYDRA_WIFI_OTA
  typedef void (*CliHandler)(char* command, char* reply);
  void setCliHandler(CliHandler handler);

  void onBootComplete() override;
  void loop() override;
  bool startOTAUpdate(const char* id, char reply[]) override;
#endif
};

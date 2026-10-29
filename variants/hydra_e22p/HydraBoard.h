#pragma once

#include <helpers/ESP32Board.h>

// Hydra: ESP32Board plus firmware update over the home Wi-Fi, like Meshtastic OTA on the Hydras.
// With HYDRA_WIFI_OTA the board joins WIFI_SSID at boot and serves ElegantOTA at http://<ip>/update
// (basic auth: admin / HYDRA_OTA_PASSWORD) and the running version at http://<ip>/version.
class HydraBoard : public ESP32Board {
public:
#ifdef HYDRA_WIFI_OTA
  void onBootComplete() override;
  void loop() override;
  bool startOTAUpdate(const char* id, char reply[]) override;
#endif
};

#include "HydraBoard.h"

#ifdef HYDRA_WIFI_OTA

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION "unknown"
#endif

static AsyncWebServer* ota_server = NULL;
static char hostname[24];

void HydraBoard::onBootComplete() {
  inhibit_sleep = true;   // light sleep would drop the Wi-Fi connection

  uint8_t mac[6];
  WiFi.macAddress(mac);
  sprintf(hostname, "hydra-mc-%02x%02x%02x", mac[3], mac[4], mac[5]);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PWD);
}

void HydraBoard::loop() {
  // start the web server once, after the first Wi-Fi connection (auto reconnect keeps it reachable later)
  if (ota_server == NULL && WiFi.status() == WL_CONNECTED) {
    ota_server = new AsyncWebServer(80);
    ota_server->on("/version", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", FIRMWARE_VERSION);
    });
    AsyncElegantOTA.setID(hostname);
    AsyncElegantOTA.begin(ota_server, "admin", HYDRA_OTA_PASSWORD);
    ota_server->begin();
    Serial.printf("Hydra OTA: http://%s/update (%s)\n", WiFi.localIP().toString().c_str(), FIRMWARE_VERSION);
  }
}

bool HydraBoard::startOTAUpdate(const char* id, char reply[]) {
  if (WiFi.status() != WL_CONNECTED) {
    strcpy(reply, "Wi-Fi not connected");
    return true;
  }
  sprintf(reply, "OTA: http://%s/update", WiFi.localIP().toString().c_str());
  return true;
}

#endif

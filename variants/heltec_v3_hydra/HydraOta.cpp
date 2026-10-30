#include "HydraOta.h"

#ifdef HYDRA_COMPANION_OTA

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION "v1.18.0"
#endif

static AsyncWebServer* ota_server = NULL;

void hydraOtaLoop() {
  // start the web server once, after the first Wi-Fi connection (it keeps serving after reconnects)
  if (ota_server != NULL || WiFi.status() != WL_CONNECTED) return;

  ota_server = new AsyncWebServer(80);
  ota_server->on("/version", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", FIRMWARE_VERSION);
  });
  ota_server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("/update");
  });
  char id[24];
  uint64_t mac = ESP.getEfuseMac();
  sprintf(id, "heltec-%02x%02x%02x", (uint8_t)(mac >> 24), (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
  AsyncElegantOTA.setID(id);
  AsyncElegantOTA.begin(ota_server, "admin", HYDRA_OTA_PASSWORD);
  ota_server->begin();
  Serial.printf("Hydra OTA: http://%s/update (%s)\n", WiFi.localIP().toString().c_str(), FIRMWARE_VERSION);
}

#endif

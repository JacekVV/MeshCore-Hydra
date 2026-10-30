#pragma once

// Hydra: OTA updates over Wi-Fi for the companion radio (stock MeshCore has OTA only in repeaters and room servers).
// After the first Wi-Fi connection: ElegantOTA at http://<ip>/update (user "admin", password HYDRA_OTA_PASSWORD)
// and the firmware version at /version. Called from the companion loop() under #ifdef HYDRA_COMPANION_OTA.
void hydraOtaLoop();

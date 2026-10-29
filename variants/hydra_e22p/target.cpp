#include <Arduino.h>
#include "target.h"

// Hydra: ESP32 + Ebyte E22P-868M30S. Same as generic-e22, except pin 14 powers the E22P PA and must stay HIGH
// (it is not RXEN on the E22P), and the radio reset is wired to GPIO 23.

ESP32Board board;

static SPIClass spi;
RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, spi);

WRAPPER_CLASS radio_driver(radio, board);

ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);
SensorManager sensors;

bool radio_init() {
  digitalWrite(P_LORA_PA_POWER, HIGH);
  pinMode(P_LORA_PA_POWER, OUTPUT);
  delay(10);

  fallback_clock.begin();
  rtc_clock.begin(Wire);
  return radio.std_init(&spi);
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng);  // create new random identity
}

#include "HydraUdp.h"

#ifdef HYDRA_UDP

#include <WiFi.h>
#include <WiFiUdp.h>
#include <string.h>

#define UDP_HDR_LEN  3
#define UDP_VERSION  1

bool hydraUdpInjecting = false;

static WiFiUDP udp;
static bool udp_up = false;
static IPAddress group_ip;
static uint8_t tx_buf[UDP_HDR_LEN + MAX_TRANS_UNIT];
static uint8_t rx_buf[UDP_HDR_LEN + MAX_TRANS_UNIT];

static void sendRaw(const uint8_t* raw, int len) {
  if (!udp_up || len <= 0 || len > MAX_TRANS_UNIT) return;
  tx_buf[0] = 'H';
  tx_buf[1] = 'C';
  tx_buf[2] = UDP_VERSION;
  memcpy(&tx_buf[UDP_HDR_LEN], raw, len);
  udp.beginPacket(group_ip, HYDRA_UDP_PORT);
  udp.write(tx_buf, UDP_HDR_LEN + len);
  udp.endPacket();
}

void hydraUdpSendRaw(const uint8_t* raw, int len) {
  sendRaw(raw, len);
}

// same bytes as Dispatcher::checkSend() puts on the radio
void hydraUdpSendPacket(const mesh::Packet* pkt) {
  uint8_t raw[MAX_TRANS_UNIT];
  int len = 0;
  raw[len++] = pkt->header;
  if (pkt->hasTransportCodes()) {
    memcpy(&raw[len], &pkt->transport_codes[0], 2); len += 2;
    memcpy(&raw[len], &pkt->transport_codes[1], 2); len += 2;
  }
  raw[len++] = pkt->path_len;
  len += mesh::Packet::writePath(&raw[len], pkt->path, pkt->path_len);
  if (len + pkt->payload_len > MAX_TRANS_UNIT) return;
  memcpy(&raw[len], pkt->payload, pkt->payload_len); len += pkt->payload_len;
  sendRaw(raw, len);
}

void hydraUdpLoop(mesh::Dispatcher& dispatcher) {
  bool wifi_up = WiFi.status() == WL_CONNECTED;
  if (wifi_up && !udp_up) {
#ifdef HYDRA_UDP_RX
    group_ip.fromString(HYDRA_UDP_GROUP);
    udp_up = udp.beginMulticast(group_ip, HYDRA_UDP_PORT);
#else
    // sending only: the boards do not receive, the RPi does (multicast frames go out to the whole LAN)
    group_ip.fromString(HYDRA_UDP_GROUP);
    udp_up = udp.begin(HYDRA_UDP_PORT);
#endif
    Serial.printf("Hydra UDP: %s:%d %s\n", HYDRA_UDP_GROUP, HYDRA_UDP_PORT, udp_up ? "on" : "failed");
  } else if (!wifi_up && udp_up) {
    udp.stop();
    udp_up = false;
  }
  if (!udp_up) return;

#ifdef HYDRA_UDP_RX
  // parsePacket() drops whatever part of an oversized datagram did not fit in rx_buf
  while (udp.parsePacket() > 0) {
    int got = udp.read(rx_buf, sizeof(rx_buf));
    if (got <= UDP_HDR_LEN || rx_buf[0] != 'H' || rx_buf[1] != 'C' || rx_buf[2] != UDP_VERSION) continue;
    if (udp.remoteIP() == WiFi.localIP()) continue;   // our own multicast, looped back

    hydraUdpInjecting = true;
    dispatcher.injectRawPacket(&rx_buf[UDP_HDR_LEN], got - UDP_HDR_LEN);
    hydraUdpInjecting = false;
  }
#else
  (void)dispatcher;
#endif
}

#endif

#pragma once

#include <Mesh.h>

// Hydra: packets heard over LoRa, and this node's own transmissions, are also sent as UDP multicast, so the other
// Hydras on the same LAN hear them directly (a "0 hop" link, as the Meshtastic UDP broadcast does). Packets received
// that way go into the normal receive path, but are never repeated over LoRa: they stay local.
// Frame: 'H' 'C' 0x01, then the raw packet bytes as the radio gets them. Other datagrams are ignored.
// Repeater only (HYDRA_UDP, needs HYDRA_WIFI_OTA for the Wi-Fi connection).
#ifndef HYDRA_UDP_GROUP
  #define HYDRA_UDP_GROUP "239.77.67.1"   // admin-scoped, not the Meshtastic group 224.0.0.69
#endif
#ifndef HYDRA_UDP_PORT
  #define HYDRA_UDP_PORT 5600             // not the Meshtastic port 4403
#endif

extern bool hydraUdpInjecting;   // true while a packet from UDP is handled; MyMesh::onRecvPacket keeps it off LoRa

void hydraUdpSendRaw(const uint8_t* raw, int len);   // packet heard over LoRa (MyMesh::logRxRaw)
void hydraUdpSendPacket(const mesh::Packet* pkt);    // packet sent over LoRa (MyMesh::logTx)
void hydraUdpLoop(mesh::Dispatcher& dispatcher);      // Wi-Fi state, receive and inject (main loop)

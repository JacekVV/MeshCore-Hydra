#include "HydraNodes.h"

#ifdef HYDRA_NODE_LIST

#include <Arduino.h>
#include <helpers/AdvertDataHelpers.h>
#include <target.h>

struct HydraNode {
  uint8_t pub_key[PUB_KEY_SIZE];
  char name[MAX_ADVERT_DATA_SIZE];
  uint8_t type;
  uint8_t hops;              // repeaters between the node and us (0 = heard directly)
  uint8_t via[4];            // path hash of the last repeater (hops > 0), via_len bytes
  uint8_t via_len;
  bool has_pos;
  int32_t lat, lon;          // degrees x 1e6
  int8_t snr4;               // x 4, of the last hop
  int16_t rssi;              // of the last hop
  uint32_t advert_timestamp; // sender's clock
  uint32_t heard_millis;
  uint32_t first_millis;
  uint16_t count;
};

static HydraNode nodes[HYDRA_MAX_NODES];
static portMUX_TYPE nodes_mux = portMUX_INITIALIZER_UNLOCKED;   // web server runs in another task

void hydraNodeHeard(const mesh::Packet* packet, const mesh::Identity& id, uint32_t timestamp,
                    const uint8_t* app_data, size_t app_data_len) {
  AdvertDataParser parser(app_data, app_data_len);
  if (!parser.isValid()) return;

  HydraNode n;
  memset(&n, 0, sizeof(n));
  memcpy(n.pub_key, id.pub_key, PUB_KEY_SIZE);
  if (parser.hasName()) strncpy(n.name, parser.getName(), sizeof(n.name) - 1);
  n.type = parser.getType();
  n.hops = packet->getPathHashCount();
  if (n.hops > 0) {
    n.via_len = packet->getPathHashSize();
    if (n.via_len > sizeof(n.via)) n.via_len = sizeof(n.via);
    memcpy(n.via, &packet->path[(n.hops - 1) * packet->getPathHashSize()], n.via_len);
  }
  n.has_pos = parser.hasLatLon();
  n.lat = parser.getIntLat();
  n.lon = parser.getIntLon();
  n.snr4 = (int8_t)(packet->getSNR() * 4);
  n.rssi = (int16_t)radio_driver.getLastRSSI();
  n.advert_timestamp = timestamp;
  n.heard_millis = millis();
  n.first_millis = n.heard_millis;
  n.count = 1;

  // find existing node, else take an empty slot, else the least recently heard
  portENTER_CRITICAL(&nodes_mux);
  int slot = -1, empty = -1, oldest = -1;
  uint32_t oldest_age = 0;
  for (int i = 0; i < HYDRA_MAX_NODES; i++) {
    if (nodes[i].count == 0) {
      if (empty < 0) empty = i;
      continue;
    }
    if (memcmp(nodes[i].pub_key, n.pub_key, PUB_KEY_SIZE) == 0) { slot = i; break; }
    uint32_t age = n.heard_millis - nodes[i].heard_millis;
    if (oldest < 0 || age > oldest_age) { oldest = i; oldest_age = age; }
  }
  if (slot >= 0) {
    n.first_millis = nodes[slot].first_millis;
    n.count = nodes[slot].count < 0xFFFF ? nodes[slot].count + 1 : 0xFFFF;
  } else {
    slot = empty >= 0 ? empty : oldest;
  }
  nodes[slot] = n;
  portEXIT_CRITICAL(&nodes_mux);
}

static void printJsonString(Print& out, const char* s) {
  out.print('"');
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') { out.print('\\'); out.print(*s); }
    else if ((uint8_t)*s < 0x20) out.printf("\\u%04x", (uint8_t)*s);
    else out.print(*s);
  }
  out.print('"');
}

static const char* typeName(uint8_t type) {
  switch (type) {
    case ADV_TYPE_CHAT:     return "chat";
    case ADV_TYPE_REPEATER: return "repeater";
    case ADV_TYPE_ROOM:     return "room";
    case ADV_TYPE_SENSOR:   return "sensor";
    default:                return "other";
  }
}

void hydraNodesJson(Print& out) {
  uint32_t now = millis();
  out.printf("{\"uptime_s\":%lu,\"nodes\":[", (unsigned long)(now / 1000));
  bool first = true;
  for (int i = 0; i < HYDRA_MAX_NODES; i++) {
    HydraNode n;
    portENTER_CRITICAL(&nodes_mux);
    n = nodes[i];
    portEXIT_CRITICAL(&nodes_mux);
    if (n.count == 0) continue;

    if (!first) out.print(',');
    first = false;
    out.print("{\"id\":\"");
    for (int j = 0; j < PUB_KEY_SIZE; j++) out.printf("%02x", n.pub_key[j]);
    out.print("\",\"name\":");
    printJsonString(out, n.name);
    out.printf(",\"type\":\"%s\",\"hops\":%u,\"via\":\"", typeName(n.type), n.hops);
    for (int j = 0; j < n.via_len; j++) out.printf("%02x", n.via[j]);
    out.printf("\",\"snr\":%.2f,\"rssi\":%d", n.snr4 / 4.0f, n.rssi);
    if (n.has_pos) out.printf(",\"lat\":%.6f,\"lon\":%.6f", n.lat / 1e6, n.lon / 1e6);
    out.printf(",\"last_heard_s\":%lu,\"first_heard_s\":%lu,\"adverts\":%u,\"advert_time\":%lu}",
               (unsigned long)((now - n.heard_millis) / 1000), (unsigned long)((now - n.first_millis) / 1000),
               n.count, (unsigned long)n.advert_timestamp);
  }
  out.print("]}");
}

#else
void hydraNodeHeard(const mesh::Packet*, const mesh::Identity&, uint32_t, const uint8_t*, size_t) { }
void hydraNodesJson(Print& out) { out.print("{\"nodes\":[]}"); }
#endif

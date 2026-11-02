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

// strncpy cuts by bytes, so a multi-byte UTF-8 character can end up half-written; /nodes JSON then is not valid UTF-8
static void trimIncompleteUtf8(char* s) {
  size_t len = strlen(s), start = len;
  while (start > 0 && len - start < 3 && ((uint8_t)s[start - 1] & 0xC0) == 0x80) start--;   // continuation bytes at the end
  if (start == 0) return;
  uint8_t lead = (uint8_t)s[start - 1];
  size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
  if (len - (start - 1) < need) s[start - 1] = 0;
}

void hydraNodeHeard(const mesh::Packet* packet, const mesh::Identity& id, uint32_t timestamp,
                    const uint8_t* app_data, size_t app_data_len) {
  AdvertDataParser parser(app_data, app_data_len);
  if (!parser.isValid()) return;

  HydraNode n;
  memset(&n, 0, sizeof(n));
  memcpy(n.pub_key, id.pub_key, PUB_KEY_SIZE);
  if (parser.hasName()) {
    strncpy(n.name, parser.getName(), sizeof(n.name) - 1);
    trimIncompleteUtf8(n.name);
  }
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

struct HydraPath {
  uint32_t seq;              // 0 = empty slot
  uint8_t type;
  uint8_t path_len;          // as in the packet: hash size and hash count
  uint8_t path[MAX_PATH_SIZE];
  uint8_t route;             // ROUTE_TYPE_*
  uint8_t payload_len;
  uint8_t payload[MAX_PACKET_PAYLOAD];   // the RPi reads what is readable without keys (adverts, hashes, channels)
  uint8_t pkt_hash[4];       // the same packet heard several times has the same hash
  int8_t snr4;
  int16_t rssi;
  uint32_t heard_millis;
};

static HydraPath* paths = NULL;   // on the heap: too big for static DRAM
static uint32_t next_path_seq = 1;
static uint8_t self_key[6];
static double self_lat, self_lon;

void hydraPathHeard(const mesh::Packet* packet, const uint8_t* self_pub_key, double lat, double lon) {
  if (paths == NULL) paths = (HydraPath*)calloc(HYDRA_MAX_PATHS, sizeof(HydraPath));   // main loop only
  if (paths == NULL) return;
  HydraPath p;
  memset(&p, 0, sizeof(p));
  p.type = packet->getPayloadType();
  p.path_len = packet->path_len;
  memcpy(p.path, packet->path, packet->getPathByteLen() > MAX_PATH_SIZE ? MAX_PATH_SIZE : packet->getPathByteLen());
  p.route = packet->getRouteType();
  p.payload_len = packet->payload_len > sizeof(p.payload) ? sizeof(p.payload) : packet->payload_len;
  memcpy(p.payload, packet->payload, p.payload_len);
  uint8_t hash[MAX_HASH_SIZE];
  packet->calculatePacketHash(hash);
  memcpy(p.pkt_hash, hash, sizeof(p.pkt_hash));
  p.snr4 = (int8_t)(packet->getSNR() * 4);
  p.rssi = (int16_t)radio_driver.getLastRSSI();
  p.heard_millis = millis();

  portENTER_CRITICAL(&nodes_mux);
  memcpy(self_key, self_pub_key, sizeof(self_key));
  self_lat = lat;
  self_lon = lon;
  p.seq = next_path_seq++;
  paths[p.seq % HYDRA_MAX_PATHS] = p;   // ring buffer, oldest overwritten
  portEXIT_CRITICAL(&nodes_mux);
}

void hydraPathsJson(Print& out) {
  uint32_t now = millis();
  uint8_t key[6];
  double lat, lon;
  portENTER_CRITICAL(&nodes_mux);
  memcpy(key, self_key, sizeof(key));
  lat = self_lat;
  lon = self_lon;
  portEXIT_CRITICAL(&nodes_mux);
  out.printf("{\"uptime_s\":%lu,\"self\":\"", (unsigned long)(now / 1000));
  for (int j = 0; j < sizeof(key); j++) out.printf("%02x", key[j]);
  out.printf("\",\"lat\":%.6f,\"lon\":%.6f,\"paths\":[", lat, lon);
  bool first = true;
  for (int i = 0; paths != NULL && i < HYDRA_MAX_PATHS; i++) {
    HydraPath p;
    portENTER_CRITICAL(&nodes_mux);
    p = paths[i];
    portEXIT_CRITICAL(&nodes_mux);
    if (p.seq == 0) continue;

    if (!first) out.print(',');
    first = false;
    uint8_t size = (p.path_len >> 6) + 1, count = p.path_len & 63;
    out.printf("{\"seq\":%lu,\"type\":%u,\"hash\":\"%02x%02x%02x%02x\",\"hash_size\":%u,\"path\":\"",
               (unsigned long)p.seq, p.type, p.pkt_hash[0], p.pkt_hash[1], p.pkt_hash[2], p.pkt_hash[3], size);
    for (int j = 0; j < size * count && j < MAX_PATH_SIZE; j++) out.printf("%02x", p.path[j]);
    out.print("\",\"origin\":\"");
    if (p.type == PAYLOAD_TYPE_ADVERT && p.payload_len >= 6) for (int j = 0; j < 6; j++) out.printf("%02x", p.payload[j]);
    out.printf("\",\"route\":%u,\"payload\":\"", p.route);
    for (int j = 0; j < p.payload_len; j++) out.printf("%02x", p.payload[j]);
    out.printf("\",\"snr\":%.2f,\"rssi\":%d,\"heard_s\":%lu}", p.snr4 / 4.0f, p.rssi, (unsigned long)((now - p.heard_millis) / 1000));
  }
  out.print("]}");
}

#else
void hydraNodeHeard(const mesh::Packet*, const mesh::Identity&, uint32_t, const uint8_t*, size_t) { }
void hydraNodesJson(Print& out) { out.print("{\"nodes\":[]}"); }
void hydraPathHeard(const mesh::Packet*, const uint8_t*, double, double) { }
void hydraPathsJson(Print& out) { out.print("{\"paths\":[]}"); }
#endif

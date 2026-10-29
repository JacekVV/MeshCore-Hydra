#include "HydraChannels.h"

#ifdef HYDRA_CHANNELS

#include <Arduino.h>
#include <Utils.h>
#include <target.h>

#ifndef HYDRA_CHANNEL_TAGS
  // comma separated hashtag channels: our own plus popular names to discover (lowercase; names with and without Polish letters)
  #define HYDRA_CHANNEL_TAGS ""     "#polska,#poland,#pl,#wroclaw,#wrocław,#poznan,#poznań,#zielonagora,#glogow,#głogów,#legnica,#rawicz,#koscian,#kościan,"     "#gostyn,#gostyń,#lubin,#opole,#kalisz,#warszawa,#warsaw,#krakow,#kraków,#lodz,#łódź,#gdansk,#katowice,#szczecin,"     "#dolnyslask,#dolnośląskie,#wielkopolska,#slask,#śląsk,#lubuskie,"     "#test,#testy,#testing,#ping,#emergency,#sos,#help,#pomoc,#ratunek,#ham,#hamradio,#sp,"     "#meshcore,#mesh,#meshtastic,#lora,#chat,#general,#random,#news,#info,#pogoda,#weather,#repeater,#europe,#eu,#offtopic,#bot,#mqtt"
#endif

#define MAX_HYDRA_CHANNELS 72
#define MSG_TEXT_SIZE      160

struct HydraChannel {
  mesh::GroupChannel ch;
  char name[24];
};

struct HydraMessage {
  uint8_t channel;
  uint8_t hops;
  int8_t snr4;               // x 4, of the last hop
  int16_t rssi;              // of the last hop
  uint32_t sent_timestamp;   // sender's clock
  uint32_t heard_millis;
  uint32_t seq;              // 0 = empty slot
  char text[MSG_TEXT_SIZE];  // "sender: message"
};

static HydraChannel channels[MAX_HYDRA_CHANNELS];

// statistics since boot: group packets (text and data) per 1-byte channel hash, and how many we could decrypt
static uint16_t hash_packets[256];
static uint16_t hash_decrypted[256];
static int8_t hash_channel[256];   // last channel decrypted with this hash, -1 none
static int num_channels = 0;
static HydraMessage messages[HYDRA_MAX_MESSAGES];
static uint32_t next_seq = 1;
static portMUX_TYPE msg_mux = portMUX_INITIALIZER_UNLOCKED;   // web server runs in another task

static void addChannel(const char* name, const uint8_t secret[16]) {
  if (num_channels >= MAX_HYDRA_CHANNELS) return;
  HydraChannel& c = channels[num_channels++];
  memset(&c, 0, sizeof(c));
  memcpy(c.ch.secret, secret, 16);
  mesh::Utils::sha256(c.ch.hash, sizeof(c.ch.hash), c.ch.secret, 16);   // same as BaseChatMesh::addChannel()
  strncpy(c.name, name, sizeof(c.name) - 1);
}

void hydraChannelsInit() {
  memset(hash_channel, -1, sizeof(hash_channel));

  // Public channel: publicly known key (docs/companion_protocol.md)
  static const uint8_t public_key[16] = { 0x8b, 0x33, 0x87, 0xe9, 0xc5, 0xcd, 0xea, 0x6a,
                                          0xc9, 0xe5, 0xed, 0xba, 0xa1, 0x15, 0xcd, 0x72 };
  addChannel("Public", public_key);

  // hashtag channels: key = first 16 bytes of sha256("#name")
  char tags[] = HYDRA_CHANNEL_TAGS;
  for (char* tag = strtok(tags, ","); tag; tag = strtok(NULL, ",")) {
    uint8_t key[16];
    mesh::Utils::sha256(key, sizeof(key), (const uint8_t*)tag, strlen(tag));
    addChannel(tag, key);
  }
}

int hydraSearchChannels(const uint8_t* hash, mesh::GroupChannel dest[], int max_matches) {
  // called once per new (not duplicate) group packet, before decryption
  if (hash_packets[hash[0]] < 0xFFFF) hash_packets[hash[0]]++;
  int n = 0;
  for (int i = 0; i < num_channels && n < max_matches; i++) {
    if (memcmp(channels[i].ch.hash, hash, sizeof(channels[i].ch.hash)) == 0) dest[n++] = channels[i].ch;
  }
  return n;
}

void hydraGroupMsg(const mesh::Packet* packet, uint8_t type, const mesh::GroupChannel& channel, const uint8_t* data, size_t len) {
  int idx = -1;
  for (int i = 0; i < num_channels; i++) {
    if (memcmp(channels[i].ch.secret, channel.secret, 16) == 0) { idx = i; break; }
  }
  if (idx < 0) return;
  if (hash_decrypted[channel.hash[0]] < 0xFFFF) hash_decrypted[channel.hash[0]]++;
  hash_channel[channel.hash[0]] = idx;

  if (type != PAYLOAD_TYPE_GRP_TXT || len < 5) return;
  if ((data[4] >> 2) != 0) return;   // unsupported text type (same check as BaseChatMesh)

  HydraMessage m;
  memset(&m, 0, sizeof(m));
  m.channel = idx;
  m.hops = packet->getPathHashCount();
  m.snr4 = (int8_t)(packet->getSNR() * 4);
  m.rssi = (int16_t)radio_driver.getLastRSSI();
  memcpy(&m.sent_timestamp, data, 4);
  m.heard_millis = millis();
  size_t n = len - 5;   // text may be zero padded
  if (n > MSG_TEXT_SIZE - 1) n = MSG_TEXT_SIZE - 1;
  memcpy(m.text, &data[5], n);
  m.text[n] = 0;

  portENTER_CRITICAL(&msg_mux);
  m.seq = next_seq++;
  messages[m.seq % HYDRA_MAX_MESSAGES] = m;   // ring buffer, oldest overwritten
  portEXIT_CRITICAL(&msg_mux);
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

void hydraChannelsJson(Print& out) {
  uint32_t now = millis();
  out.printf("{\"uptime_s\":%lu,\"channels\":[", (unsigned long)(now / 1000));
  for (int i = 0; i < num_channels; i++) {
    if (i) out.print(',');
    printJsonString(out, channels[i].name);
  }
  out.print("],\"hash_stats\":[");
  bool first_hash = true;
  for (int h = 0; h < 256; h++) {
    if (hash_packets[h] == 0) continue;
    if (!first_hash) out.print(',');
    first_hash = false;
    out.printf("{\"hash\":\"%02x\",\"packets\":%u,\"decrypted\":%u,\"channel\":", h, hash_packets[h], hash_decrypted[h]);
    printJsonString(out, hash_channel[h] >= 0 ? channels[hash_channel[h]].name : "");
    out.print('}');
  }
  out.print("],\"messages\":[");
  bool first = true;
  for (int i = 0; i < HYDRA_MAX_MESSAGES; i++) {
    HydraMessage m;
    portENTER_CRITICAL(&msg_mux);
    m = messages[i];
    portEXIT_CRITICAL(&msg_mux);
    if (m.seq == 0) continue;

    if (!first) out.print(',');
    first = false;
    out.printf("{\"seq\":%lu,\"channel\":", (unsigned long)m.seq);
    printJsonString(out, channels[m.channel].name);
    out.print(",\"text\":");
    printJsonString(out, m.text);
    out.printf(",\"hops\":%u,\"snr\":%.2f,\"rssi\":%d,\"sent_time\":%lu,\"heard_s\":%lu}",
               m.hops, m.snr4 / 4.0f, m.rssi, (unsigned long)m.sent_timestamp, (unsigned long)((now - m.heard_millis) / 1000));
  }
  out.print("]}");
}

#else
void hydraChannelsInit() { }
int hydraSearchChannels(const uint8_t*, mesh::GroupChannel[], int) { return 0; }
void hydraGroupMsg(const mesh::Packet*, uint8_t, const mesh::GroupChannel&, const uint8_t*, size_t) { }
void hydraChannelsJson(Print& out) { out.print("{\"channels\":[],\"messages\":[]}"); }
#endif

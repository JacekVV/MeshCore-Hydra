#include "HydraBoard.h"

#ifdef HYDRA_WIFI_OTA

#include <WiFi.h>
#include <esp_wifi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <SPIFFS.h>
#include <target.h>

#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION "unknown"
#endif

#ifdef HYDRA_NODE_LIST
#include "HydraNodes.h"

// node list page (Polish UI), reads /nodes every 30 s
static const char NODES_PAGE[] = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Hydra MeshCore</title>
<style>body{font-family:sans-serif;margin:12px}table{border-collapse:collapse;font-size:14px}
td,th{border:1px solid #bbb;padding:3px 6px;text-align:left}th{background:#eee}.d{color:#070;font-weight:bold}</style>
</head><body><h3>Węzły słyszane przez repeater</h3><p id="s"></p><table id="t"></table>
<p>Skoki 0 = słyszany bezpośrednio. Dla skoków &gt; 0 SNR i RSSI dotyczą ostatniego repeatera (kolumna „przez”). <a href="/kanal">Kanały</a></p>
<script>
function ago(s){return s<60?s+' s':s<3600?Math.floor(s/60)+' min':s<86400?Math.floor(s/3600)+' h':Math.floor(s/86400)+' d'}
function esc(s){return s.replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function load(){fetch('/nodes').then(r=>r.json()).then(j=>{
 j.nodes.sort((a,b)=>a.last_heard_s-b.last_heard_s);
 let h='<tr><th>Nazwa</th><th>Typ</th><th>ID</th><th>Skoki</th><th>Przez</th><th>SNR</th><th>RSSI</th><th>Ostatnio</th><th>Ogłoszeń</th><th>Pozycja</th></tr>';
 for(const n of j.nodes){h+='<tr><td>'+esc(n.name)+'</td><td>'+n.type+'</td><td>'+n.id.substr(0,8)+'</td><td'+(n.hops==0?' class="d"':'')+'>'+n.hops+
  '</td><td>'+n.via+'</td><td>'+n.snr+'</td><td>'+n.rssi+'</td><td>'+ago(n.last_heard_s)+'</td><td>'+n.adverts+'</td><td>'+
  (n.lat!==undefined?'<a href="https://www.openstreetmap.org/?mlat='+n.lat+'&mlon='+n.lon+'#map=13/'+n.lat+'/'+n.lon+'">'+n.lat.toFixed(4)+', '+n.lon.toFixed(4)+'</a>':'')+'</td></tr>'}
 document.getElementById('t').innerHTML=h;
 document.getElementById('s').textContent='Węzłów: '+j.nodes.length+', bezpośrednio: '+j.nodes.filter(n=>n.hops==0).length+', czas pracy: '+ago(j.uptime_s);
})}
load();setInterval(load,30000);
</script></body></html>)HTML";
#endif

#ifdef HYDRA_CHANNELS
#include "HydraChannels.h"

// channel messages page (Polish UI), reads /channels every 15 s
static const char CHANNELS_PAGE[] = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Hydra MeshCore kanały</title>
<style>body{font-family:sans-serif;margin:12px}table{border-collapse:collapse;font-size:14px}
td,th{border:1px solid #bbb;padding:3px 6px;text-align:left;vertical-align:top}th{background:#eee}td.t{max-width:600px}
#tabs{margin:8px 0}#tabs a{display:inline-block;padding:5px 12px;margin-right:4px;border:1px solid #bbb;border-bottom:none;
border-radius:6px 6px 0 0;text-decoration:none;color:#000;background:#eee}#tabs a.on{background:#fff;font-weight:bold}</style>
</head><body><h3>Wiadomości na kanałach</h3><p id="s"></p><div id="tabs"></div><table id="t"></table>
<p>Ostatnie wiadomości w pamięci repeatera (znikają po restarcie). Czas nadania z zegara nadawcy. SNR i RSSI dotyczą ostatniego skoku.
<a href="/">Węzły</a></p>
<script>
let data=null;
function ago(s){return s<60?s+' s':s<3600?Math.floor(s/60)+' min':s<86400?Math.floor(s/3600)+' h':Math.floor(s/86400)+' d'}
function esc(s){return s.replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function tab(){return decodeURIComponent(location.hash.slice(1))}
function show(){if(!data)return;const cur=tab();
 let t='<a href="#" class="'+(cur==''?'on':'')+'">Wszystkie ('+data.messages.length+')</a>';
 data.channels.forEach((c,i)=>{const n=data.messages.filter(m=>m.channel==c).length;
  if(i<2||n>0||cur==c)t+='<a href="#'+encodeURIComponent(c)+'" class="'+(cur==c?'on':'')+'">'+esc(c)+' ('+n+')</a>'});
 t+='<a href="#stat" class="'+(cur=='stat'?'on':'')+'">Statystyka</a>';
 document.getElementById('tabs').innerHTML=t;
 if(cur=='stat'){let tot=0,dec=0;data.hash_stats.sort((a,b)=>b.packets-a.packets);
  let h='<tr><th>ID kanału</th><th>Kanał</th><th>Wiadomości</th><th>Odszyfrowane</th></tr>';
  for(const s of data.hash_stats){tot+=s.packets;dec+=s.decrypted;
   h+='<tr><td>'+s.hash+'</td><td>'+(s.channel?esc(s.channel):'nieznany (prywatny lub inna nazwa)')+'</td><td>'+s.packets+'</td><td>'+s.decrypted+'</td></tr>'}
  document.getElementById('t').innerHTML=h;
  document.getElementById('s').textContent='Wiadomości kanałowe od startu: '+tot+', odszyfrowane: '+dec+', sprawdzane kanały: '+data.channels.length+
   ' (Public i nazwy z listy), czas pracy: '+ago(data.uptime_s);
  return}
 let h='<tr><th>Odebrano</th><th>Nadano</th>'+(cur==''?'<th>Kanał</th>':'')+'<th>Wiadomość</th><th>Skoki</th><th>SNR</th><th>RSSI</th></tr>';
 for(const m of data.messages){if(cur!=''&&m.channel!=cur)continue;
  h+='<tr><td>'+ago(m.heard_s)+' temu</td><td>'+new Date(m.sent_time*1000).toLocaleString('pl-PL')+'</td>'+(cur==''?'<td>'+esc(m.channel)+'</td>':'')+
  '<td class="t">'+esc(m.text)+'</td><td>'+m.hops+'</td><td>'+m.snr+'</td><td>'+m.rssi+'</td></tr>'}
 document.getElementById('t').innerHTML=h;
 document.getElementById('s').textContent='Kanały: '+data.channels.join(', ')+'. Wiadomości: '+data.messages.length+', czas pracy: '+ago(data.uptime_s);
}
function load(){fetch('/channels').then(r=>r.json()).then(j=>{j.messages.sort((a,b)=>b.seq-a.seq);data=j;show()})}
window.onhashchange=show;
load();setInterval(load,15000);
</script></body></html>)HTML";
#endif

static AsyncWebServer* ota_server = NULL;
static char hostname[24];
static bool ntp_started = false;

// Wi-Fi watchdog: the ESP32 may stop retrying after many failed attempts, so reconnect by hand
// With WIFI_SSID2 (fallback network) every retry switches to the other network.
#define WIFI_RETRY_MILLIS 60000
static unsigned long wifi_lost_at = 0;
static uint32_t wifi_reconnects = 0;
static const char* const wifi_ssids[] = { WIFI_SSID,
#ifdef WIFI_SSID2
  WIFI_SSID2
#endif
};
static const char* const wifi_pwds[] = { WIFI_PWD,
#ifdef WIFI_SSID2
  WIFI_PWD2
#endif
};
static int wifi_net = 0;

// Wi-Fi signal history for /rssi: one sample a minute in a ring buffer (480 samples = 8 h), kept in RAM
#define RSSI_SAMPLE_MILLIS 60000
#define RSSI_SAMPLES 480
struct RssiSample {
  uint32_t uptime_s;
  int8_t rssi;          // dBm, 0 when not connected
  uint8_t net;          // index into wifi_ssids
  uint16_t reconnects;
};
static RssiSample rssi_samples[RSSI_SAMPLES];
static uint32_t rssi_total = 0;   // samples taken since boot
static unsigned long rssi_last_sample = 0;

static void sampleRssi() {
  RssiSample& s = rssi_samples[rssi_total % RSSI_SAMPLES];
  s.uptime_s = millis() / 1000;
  s.rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  s.net = wifi_net;
  s.reconnects = wifi_reconnects;
  rssi_total++;
  rssi_last_sample = millis();
}

// CLI over HTTP: the web server task hands the command to the main loop and waits for the reply
#define CLI_WAIT_MILLIS 3000
static HydraBoard::CliHandler cli_handler = NULL;
static char cli_command[256];   // Hydra: "hydra send <klucz hex> <tekst>" needs room for a 64-character key
static char cli_reply[160];
static volatile uint8_t cli_state = 0;   // 0 idle, 1 pending, 2 done
static portMUX_TYPE cli_mux = portMUX_INITIALIZER_UNLOCKED;

static bool checkAuth(AsyncWebServerRequest *request) {
  if (request->authenticate("admin", HYDRA_OTA_PASSWORD)) return true;
  request->requestAuthentication();
  return false;
}

static void handleCli(AsyncWebServerRequest *request) {
  if (!checkAuth(request)) return;
  if (!request->hasParam("cmd") && !request->hasParam("cmd", true)) {
    request->send(400, "text/plain", "missing cmd");
    return;
  }
  String cmd = request->hasParam("cmd", true) ? request->getParam("cmd", true)->value() : request->getParam("cmd")->value();

  portENTER_CRITICAL(&cli_mux);
  bool busy = cli_state == 1;
  if (!busy) {
    strncpy(cli_command, cmd.c_str(), sizeof(cli_command) - 1);
    cli_command[sizeof(cli_command) - 1] = 0;
    cli_state = 1;
  }
  portEXIT_CRITICAL(&cli_mux);
  if (busy) {
    request->send(503, "text/plain", "busy");
    return;
  }

  unsigned long start = millis();
  while (cli_state != 2 && millis() - start < CLI_WAIT_MILLIS) delay(10);
  if (cli_state != 2) {
    request->send(504, "text/plain", "no reply (command still queued)");
    return;
  }
  request->send(200, "text/plain", cli_reply);
}

static void handleStats(AsyncWebServerRequest *request) {
  AsyncResponseStream* r = request->beginResponseStream("application/json");
  uint32_t reason = board.getResetReason();
  r->printf("{\"version\":\"%s\",\"uptime_s\":%lu,\"reset_reason\":%lu,\"reset_reason_text\":\"%s\",",
            FIRMWARE_VERSION, (unsigned long)(millis() / 1000), (unsigned long)reason, board.getResetReasonString(reason));
  r->printf("\"free_heap\":%lu,\"min_free_heap\":%lu,\"max_alloc_heap\":%lu,",
            (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(), (unsigned long)ESP.getMaxAllocHeap());
  r->printf("\"wifi_ssid\":\"%s\",\"wifi_rssi\":%d,\"wifi_reconnects\":%lu,\"mcu_temp\":%.1f,",
            wifi_ssids[wifi_net], WiFi.RSSI(), (unsigned long)wifi_reconnects, board.getMCUTemperature());
  r->printf("\"radio\":{\"recv\":%lu,\"sent\":%lu,\"recv_errors\":%lu,\"noise_floor\":%d,\"last_rssi\":%.1f,\"last_snr\":%.2f}}",
            (unsigned long)radio_driver.getPacketsRecv(), (unsigned long)radio_driver.getPacketsSent(),
            (unsigned long)radio_driver.getPacketsRecvErrors(), radio_driver.getNoiseFloor(),
            radio_driver.getLastRSSI(), radio_driver.getLastSNR());
  request->send(r);
}

// /rssi?since=<uptime_s>: samples newer than that uptime (all if no parameter), oldest first
// JSON: {"interval_s":60,"total":N,"samples":[[uptime_s,rssi_dbm,net,reconnects],...]}, rssi 0 = not connected
static void handleRssi(AsyncWebServerRequest *request) {
  uint32_t since = request->hasParam("since") ? strtoul(request->getParam("since")->value().c_str(), NULL, 10) : 0;
  uint32_t first = rssi_total > RSSI_SAMPLES ? rssi_total - RSSI_SAMPLES : 0;
  AsyncResponseStream* r = request->beginResponseStream("application/json");
  r->printf("{\"interval_s\":%d,\"total\":%lu,\"samples\":[", RSSI_SAMPLE_MILLIS / 1000, (unsigned long)rssi_total);
  bool comma = false;
  for (uint32_t i = first; i < rssi_total; i++) {
    const RssiSample& s = rssi_samples[i % RSSI_SAMPLES];
    if (s.uptime_s <= since) continue;
    r->printf("%s[%lu,%d,%u,%u]", comma ? "," : "", (unsigned long)s.uptime_s, s.rssi, s.net, s.reconnects);
    comma = true;
  }
  r->print("]}");
  request->send(r);
}

void HydraBoard::setCliHandler(CliHandler handler) {
  cli_handler = handler;
}

void HydraBoard::onBootComplete() {
  inhibit_sleep = true;   // light sleep would drop the Wi-Fi connection
#ifdef HYDRA_CHANNELS
  hydraChannelsInit();
#endif

  uint8_t mac[6];
  WiFi.macAddress(mac);
  sprintf(hostname, "hydra-mc-%02x%02x%02x", mac[3], mac[4], mac[5]);

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname);
  // as in Meshtastic (WiFiAPClient.cpp): reconnects are handled only by the watchdog in loop(), and the
  // radio power saving is off (with the default modem sleep the board associated but got no DHCP answer)
  WiFi.setAutoReconnect(false);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.printf("WiFi lost connection. Reason: %d\n", info.wifi_sta_disconnected.reason);
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.begin(wifi_ssids[wifi_net], wifi_pwds[wifi_net]);
}

void HydraBoard::loop() {
  if (cli_state == 1) {
    cli_reply[0] = 0;
    if (cli_handler) cli_handler(cli_command, cli_reply);
    else strcpy(cli_reply, "CLI not available");
    if (cli_reply[0] == 0) strcpy(cli_reply, "OK");
    cli_state = 2;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifi_lost_at = 0;
  } else if (wifi_lost_at == 0) {
    wifi_lost_at = millis();
  } else if (millis() - wifi_lost_at > WIFI_RETRY_MILLIS) {
    wifi_net = (wifi_net + 1) % (sizeof(wifi_ssids) / sizeof(wifi_ssids[0]));
    WiFi.disconnect(false, true);   // clear the old credentials first, as Meshtastic does
    WiFi.begin(wifi_ssids[wifi_net], wifi_pwds[wifi_net]);
    wifi_reconnects++;
    wifi_lost_at = millis();
  }

  if (rssi_total == 0 || millis() - rssi_last_sample >= RSSI_SAMPLE_MILLIS) sampleRssi();

  // system clock from NTP (the ESP32 clock restarts from a fixed date after each reset, and MeshCore
  // nodes ignore adverts older than the last one they saw); ESP32RTCClock reads the system clock
  if (!ntp_started && WiFi.status() == WL_CONNECTED) {
    configTime(0, 0, "pool.ntp.org", "time.google.com");
    ntp_started = true;
  }

  // start the web server once, after the first Wi-Fi connection (it keeps serving after reconnects)
  if (ota_server == NULL && WiFi.status() == WL_CONNECTED) {
    ota_server = new AsyncWebServer(80);
    ota_server->on("/version", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/plain", FIRMWARE_VERSION);
    });
#ifdef HYDRA_CHANNELS
    ota_server->on("/channels", HTTP_GET, [](AsyncWebServerRequest *request) {
      AsyncResponseStream* response = request->beginResponseStream("application/json");
      hydraChannelsJson(*response);
      request->send(response);
    });
    ota_server->on("/grp", HTTP_GET, [](AsyncWebServerRequest *request) {
      AsyncResponseStream* response = request->beginResponseStream("application/json");
      hydraRawJson(*response);
      request->send(response);
    });
    ota_server->on("/kanal", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/html", CHANNELS_PAGE);
    });
#endif
    ota_server->on("/cli", HTTP_ANY, handleCli);
    ota_server->on("/stats", HTTP_GET, handleStats);
    ota_server->on("/rssi", HTTP_GET, handleRssi);
    ota_server->on("/log", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (SPIFFS.exists("/packet_log")) request->send(SPIFFS, "/packet_log", "text/plain");
      else request->send(200, "text/plain", "(no packet log, use: log start)");
    });
#ifdef HYDRA_NODE_LIST
    ota_server->on("/nodes", HTTP_GET, [](AsyncWebServerRequest *request) {
      AsyncResponseStream* response = request->beginResponseStream("application/json");
      hydraNodesJson(*response);
      request->send(response);
    });
    ota_server->on("/paths", HTTP_GET, [](AsyncWebServerRequest *request) {
      AsyncResponseStream* response = request->beginResponseStream("application/json");
      hydraPathsJson(*response);
      request->send(response);
    });
    ota_server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(200, "text/html", NODES_PAGE);
    });
#endif
    AsyncElegantOTA.setID(hostname);
    AsyncElegantOTA.begin(ota_server, "admin", HYDRA_OTA_PASSWORD);
    ota_server->begin();
    Serial.printf("Hydra OTA: http://%s/update (%s)\n", WiFi.localIP().toString().c_str(), FIRMWARE_VERSION);
  }
}

bool HydraBoard::startOTAUpdate(const char* id, char reply[]) {
  if (WiFi.status() != WL_CONNECTED) {
    // diagnostics: connection status and whether the SSID is visible (scan blocks for ~2 s)
    int status = WiFi.status();
    int found = WiFi.scanNetworks(), rssi = 0, channel = 0;
    for (int i = 0; i < found; i++) {
      if (WiFi.SSID(i) == wifi_ssids[wifi_net]) { rssi = WiFi.RSSI(i); channel = WiFi.channel(i); break; }
    }
    WiFi.scanDelete();
    if (rssi != 0) {
      sprintf(reply, "Wi-Fi not connected, status %d, SSID seen %d dBm ch %d", status, rssi, channel);
    } else {
      sprintf(reply, "Wi-Fi not connected, status %d, SSID not seen (%d networks)", status, found);
    }
    return true;
  }
  sprintf(reply, "OTA: http://%s/update (%s)", WiFi.localIP().toString().c_str(), wifi_ssids[wifi_net]);
  return true;
}

#endif

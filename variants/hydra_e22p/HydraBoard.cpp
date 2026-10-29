#include "HydraBoard.h"

#ifdef HYDRA_WIFI_OTA

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

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
<p>Skoki 0 = słyszany bezpośrednio. Dla skoków &gt; 0 SNR i RSSI dotyczą ostatniego repeatera (kolumna „przez”).</p>
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
#ifdef HYDRA_NODE_LIST
    ota_server->on("/nodes", HTTP_GET, [](AsyncWebServerRequest *request) {
      AsyncResponseStream* response = request->beginResponseStream("application/json");
      hydraNodesJson(*response);
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
      if (WiFi.SSID(i) == WIFI_SSID) { rssi = WiFi.RSSI(i); channel = WiFi.channel(i); break; }
    }
    WiFi.scanDelete();
    if (rssi != 0) {
      sprintf(reply, "Wi-Fi not connected, status %d, SSID seen %d dBm ch %d", status, rssi, channel);
    } else {
      sprintf(reply, "Wi-Fi not connected, status %d, SSID not seen (%d networks)", status, found);
    }
    return true;
  }
  sprintf(reply, "OTA: http://%s/update", WiFi.localIP().toString().c_str());
  return true;
}

#endif

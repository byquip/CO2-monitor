#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "config.h"
#include "data.h"
#include "prototypes.h"

extern DeviceConfig cfg;
extern char deviceId[];

static DNSServer dnsServer;
static bool captiveMode = false;
static uint32_t lastReconnect = 0;

extern WebServer webServer;

static const char PORTAL_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CO2 Monitor Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:16px;max-width:420px;margin:0 auto}
h1{color:#00b894;margin-bottom:4px;font-size:1.4em}
.sub{color:#888;margin-bottom:16px;font-size:.85em}
.card{background:#16213e;border-radius:12px;padding:16px;margin-bottom:12px}
label{display:block;margin-bottom:4px;font-size:.9em;color:#aaa}
input[type=password],input[type=text]{width:100%;padding:10px;border:1px solid #333;border-radius:8px;background:#0f3460;color:#fff;font-size:1em;margin-bottom:8px}
button{width:100%;padding:12px;background:#00b894;color:#fff;border:none;border-radius:8px;font-size:1em;cursor:pointer;margin-top:8px}
button:hover{background:#00a884}
button:disabled{background:#555;cursor:wait}
.net{padding:10px;border-bottom:1px solid #1a1a2e;cursor:pointer;display:flex;justify-content:space-between}
.net:hover{background:#0f3460;border-radius:6px}
.signal{font-size:.8em;color:#888}
.ok{color:#4caf50;text-align:center;padding:20px;font-size:1.1em}
.err{color:#e94560;text-align:center;padding:10px;font-size:.9em}
#loader{display:none;text-align:center;padding:20px}
.spin{border:3px solid #333;border-top:3px solid #00b894;border-radius:50%;width:30px;height:30px;animation:spin 1s linear infinite;margin:0 auto 10px}
@keyframes spin{to{transform:rotate(360deg)}}
</style></head><body>
<h1>CO2 Monitor</h1>
<div class="sub">Wi-Fi Setup</div>
<div class="card" id="scanCard">
<div id="nets"></div>
<button onclick="scan()">Scan Networks</button>
</div>
<div class="card" id="connCard" style="display:none">
<div id="selNet" style="font-weight:bold;margin-bottom:8px"></div>
<label>Password:</label>
<input type="password" id="pass">
<button onclick="connect()">Connect</button>
<button onclick="back()" style="background:#333;margin-top:6px">Back</button>
</div>
<div id="loader"><div class="spin"></div><span id="loadTxt">Connecting...</span></div>
<div id="result"></div>
<script>
var selSSID='';
function scan(){
  fetch('/scan').then(r=>r.json()).then(d=>{
    var h='';
    d.forEach(n=>{h+='<div class="net" onclick="sel(\''+n.s.replace(/'/g,"\\'")+'\')">'
      +'<span>'+n.s+(n.e?' 🔒':'')+'</span><span class="signal">'+n.r+' dBm</span></div>';});
    document.getElementById('nets').innerHTML=h||'<div class="err">No networks</div>';
  });
}
function sel(s){selSSID=s;document.getElementById('selNet').textContent=s;
  document.getElementById('scanCard').style.display='none';
  document.getElementById('connCard').style.display='block';
  document.getElementById('pass').focus();}
function back(){document.getElementById('scanCard').style.display='block';
  document.getElementById('connCard').style.display='none';}
function connect(){
  var p=document.getElementById('pass').value;
  document.getElementById('connCard').style.display='none';
  document.getElementById('loader').style.display='block';
  fetch('/connect',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'ssid='+encodeURIComponent(selSSID)+'&pass='+encodeURIComponent(p)})
  .then(r=>r.json()).then(d=>{
    document.getElementById('loader').style.display='none';
    if(d.ok)document.getElementById('result').innerHTML='<div class="ok">Connected! IP: '+d.ip+'<br>Rebooting...</div>';
    else{document.getElementById('result').innerHTML='<div class="err">Failed. Try again.</div>';
      setTimeout(()=>{document.getElementById('result').innerHTML='';
        document.getElementById('scanCard').style.display='block';},3000);}
  }).catch(()=>{document.getElementById('loader').style.display='none';
    document.getElementById('result').innerHTML='<div class="ok">Rebooting...</div>';});
}
scan();
</script></body></html>)rawliteral";

void startCaptivePortal() {
  DBGLN("Starting captive portal");
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  delay(100);

  char apName[24];
  snprintf(apName, sizeof(apName), "CO2Monitor-%s", deviceId);
  WiFi.softAP(apName, "12345678");

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", WiFi.softAPIP());

  webServer.on("/", HTTP_GET, []() {
    webServer.send_P(200, "text/html", PORTAL_HTML);
  });
  webServer.on("/scan", HTTP_GET, []() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      json += "{\"s\":\"" + WiFi.SSID(i) + "\",\"r\":" + WiFi.RSSI(i) +
              ",\"e\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
    }
    json += "]";
    WiFi.scanDelete();
    webServer.send(200, "application/json", json);
  });
  webServer.on("/connect", HTTP_POST, []() {
    String ssid = webServer.arg("ssid");
    String pass = webServer.arg("pass");
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    uint32_t tmr = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - tmr < WIFI_CONNECT_TIMEOUT) {
      delay(200);
      yield();
    }
    if (WiFi.status() == WL_CONNECTED) {
      strncpy(cfg.ssid, ssid.c_str(), 31); cfg.ssid[31] = 0;
      strncpy(cfg.pass, pass.c_str(), 31); cfg.pass[31] = 0;
      cfg.wifiMode = 1;
      configSave();
      webServer.send(200, "application/json",
        "{\"ok\":true,\"ip\":\"" + WiFi.localIP().toString() + "\"}");
      delay(2000);
      ESP.restart();
    } else {
      WiFi.disconnect();
      WiFi.mode(WIFI_AP);
      char apName[24];
      snprintf(apName, sizeof(apName), "CO2Monitor-%s", deviceId);
      WiFi.softAP(apName, "12345678");
      webServer.send(200, "application/json", "{\"ok\":false}");
    }
  });
  webServer.on("/generate_204", HTTP_GET, []() {
    webServer.send_P(200, "text/html", PORTAL_HTML);
  });
  webServer.onNotFound([]() {
    webServer.send_P(200, "text/html", PORTAL_HTML);
  });
  webServer.begin();

  captiveMode = true;
  DBGF("AP: CO2Monitor-%s  IP: %s\n", deviceId, WiFi.softAPIP().toString().c_str());
}

void wifiSetup() {
  if (cfg.ssid[0] == 0) {
    startCaptivePortal();
    return;
  }

  DBGF("Connecting to %s", cfg.ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid, cfg.pass);

  uint32_t tmr = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - tmr < WIFI_CONNECT_TIMEOUT) {
    delay(200);
    DBG(".");
  }
  DBGLN();

  if (WiFi.status() == WL_CONNECTED) {
    DBGF("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    DBGLN("WiFi failed, starting AP");
    startCaptivePortal();
  }
}

void wifiLoop() {
  if (captiveMode) {
    dnsServer.processNextRequest();
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastReconnect > 120000UL) {
      lastReconnect = millis();
      DBGLN("WiFi reconnecting...");
      WiFi.reconnect();
    }
  }
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED && !captiveMode;
}

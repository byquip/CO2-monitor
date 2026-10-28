#include <WebServer.h>
#include <Update.h>
#include <WiFi.h>
#include <Wire.h>
#include "config.h"
#include "data.h"
#include "prototypes.h"

extern DeviceConfig cfg;
extern SensorData   sensor;
extern WeatherData  weather;
extern char         deviceId[];

WebServer webServer(80);

// forward declarations from mqtt.cpp
extern String getMqttLog();
extern bool   mqttIsConnected();

static bool checkAuth() {
  if (cfg.webPass[0] == 0) return true;
  if (!webServer.authenticate(cfg.webUser, cfg.webPass)) {
    webServer.requestAuthentication();
    return false;
  }
  return true;
}

// ==================== STATUS PAGE ====================
static const char STATUS_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CO2 Monitor</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:16px;max-width:480px;margin:0 auto}
h1{color:#00b894;margin-bottom:4px;font-size:1.4em}
.sub{color:#888;margin-bottom:16px;font-size:.85em}
.card{background:#16213e;border-radius:12px;padding:16px;margin-bottom:12px}
.big{font-size:3em;font-weight:bold;text-align:center;padding:12px 0}
.co2-good{color:#00b894}.co2-mod{color:#fdcb6e}.co2-poor{color:#e17055}.co2-bad{color:#d63031}
.info{display:flex;justify-content:space-between;margin-bottom:6px;font-size:.9em}
.info .val{color:#00b894;font-weight:bold}
a.btn{display:block;width:100%;padding:12px;background:#00b894;color:#fff;border:none;border-radius:8px;font-size:1em;text-align:center;text-decoration:none;margin-top:8px}
a.btn:hover{background:#00a884}
a.btn.sec{background:#0f3460}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
</style></head><body>
<h1>CO2 Monitor %ID%</h1>
<div class="sub">%STATUS%</div>
<div class="card">
<div class="big %CO2CLASS%">%CO2% ppm</div>
<div style="text-align:center;color:#aaa;font-size:.9em">%PHRASE%</div>
</div>
<div class="card grid">
<div><div class="info"><span>Temperature</span></div><div class="info"><span class="val">%TEMP% &deg;C</span></div></div>
<div><div class="info"><span>Humidity</span></div><div class="info"><span class="val">%HUM% %%</span></div></div>
</div>
%WEATHER%
<div class="card">
<div class="info"><span>Firmware</span><span class="val">v%VER%</span></div>
<div class="info"><span>IP</span><span class="val">%IP%</span></div>
<div class="info"><span>WiFi</span><span class="val">%RSSI% dBm</span></div>
<div class="info"><span>Free RAM</span><span class="val">%HEAP% bytes</span></div>
<div class="info"><span>Uptime</span><span class="val">%UPTIME%</span></div>
<div class="info"><span>MQTT</span><span class="val">%MQTT%</span></div>
</div>
<a class="btn" href="/settings">Settings</a>
<a class="btn sec" href="/mqtt">MQTT / Home Assistant</a>
<a class="btn sec" href="/update">Firmware Update</a>
<script>setTimeout(()=>location.reload(),30000)</script>
</body></html>)rawliteral";

static void handleStatus() {
  if (!checkAuth()) return;
  String page = FPSTR(STATUS_HTML);
  page.replace("%ID%", deviceId);
  page.replace("%VER%", FW_VERSION);
  page.replace("%IP%", WiFi.localIP().toString());
  page.replace("%RSSI%", String(WiFi.RSSI()));
  page.replace("%HEAP%", String(ESP.getFreeHeap()));
  page.replace("%UPTIME%", getUptime());
  page.replace("%MQTT%", cfg.mqttEnabled ? (mqttIsConnected() ? "Connected" : "Disconnected") : "Disabled");

  page.replace("%CO2%", String(sensor.co2));
  page.replace("%TEMP%", String(sensor.temperature, 1));
  page.replace("%HUM%", String(sensor.humidity, 1));
  page.replace("%PHRASE%", sensor.valid ? get_co2_phrase(sensor.co2) : "Waiting for sensor...");
  page.replace("%STATUS%", sensor.valid ? get_co2_status(sensor.co2) : "Initializing");

  const char* cls = "co2-good";
  if (sensor.co2 >= CO2_BAD) cls = "co2-bad";
  else if (sensor.co2 >= CO2_POOR) cls = "co2-poor";
  else if (sensor.co2 >= CO2_MODERATE) cls = "co2-mod";
  page.replace("%CO2CLASS%", cls);

  if (weather.valid) {
    String w = "<div class=\"card grid\">"
      "<div><div class=\"info\"><span>Outdoor</span></div>"
      "<div class=\"info\"><span class=\"val\">" + String(weather.tempOutdoor, 1) + " &deg;C</span></div></div>"
      "<div><div class=\"info\"><span>" + String(weather.description) + "</span></div>"
      "<div class=\"info\"><span class=\"val\">" + String(weather.humidityOut, 0) + " %%</span></div></div>"
      "</div>";
    page.replace("%WEATHER%", w);
  } else {
    page.replace("%WEATHER%", "");
  }

  webServer.send(200, "text/html", page);
}

// ==================== SETTINGS PAGE ====================
static const char SETTINGS_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CO2 Monitor Settings</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:16px;max-width:480px;margin:0 auto}
h1{color:#00b894;margin-bottom:4px;font-size:1.4em}
.sub{color:#888;margin-bottom:16px;font-size:.85em}
.card{background:#16213e;border-radius:12px;padding:16px;margin-bottom:12px}
label{display:block;margin-bottom:4px;font-size:.9em;color:#aaa}
input[type=text],input[type=password],input[type=number],select{width:100%;padding:10px;border:1px solid #333;border-radius:8px;background:#0f3460;color:#fff;font-size:1em;margin-bottom:12px}
button{width:100%;padding:12px;background:#00b894;color:#fff;border:none;border-radius:8px;font-size:1em;cursor:pointer;margin-top:4px}
button:hover{background:#00a884}
.toggle{display:flex;align-items:center;gap:10px;margin-bottom:12px}
.sw{position:relative;width:48px;height:26px;cursor:pointer}
.sw input{opacity:0;width:0;height:0}
.sl{position:absolute;top:0;left:0;right:0;bottom:0;background:#333;border-radius:13px;transition:.3s}
.sl:before{content:"";position:absolute;height:20px;width:20px;left:3px;bottom:3px;background:#888;border-radius:50%;transition:.3s}
.sw input:checked+.sl{background:#00b894}
.sw input:checked+.sl:before{transform:translateX(22px);background:#fff}
a{color:#00b894;text-decoration:none}
</style></head><body>
<h1>Settings</h1>
<div class="sub">CO2 Monitor Configuration</div>
<form method="POST" action="/settings">
<div class="card">
<h2 style="font-size:1.1em;margin-bottom:12px;color:#00b894">Device</h2>
<label>Device Name</label>
<input type="text" name="devname" value="%DEVNAME%" maxlength="23">
<label>Timezone (UTC offset hours)</label>
<input type="number" name="tz" value="%TZ%" min="-12" max="14">
<label>NTP Server</label>
<input type="text" name="ntp" value="%NTP%" maxlength="31">
</div>
<div class="card">
<h2 style="font-size:1.1em;margin-bottom:12px;color:#00b894">Weather (OpenWeatherMap)</h2>
<div class="toggle"><label class="sw"><input type="checkbox" name="owm" value="1" %OWMEN%><span class="sl"></span></label><span>Enable Weather</span></div>
<label>API Key</label>
<input type="text" name="owmkey" value="%OWMKEY%" maxlength="47">
<label>City</label>
<input type="text" name="owmcity" value="%OWMCITY%" maxlength="31">
<label>Latitude</label>
<input type="text" name="lat" value="%LAT%" maxlength="11">
<label>Longitude</label>
<input type="text" name="lon" value="%LON%" maxlength="11">
</div>
<div class="card">
<h2 style="font-size:1.1em;margin-bottom:12px;color:#00b894">Web Auth</h2>
<label>Username</label>
<input type="text" name="webuser" value="%WEBUSER%" maxlength="15">
<label>Password (empty = no auth)</label>
<input type="password" name="webpass" placeholder="********" maxlength="15">
</div>
<div class="card">
<h2 style="font-size:1.1em;margin-bottom:12px;color:#00b894">Display</h2>
<label>Theme</label>
<select name="theme"><option value="0" %TH0%>Full (all info + graph)</option><option value="1" %TH1%>Big Number</option><option value="2" %TH2%>Minimal</option></select>
<label>CO2 Alert Level</label>
<select name="alertlvl"><option value="0" %AL0%>Off</option><option value="1" %AL1%>Moderate (1000+)</option><option value="2" %AL2%>Poor (1400+)</option><option value="3" %AL3%>Bad (2000+)</option></select>
</div>
<div class="card">
<h2 style="font-size:1.1em;margin-bottom:12px;color:#00b894">LED Night Mode</h2>
<div class="toggle"><label class="sw"><input type="checkbox" name="lednight" value="1" %LEDNIGHTEN%><span class="sl"></span></label><span>Turn off LED at night</span></div>
<label>Off at (hour, 0-23)</label>
<input type="number" name="ledoff" value="%LEDOFF%" min="0" max="23">
<label>On at (hour, 0-23)</label>
<input type="number" name="ledon" value="%LEDON%" min="0" max="23">
</div>
<div class="card">
<h2 style="font-size:1.1em;margin-bottom:12px;color:#00b894">SCD41 Calibration</h2>
<p style="font-size:.82em;color:#aaa;margin-bottom:10px">Place sensor in fresh air (~420 ppm) or known CO2 concentration for 3+ minutes, then calibrate.</p>
<label>Reference CO2 (ppm)</label>
<input type="number" name="calref" value="%CALREF%" min="400" max="2000">
<button type="button" onclick="if(confirm('Start forced recalibration at '+document.querySelector('[name=calref]').value+' ppm?'))fetch('/calibrate?ref='+document.querySelector('[name=calref]').value).then(()=>alert('Calibration done'))">Calibrate Now</button>
</div>
<button type="submit">Save</button>
</form>
<p style="text-align:center;margin-top:16px;font-size:.8em;color:#555"><a href="/">&larr; Back</a></p>
</body></html>)rawliteral";

static void handleSettings() {
  if (!checkAuth()) return;
  String page = FPSTR(SETTINGS_HTML);
  page.replace("%DEVNAME%", cfg.deviceName);
  page.replace("%TZ%", String(cfg.timezone));
  page.replace("%NTP%", cfg.ntpServer);
  page.replace("%OWMEN%", cfg.owmEnabled ? "checked" : "");
  page.replace("%OWMKEY%", cfg.owmApiKey);
  page.replace("%OWMCITY%", cfg.owmCity);
  page.replace("%LAT%", cfg.owmLat);
  page.replace("%LON%", cfg.owmLon);
  page.replace("%WEBUSER%", cfg.webUser);
  page.replace("%CALREF%", String(cfg.co2CalibrationRef));

  page.replace("%TH0%", cfg.displayTheme == 0 ? "selected" : "");
  page.replace("%TH1%", cfg.displayTheme == 1 ? "selected" : "");
  page.replace("%TH2%", cfg.displayTheme == 2 ? "selected" : "");

  page.replace("%AL0%", cfg.co2AlertLevel == 0 ? "selected" : "");
  page.replace("%AL1%", cfg.co2AlertLevel == 1 ? "selected" : "");
  page.replace("%AL2%", cfg.co2AlertLevel == 2 ? "selected" : "");
  page.replace("%AL3%", cfg.co2AlertLevel == 3 ? "selected" : "");

  page.replace("%LEDNIGHTEN%", cfg.ledNightMode ? "checked" : "");
  page.replace("%LEDOFF%", String(cfg.ledOffHour));
  page.replace("%LEDON%", String(cfg.ledOnHour));

  webServer.send(200, "text/html", page);
}

static void handleSettingsSave() {
  if (!checkAuth()) return;
  if (webServer.hasArg("devname")) {
    strncpy(cfg.deviceName, webServer.arg("devname").c_str(), 23);
    cfg.deviceName[23] = 0;
  }
  if (webServer.hasArg("tz")) cfg.timezone = webServer.arg("tz").toInt();
  if (webServer.hasArg("ntp")) {
    strncpy(cfg.ntpServer, webServer.arg("ntp").c_str(), 31);
    cfg.ntpServer[31] = 0;
  }
  cfg.owmEnabled = webServer.hasArg("owm");
  if (webServer.hasArg("owmkey")) {
    strncpy(cfg.owmApiKey, webServer.arg("owmkey").c_str(), 47);
    cfg.owmApiKey[47] = 0;
  }
  if (webServer.hasArg("owmcity")) {
    strncpy(cfg.owmCity, webServer.arg("owmcity").c_str(), 31);
    cfg.owmCity[31] = 0;
  }
  if (webServer.hasArg("lat")) {
    strncpy(cfg.owmLat, webServer.arg("lat").c_str(), 11);
    cfg.owmLat[11] = 0;
  }
  if (webServer.hasArg("lon")) {
    strncpy(cfg.owmLon, webServer.arg("lon").c_str(), 11);
    cfg.owmLon[11] = 0;
  }
  if (webServer.hasArg("webuser")) {
    strncpy(cfg.webUser, webServer.arg("webuser").c_str(), 15);
    cfg.webUser[15] = 0;
  }
  if (webServer.hasArg("webpass") && webServer.arg("webpass").length() > 0) {
    strncpy(cfg.webPass, webServer.arg("webpass").c_str(), 15);
    cfg.webPass[15] = 0;
  }
  if (webServer.hasArg("theme")) {
    cfg.displayTheme = constrain(webServer.arg("theme").toInt(), 0, 2);
  }
  if (webServer.hasArg("alertlvl")) {
    cfg.co2AlertLevel = constrain(webServer.arg("alertlvl").toInt(), 0, 3);
  }
  if (webServer.hasArg("calref")) {
    cfg.co2CalibrationRef = constrain(webServer.arg("calref").toInt(), 400, 2000);
  }
  cfg.ledNightMode = webServer.hasArg("lednight") ? 1 : 0;
  if (webServer.hasArg("ledoff")) {
    cfg.ledOffHour = constrain(webServer.arg("ledoff").toInt(), 0, 23);
  }
  if (webServer.hasArg("ledon")) {
    cfg.ledOnHour = constrain(webServer.arg("ledon").toInt(), 0, 23);
  }
  configSave();
  displayUpdate(true);
  webServer.sendHeader("Location", "/settings");
  webServer.send(302, "text/plain", "Saved");
}

static void handleCalibrate() {
  if (!checkAuth()) return;
  if (!webServer.hasArg("ref")) {
    webServer.send(400, "text/plain", "Missing ref parameter");
    return;
  }
  uint16_t ref = constrain(webServer.arg("ref").toInt(), 400, 2000);
  sensorForceCalibration(ref);
  cfg.co2CalibrationRef = ref;
  configSave();
  webServer.send(200, "text/plain", "Calibration done");
}

// ==================== MQTT PAGE ====================
static const char MQTT_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CO2 Monitor MQTT</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:16px;max-width:480px;margin:0 auto}
h1{color:#00b894;margin-bottom:4px;font-size:1.4em}
.sub{color:#888;margin-bottom:16px;font-size:.85em}
.card{background:#16213e;border-radius:12px;padding:16px;margin-bottom:12px}
label{display:block;margin-bottom:4px;font-size:.9em;color:#aaa}
input[type=text],input[type=password],input[type=number]{width:100%;padding:10px;border:1px solid #333;border-radius:8px;background:#0f3460;color:#fff;font-size:1em;margin-bottom:12px}
button{width:100%;padding:12px;background:#00b894;color:#fff;border:none;border-radius:8px;font-size:1em;cursor:pointer;margin-top:4px}
.toggle{display:flex;align-items:center;gap:10px;margin-bottom:12px}
.sw{position:relative;width:48px;height:26px;cursor:pointer}
.sw input{opacity:0;width:0;height:0}
.sl{position:absolute;top:0;left:0;right:0;bottom:0;background:#333;border-radius:13px;transition:.3s}
.sl:before{content:"";position:absolute;height:20px;width:20px;left:3px;bottom:3px;background:#888;border-radius:50%;transition:.3s}
.sw input:checked+.sl{background:#00b894}
.sw input:checked+.sl:before{transform:translateX(22px);background:#fff}
.status{text-align:center;padding:8px;border-radius:8px;margin-bottom:12px;font-size:.9em}
.st-on{background:#1b4332;color:#4caf50}
.st-off{background:#3d1f1f;color:#e94560}
.st-dis{background:#1a1a2e;color:#555}
a{color:#00b894;text-decoration:none}
</style></head><body>
<h1>MQTT / Home Assistant</h1>
<div class="sub">CO2 Monitor</div>
<div class="status %MQTTCLASS%">%MQTTSTATUS%</div>
<form method="POST" action="/mqtt">
<div class="card">
<div class="toggle"><label class="sw"><input type="checkbox" name="enabled" value="1" %MQTTEN%><span class="sl"></span></label><span>MQTT Enabled</span></div>
<label>Broker (IP or hostname)</label>
<input type="text" name="host" value="%MQTTHOST%" maxlength="31">
<label>Port</label>
<input type="number" name="port" value="%MQTTPORT%" min="1" max="65535">
<label>Username</label>
<input type="text" name="login" value="%MQTTLOGIN%" maxlength="15">
<label>Password</label>
<input type="password" name="pass" placeholder="********" maxlength="15">
<div class="toggle"><label class="sw"><input type="checkbox" name="autodisc" value="1" %AUTODISCEN%><span class="sl"></span></label><span>HA Auto-Discovery</span></div>
<button type="submit">Save</button>
</div>
</form>
<div class="card">
<div style="font-size:.9em;color:#aaa;margin-bottom:8px">MQTT Log</div>
<pre id="log" style="background:#0a0e1a;border-radius:8px;padding:10px;font-size:.78em;color:#8f8;min-height:60px;max-height:200px;overflow-y:auto;white-space:pre-wrap;font-family:monospace"></pre>
</div>
<p style="text-align:center;margin-top:16px;font-size:.8em;color:#555"><a href="/">&larr; Back</a></p>
<script>
function fl(){fetch('/mqtt/log').then(r=>r.text()).then(d=>{
var b=document.getElementById('log');b.textContent=d||'(empty)';b.scrollTop=b.scrollHeight;});}
fl();setInterval(fl,3000);
</script></body></html>)rawliteral";

static void handleMqtt() {
  if (!checkAuth()) return;
  String page = FPSTR(MQTT_HTML);
  page.replace("%MQTTHOST%", cfg.mqttHost);
  page.replace("%MQTTPORT%", String(cfg.mqttPort > 0 ? cfg.mqttPort : MQTT_PORT_DEFAULT));
  page.replace("%MQTTLOGIN%", cfg.mqttLogin);
  page.replace("%MQTTEN%", cfg.mqttEnabled ? "checked" : "");
  page.replace("%AUTODISCEN%", cfg.mqttAutoDisc ? "checked" : "");

  if (cfg.mqttEnabled) {
    page.replace("%MQTTCLASS%", mqttIsConnected() ? "st-on" : "st-off");
    page.replace("%MQTTSTATUS%", mqttIsConnected() ? "Connected" : "Disconnected");
  } else {
    page.replace("%MQTTCLASS%", "st-dis");
    page.replace("%MQTTSTATUS%", "Disabled");
  }
  webServer.send(200, "text/html", page);
}

static void handleMqttSave() {
  if (!checkAuth()) return;
  cfg.mqttEnabled = webServer.hasArg("enabled");
  if (webServer.hasArg("host")) {
    strncpy(cfg.mqttHost, webServer.arg("host").c_str(), 31);
    cfg.mqttHost[31] = 0;
  }
  if (webServer.hasArg("port")) cfg.mqttPort = webServer.arg("port").toInt();
  if (webServer.hasArg("login")) {
    strncpy(cfg.mqttLogin, webServer.arg("login").c_str(), 15);
    cfg.mqttLogin[15] = 0;
  }
  if (webServer.hasArg("pass") && webServer.arg("pass").length() > 0) {
    strncpy(cfg.mqttPass, webServer.arg("pass").c_str(), 15);
    cfg.mqttPass[15] = 0;
  }
  cfg.mqttAutoDisc = webServer.hasArg("autodisc");
  configSave();
  mqttSetup();
  webServer.sendHeader("Location", "/mqtt");
  webServer.send(302, "text/plain", "Saved");
}

static void handleMqttLog() {
  if (!checkAuth()) return;
  webServer.send(200, "text/plain", getMqttLog());
}

// ==================== OTA UPDATE ====================
static const char OTA_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>CO2 Monitor Update</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:16px;max-width:480px;margin:0 auto}
h1{color:#00b894;margin-bottom:16px;font-size:1.4em}
.card{background:#16213e;border-radius:12px;padding:16px;margin-bottom:12px}
.info{display:flex;justify-content:space-between;margin-bottom:6px;font-size:.9em}
.info .val{color:#00b894}
input[type=file]{display:none}
.upload-label{display:block;width:100%;padding:12px;background:#0f3460;color:#aaa;border:2px dashed #333;border-radius:8px;text-align:center;cursor:pointer;margin-bottom:8px}
.upload-label:hover{border-color:#00b894;color:#fff}
.upload-label.ok{border-color:#4caf50;color:#4caf50}
button{width:100%;padding:12px;background:#00b894;color:#fff;border:none;border-radius:8px;font-size:1em;cursor:pointer}
button:disabled{background:#555}
.progress{width:100%;height:24px;background:#0f3460;border-radius:12px;overflow:hidden;margin:12px 0;display:none}
.bar{height:100%;background:linear-gradient(90deg,#00b894,#4caf50);width:0%;transition:width .3s;border-radius:12px;text-align:center;line-height:24px;font-size:.8em;color:#fff}
.msg{text-align:center;padding:16px;font-size:1.1em}
a{color:#00b894;text-decoration:none}
</style></head><body>
<h1>Firmware Update</h1>
<div class="card">
<div class="info"><span>Version</span><span class="val">v%VER%</span></div>
<div class="info"><span>Free Heap</span><span class="val">%HEAP% bytes</span></div>
</div>
<div class="card" id="uc">
<label class="upload-label" id="fl" for="fw">Select .bin file</label>
<input type="file" id="fw" accept=".bin" onchange="fs(this)">
<div class="progress" id="pg"><div class="bar" id="pb">0%</div></div>
<button onclick="up()" id="ub" disabled>Upload</button>
</div>
<div id="r"></div>
<p style="text-align:center;margin-top:16px;font-size:.8em;color:#555"><a href="/">&larr; Back</a></p>
<script>
var f=null;
function fs(i){if(i.files.length>0){f=i.files[0];
  document.getElementById('fl').classList.add('ok');
  document.getElementById('fl').textContent=f.name+' ('+Math.round(f.size/1024)+' KB)';
  document.getElementById('ub').disabled=false;}}
function up(){if(!f)return;var d=new FormData();d.append('update',f);
  var x=new XMLHttpRequest();x.open('POST','/update',true);
  document.getElementById('ub').disabled=true;document.getElementById('ub').textContent='Uploading...';
  document.getElementById('pg').style.display='block';
  x.upload.onprogress=function(e){if(e.lengthComputable){var p=Math.round(e.loaded/e.total*100);
    document.getElementById('pb').style.width=p+'%';document.getElementById('pb').textContent=p+'%';}};
  x.onload=function(){document.getElementById('uc').style.display='none';
    document.getElementById('r').innerHTML='<div class="msg" style="color:#4caf50">Update OK! Rebooting...</div>';};
  x.onerror=function(){document.getElementById('r').innerHTML='<div class="msg" style="color:#e94560">Update failed!</div>';};
  x.send(d);}
</script></body></html>)rawliteral";

static void handleOTA() {
  if (!checkAuth()) return;
  String page = FPSTR(OTA_HTML);
  page.replace("%VER%", FW_VERSION);
  page.replace("%HEAP%", String(ESP.getFreeHeap()));
  webServer.send(200, "text/html", page);
}

// ==================== DEBUG PAGE ====================
static void handleDebug() {
  if (!checkAuth()) return;

  char buf[256];
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "text/html", "");

  webServer.sendContent(F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>CO2 Debug</title><style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:monospace;background:#1a1a2e;color:#e0e0e0;padding:16px;max-width:480px;margin:0 auto}"
    "h1{color:#00b894;margin-bottom:16px;font-size:1.2em}"
    ".card{background:#16213e;border-radius:12px;padding:16px;margin-bottom:12px}"
    ".ok{color:#4caf50}.err{color:#e94560}"
    "pre{white-space:pre-wrap;font-size:.85em;line-height:1.6}"
    "a{color:#00b894}</style></head><body>"
    "<h1>Debug / I2C Scanner</h1>"));

  snprintf(buf, sizeof(buf),
    "<div class='card'><b>Pins</b><pre>"
    "I2C SDA: GPIO %d\nI2C SCL: GPIO %d\n"
    "LED Red: GPIO %d\nLED Green: GPIO %d\n"
    "</pre></div>",
    PIN_I2C_SDA, PIN_I2C_SCL, PIN_LED_RED, PIN_LED_GREEN);
  webServer.sendContent(buf);

  webServer.sendContent(F("<div class='card'><b>I2C Bus Scan</b><pre>"));
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      snprintf(buf, sizeof(buf), "<span class='ok'>0x%02X — FOUND%s</span>\n",
               addr, (addr == 0x62) ? " (SCD41)" : "");
      webServer.sendContent(buf);
      found++;
    }
    yield();
  }
  if (found == 0) {
    webServer.sendContent(F("<span class='err'>No devices found!</span>\n"));
  }
  snprintf(buf, sizeof(buf), "Total: %d device(s)\n</pre></div>", found);
  webServer.sendContent(buf);

  const char* stateNames[] = {"idle", "polling", "read"};
  uint8_t st = sensorGetState();
  snprintf(buf, sizeof(buf),
    "<div class='card'><b>Sensor Init</b><pre>"
    "init: %s (attempt %d)\n"
    "ready: %s\nvalid: %s\nstate: %d (%s)\n</pre></div>",
    sensorGetInitStatus(), sensorGetInitAttempts(),
    sensorIsReady() ? "YES" : "NO",
    sensor.valid ? "true" : "false",
    st, st < 3 ? stateNames[st] : "?");
  webServer.sendContent(buf);

  snprintf(buf, sizeof(buf),
    "<div class='card'><b>Sensor Reads</b><pre>"
    "attempts: %d\ndataReady: %d\n"
    "co2zero: %d\nerrors: %d\n",
    sensorGetReadAttempts(), sensorGetDataReadyCount(),
    sensorGetCo2ZeroCount(), sensorGetReadErrCount());
  webServer.sendContent(buf);
  snprintf(buf, sizeof(buf),
    "lastErr: %s\n"
    "co2: %d ppm\ntemp: %.1f C\nhum: %.1f %%\n"
    "lastRead: %lu ms\nmillis: %lu ms\n"
    "</pre></div>",
    sensorGetLastReadErr(),
    sensor.co2, sensor.temperature, sensor.humidity,
    sensor.lastRead, millis());
  webServer.sendContent(buf);

  snprintf(buf, sizeof(buf),
    "<div class='card'><b>System</b><pre>"
    "heap: %lu bytes\nuptime: %s\nRSSI: %d dBm\nMQTT: %s\n"
    "</pre></div>",
    (unsigned long)ESP.getFreeHeap(), getUptime().c_str(), WiFi.RSSI(),
    cfg.mqttEnabled ? (mqttIsConnected() ? "connected" : "disconnected") : "disabled");
  webServer.sendContent(buf);

  webServer.sendContent(F("<p style='text-align:center;margin-top:16px;font-size:.8em'>"
    "<a href='/'>Back</a></p></body></html>"));
  webServer.sendContent("");
}

// ==================== JSON API ====================
static void handleApiStatus() {
  String json = "{\"co2\":" + String(sensor.co2) +
    ",\"temperature\":" + String(sensor.temperature, 1) +
    ",\"humidity\":" + String(sensor.humidity, 1) +
    ",\"co2_status\":\"" + get_co2_status(sensor.co2) + "\"" +
    ",\"uptime\":\"" + getUptime() + "\"" +
    ",\"version\":\"" FW_VERSION "\"";
  if (weather.valid) {
    json += ",\"weather\":{\"temp\":" + String(weather.tempOutdoor, 1) +
      ",\"humidity\":" + String(weather.humidityOut, 1) +
      ",\"pressure\":" + String(weather.pressure, 0) +
      ",\"desc\":\"" + weather.description + "\"}";
  }
  json += "}";
  webServer.send(200, "application/json", json);
}

// ==================== SETUP ====================
void webSetup() {
  webServer.on("/", HTTP_GET, handleStatus);
  webServer.on("/settings", HTTP_GET, handleSettings);
  webServer.on("/settings", HTTP_POST, handleSettingsSave);
  webServer.on("/mqtt", HTTP_GET, handleMqtt);
  webServer.on("/mqtt", HTTP_POST, handleMqttSave);
  webServer.on("/mqtt/log", HTTP_GET, handleMqttLog);
  webServer.on("/update", HTTP_GET, handleOTA);
  webServer.on("/calibrate", HTTP_GET, handleCalibrate);
  webServer.on("/debug", HTTP_GET, handleDebug);
  webServer.on("/api/status", HTTP_GET, handleApiStatus);

  webServer.on("/update", HTTP_POST, []() {
    bool ok = !Update.hasError();
    webServer.send(200, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
    if (ok) { delay(1000); ESP.restart(); }
  }, []() {
    if (cfg.webPass[0] != 0 && !webServer.authenticate(cfg.webUser, cfg.webPass)) return;
    HTTPUpload& upload = webServer.upload();
    if (upload.status == UPLOAD_FILE_START) {
      DBGF("OTA: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { DBGLN("OTA begin failed"); }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      Update.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { DBGLN("OTA OK"); }
      else { DBGLN("OTA end failed"); }
    }
  });

  webServer.begin();
  DBGLN("Web server started");
}

void webLoop() {
  webServer.handleClient();
}

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "data.h"
#include "prototypes.h"

extern DeviceConfig cfg;
extern SensorData   sensor;
extern WeatherData  weather;
extern char         deviceId[];

static WiFiClient   mqttWifi;
static PubSubClient mqtt(mqttWifi);

static char topicState[48];
static char topicSet[48];
static char topicAvail[48];
static char clientId[24];

static uint32_t lastReconnect = 0;

#define MQTT_LOG_SIZE 10
#define MQTT_LOG_LEN  96
static char mqttLog[MQTT_LOG_SIZE][MQTT_LOG_LEN];
static uint8_t mqttLogIdx = 0;
static uint8_t mqttLogCount = 0;

static void mqttLogAdd(const char* msg) {
  strncpy(mqttLog[mqttLogIdx], msg, MQTT_LOG_LEN - 1);
  mqttLog[mqttLogIdx][MQTT_LOG_LEN - 1] = 0;
  mqttLogIdx = (mqttLogIdx + 1) % MQTT_LOG_SIZE;
  if (mqttLogCount < MQTT_LOG_SIZE) mqttLogCount++;
}

String getMqttLog() {
  String out;
  out.reserve(MQTT_LOG_SIZE * MQTT_LOG_LEN);
  uint8_t count = mqttLogCount < MQTT_LOG_SIZE ? mqttLogCount : MQTT_LOG_SIZE;
  uint8_t start = (mqttLogIdx - count + MQTT_LOG_SIZE) % MQTT_LOG_SIZE;
  for (uint8_t i = 0; i < count; i++) {
    uint8_t idx = (start + i) % MQTT_LOG_SIZE;
    out += mqttLog[idx];
    out += '\n';
  }
  return out;
}

bool mqttIsConnected() {
  return cfg.mqttEnabled && mqtt.connected();
}

static void buildTopics() {
  snprintf(clientId,   sizeof(clientId),   "co2mon_%s", deviceId);
  snprintf(topicState, sizeof(topicState), "co2mon_%s/state", deviceId);
  snprintf(topicSet,   sizeof(topicSet),   "co2mon_%s/set", deviceId);
  snprintf(topicAvail, sizeof(topicAvail), "co2mon_%s/availability", deviceId);
}

static void handleSet(const char* param, const char* value) {
  bool changed = false;
  if (strcmp(param, "co2_alert") == 0) {
    cfg.co2AlertLevel = constrain(atoi(value), 0, 3);
    changed = true;
  } else if (strcmp(param, "display_rotation") == 0) {
    cfg.displayRotation = constrain(atoi(value), 0, 3);
    changed = true;
  } else if (strcmp(param, "display_theme") == 0) {
    cfg.displayTheme = constrain(atoi(value), 0, 2);
    changed = true;
    displayUpdate(true);
  } else if (strcmp(param, "calibrate") == 0) {
    uint16_t ref = atoi(value);
    if (ref >= 400 && ref <= 2000) {
      sensorForceCalibration(ref);
      mqttLogAdd(">> Calibration triggered");
    }
  }
  if (changed) {
    configSave();
    mqttPublishState();
  }
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char msg[128];
  uint16_t len = (length < sizeof(msg) - 1) ? length : sizeof(msg) - 1;
  memcpy(msg, payload, len);
  msg[len] = 0;

  DBGF("MQTT << %s: %s\n", topic, msg);
  char logBuf[MQTT_LOG_LEN];
  const char* shortTopic = strrchr(topic, '/');
  snprintf(logBuf, sizeof(logBuf), "<< %s: %s", shortTopic ? shortTopic + 1 : topic, msg);
  mqttLogAdd(logBuf);

  uint8_t setLen = strlen(topicSet);
  if (strncmp(topic, topicSet, setLen) == 0 && topic[setLen] == '/') {
    handleSet(topic + setLen + 1, msg);
  }
}

static bool mqttReconnect() {
  if (!cfg.mqttEnabled || cfg.mqttHost[0] == 0) return false;
  if (!wifiConnected()) return false;

  DBGLN("MQTT connecting...");
  bool ok;
  if (cfg.mqttLogin[0] != 0) {
    ok = mqtt.connect(clientId, cfg.mqttLogin, cfg.mqttPass,
                      topicAvail, 0, true, "offline");
  } else {
    ok = mqtt.connect(clientId, NULL, NULL,
                      topicAvail, 0, true, "offline");
  }

  if (ok) {
    DBGLN("MQTT connected");
    mqttLogAdd(">> Connected to broker");
    mqtt.publish(topicAvail, "online", true);
    char sub[52];
    snprintf(sub, sizeof(sub), "%s/+", topicSet);
    mqtt.subscribe(sub);
    if (cfg.mqttAutoDisc) mqttPublishDiscovery();
    mqttPublishState();
    return true;
  }

  DBGF("MQTT fail rc=%d\n", mqtt.state());
  char logBuf[MQTT_LOG_LEN];
  snprintf(logBuf, sizeof(logBuf), ">> Connect FAIL rc=%d", mqtt.state());
  mqttLogAdd(logBuf);
  return false;
}

void mqttSetup() {
  if (!cfg.mqttEnabled || cfg.mqttHost[0] == 0) return;
  buildTopics();
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  mqtt.setServer(cfg.mqttHost, cfg.mqttPort > 0 ? cfg.mqttPort : MQTT_PORT_DEFAULT);
  mqtt.setCallback(mqttCallback);
  mqtt.setKeepAlive(60);
  DBGF("MQTT configured: %s:%d\n", cfg.mqttHost,
       cfg.mqttPort > 0 ? cfg.mqttPort : MQTT_PORT_DEFAULT);
}

void mqttLoop() {
  if (!cfg.mqttEnabled || cfg.mqttHost[0] == 0) return;
  if (!mqtt.connected()) {
    if (millis() - lastReconnect > MQTT_RECONNECT_INTERVAL) {
      lastReconnect = millis();
      mqttReconnect();
    }
  } else {
    mqtt.loop();
  }
}

void mqttPublishState() {
  if (!cfg.mqttEnabled || !mqtt.connected()) return;

  JsonDocument doc;
  doc["co2"]         = sensor.co2;
  doc["temperature"] = serialized(String(sensor.temperature, 1));
  doc["humidity"]    = serialized(String(sensor.humidity, 1));
  doc["co2_status"]  = get_co2_status(sensor.co2);

  if (weather.valid) {
    doc["temp_outdoor"]  = serialized(String(weather.tempOutdoor, 1));
    doc["humidity_out"]  = serialized(String(weather.humidityOut, 1));
    doc["pressure"]      = serialized(String(weather.pressure, 0));
    doc["weather"]       = weather.description;
  }

  doc["co2_alert"]       = cfg.co2AlertLevel;
  doc["display_theme"]   = cfg.displayTheme;
  doc["ventilation"]     = (co2_index(sensor.co2) >= cfg.co2AlertLevel && cfg.co2AlertLevel > 0) ? "ON" : "OFF";
  doc["uptime"]          = getUptime();

  char buf[384];
  serializeJson(doc, buf, sizeof(buf));

  if (mqtt.publish(topicState, buf, true)) {
    mqttLogAdd(">> State published");
  } else {
    mqttLogAdd(">> State publish FAILED");
  }
}

static void mqttDiscSensor(const char* name, const char* field,
                           const char* devClass, const char* unit,
                           const char* icon) {
  char topic[80];
  char payload[480];
  snprintf(topic, sizeof(topic), "homeassistant/sensor/%s_%s/config", clientId, field);
  snprintf(payload, sizeof(payload),
    "{\"name\":\"%s\","
    "\"uniq_id\":\"%s_%s\","
    "\"stat_t\":\"%s\","
    "\"val_tpl\":\"{{value_json.%s}}\","
    "\"dev_cla\":\"%s\","
    "\"unit_of_meas\":\"%s\","
    "\"icon\":\"%s\","
    "\"avty_t\":\"%s\","
    "\"dev\":{\"ids\":\"%s\",\"name\":\"CO2 Monitor %s\","
    "\"mf\":\"DIY\",\"mdl\":\"CO2Mon-ESP32C6\",\"sw\":\"%s\"}}",
    name, clientId, field,
    topicState, field,
    devClass, unit, icon,
    topicAvail,
    clientId, deviceId, FW_VERSION
  );
  mqtt.publish(topic, payload, true);
  yield();
}

static void mqttDiscNumber(const char* name, const char* field,
                           int minV, int maxV, const char* icon) {
  char topic[80];
  char payload[480];
  snprintf(topic, sizeof(topic), "homeassistant/number/%s_%s/config", clientId, field);
  snprintf(payload, sizeof(payload),
    "{\"name\":\"%s\","
    "\"uniq_id\":\"%s_%s\","
    "\"stat_t\":\"%s\","
    "\"cmd_t\":\"%s/%s\","
    "\"val_tpl\":\"{{value_json.%s}}\","
    "\"min\":%d,\"max\":%d,"
    "\"icon\":\"%s\","
    "\"avty_t\":\"%s\","
    "\"dev\":{\"ids\":\"%s\"}}",
    name, clientId, field,
    topicState, topicSet, field,
    field, minV, maxV, icon,
    topicAvail, clientId
  );
  mqtt.publish(topic, payload, true);
  yield();
}

void mqttPublishDiscovery() {
  mqttDiscSensor("CO2",         "co2",         "carbon_dioxide", "ppm", "mdi:molecule-co2");
  mqttDiscSensor("Temperature", "temperature", "temperature",    "\302\260C", "mdi:thermometer");
  mqttDiscSensor("Humidity",    "humidity",    "humidity",       "%",   "mdi:water-percent");

  if (cfg.owmEnabled) {
    mqttDiscSensor("Outdoor Temp",     "temp_outdoor",  "temperature", "\302\260C", "mdi:thermometer");
    mqttDiscSensor("Outdoor Humidity", "humidity_out",  "humidity",    "%",   "mdi:water-percent");
    mqttDiscSensor("Pressure",         "pressure",      "pressure",    "hPa", "mdi:gauge");
  }

  mqttDiscNumber("CO2 Alert Level", "co2_alert", 0, 3, "mdi:alert");
  mqttDiscNumber("Display Theme", "display_theme", 0, 2, "mdi:palette");

  // ventilation_needed binary_sensor
  {
    char topic[80];
    char payload[480];
    snprintf(topic, sizeof(topic), "homeassistant/binary_sensor/%s_ventilation/config", clientId);
    snprintf(payload, sizeof(payload),
      "{\"name\":\"Ventilation Needed\","
      "\"uniq_id\":\"%s_ventilation\","
      "\"stat_t\":\"%s\","
      "\"val_tpl\":\"{{value_json.ventilation}}\","
      "\"dev_cla\":\"problem\","
      "\"icon\":\"mdi:air-filter\","
      "\"avty_t\":\"%s\","
      "\"dev\":{\"ids\":\"%s\"}}",
      clientId, topicState, topicAvail, clientId
    );
    mqtt.publish(topic, payload, true);
    yield();
  }

  DBGLN("MQTT HA discovery published");
  mqttLogAdd(">> Discovery published");
}

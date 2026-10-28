#pragma once

// ---- wifi.cpp ----
void wifiSetup();
void wifiLoop();
void startCaptivePortal();
bool wifiConnected();

// ---- sensor.cpp ----
void sensorSetup();
void sensorLoop();
void sensorForceCalibration(uint16_t refCo2);
void sensorRecordHistory();
const char* sensorGetInitStatus();
uint8_t sensorGetInitAttempts();
bool sensorIsReady();
uint16_t sensorGetReadAttempts();
uint16_t sensorGetDataReadyCount();
uint16_t sensorGetCo2ZeroCount();
uint16_t sensorGetReadErrCount();
const char* sensorGetLastReadErr();
uint8_t sensorGetState();

// ---- mqtt.cpp ----
void mqttSetup();
void mqttLoop();
void mqttPublishState();
void mqttPublishDiscovery();
bool mqttIsConnected();
String getMqttLog();

// ---- web.cpp ----
void webSetup();
void webLoop();

// ---- display.cpp ----
void displaySetup();
void displayUpdate(bool force = false);

// ---- led.cpp ----
void ledSetup();
void ledUpdate();
void ledSetColor(uint8_t r, uint8_t g);

// ---- weather.cpp ----
void weatherSetup();
void weatherLoop();

// ---- ntp.cpp ----
void ntpSetup();
void ntpLoop();
String getTimeStr();
String getDateStr();
bool timeValid();

// ---- config.cpp ----
void configLoad();
void configSave();
void configReset();

// ---- co2_phrases.cpp ----
int co2_index(uint16_t co2);
const char* get_co2_phrase(uint16_t co2);
const char* get_co2_status(uint16_t co2);

// ---- utils.cpp ----
String getDeviceId();
String getUptime();
String getMacShort();

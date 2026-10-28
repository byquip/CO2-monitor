#pragma once
#include <Arduino.h>
#include "config.h"

#define CFG_KEY 0xC02A

struct DeviceConfig {
  uint16_t key = CFG_KEY;

  // WiFi
  char ssid[32]     = "";
  char pass[32]     = "";
  uint8_t wifiMode  = 1;  // 0=AP, 1=STA

  // MQTT
  bool mqttEnabled     = false;
  char mqttHost[32]    = "";
  uint16_t mqttPort    = MQTT_PORT_DEFAULT;
  char mqttLogin[16]   = "";
  char mqttPass[16]    = "";
  bool mqttAutoDisc    = true;

  // Weather (OpenWeatherMap)
  char owmApiKey[48]   = "";
  char owmCity[32]     = "";
  char owmLat[12]      = "";
  char owmLon[12]      = "";
  bool owmEnabled      = false;

  // Time
  int8_t timezone      = 3;  // UTC offset in hours (default +3 Moscow)
  char ntpServer[32]   = "pool.ntp.org";

  // Display
  uint8_t displayRotation = 0;
  uint8_t brightness   = 100;

  // Device
  char deviceName[24]  = "co2monitor";
  uint8_t co2AlertLevel = 2;  // 0=off, 1=moderate, 2=poor, 3=bad
  uint8_t displayTheme = 0;   // 0=full, 1=big number, 2=minimal

  // Calibration
  uint16_t co2CalibrationRef = 420;  // reference CO2 for forced recalibration

  // Web auth
  char webUser[16]     = "admin";
  char webPass[16]     = "";

  // LED night mode
  uint8_t ledNightMode = 0;   // 0=off, 1=on
  uint8_t ledOffHour   = 22;  // turn off at this hour
  uint8_t ledOnHour    = 7;   // turn on at this hour

  uint8_t _reserved[11] = {};
};

#define CO2_HISTORY_SIZE 144  // 24h at 10-min intervals

struct SensorData {
  uint16_t co2       = 0;
  float temperature  = NAN;
  float humidity     = NAN;
  bool valid         = false;
  uint32_t lastRead  = 0;

  uint16_t co2History[CO2_HISTORY_SIZE] = {};
  uint8_t  historyHead  = 0;
  uint8_t  historyCount = 0;
  uint32_t lastHistoryWrite = 0;
};

struct WeatherData {
  float tempOutdoor    = NAN;
  float humidityOut    = NAN;
  float pressure       = NAN;
  float windSpeed      = NAN;
  uint16_t windDeg     = 0;
  char description[32] = "";
  char icon[8]         = "";
  int16_t weatherId    = 0;
  bool valid           = false;
  uint32_t lastUpdate  = 0;
};

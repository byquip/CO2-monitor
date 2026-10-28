#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "data.h"
#include "prototypes.h"

extern DeviceConfig cfg;
extern WeatherData  weather;

static uint32_t lastWeatherUpdate = 0;

void weatherSetup() {
  if (!cfg.owmEnabled || cfg.owmApiKey[0] == 0) {
    DBGLN("Weather disabled");
    return;
  }
  DBGLN("Weather enabled");
}

void weatherLoop() {
  if (!cfg.owmEnabled || cfg.owmApiKey[0] == 0) return;
  if (!wifiConnected()) return;
  if (millis() - lastWeatherUpdate < WEATHER_UPDATE_INTERVAL && lastWeatherUpdate != 0) return;

  lastWeatherUpdate = millis();

  String url = String(OWM_ENDPOINT) + "?";
  if (cfg.owmLat[0] != 0 && cfg.owmLon[0] != 0) {
    url += "lat=" + String(cfg.owmLat) + "&lon=" + String(cfg.owmLon);
  } else if (cfg.owmCity[0] != 0) {
    url += "q=" + String(cfg.owmCity);
  } else {
    return;
  }
  url += "&appid=" + String(cfg.owmApiKey) + "&units=metric";

  DBGF("Weather fetch: %s\n", url.c_str());

  HTTPClient http;
  http.begin(url);
  http.setTimeout(10000);
  int code = http.GET();

  if (code == HTTP_CODE_OK) {
    String payload = http.getString();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      weather.tempOutdoor  = doc["main"]["temp"].as<float>();
      weather.humidityOut  = doc["main"]["humidity"].as<float>();
      weather.pressure     = doc["main"]["pressure"].as<float>();
      weather.windSpeed    = doc["wind"]["speed"].as<float>();
      weather.windDeg      = doc["wind"]["deg"].as<uint16_t>();
      weather.weatherId    = doc["weather"][0]["id"].as<int16_t>();

      strlcpy(weather.description, doc["weather"][0]["description"] | "",
              sizeof(weather.description));
      strlcpy(weather.icon, doc["weather"][0]["icon"] | "",
              sizeof(weather.icon));

      weather.valid = true;
      weather.lastUpdate = millis();

      DBGF("Weather: %.1fC %s (id=%d)\n",
           weather.tempOutdoor, weather.description, weather.weatherId);
    } else {
      DBGF("Weather JSON error: %s\n", err.c_str());
    }
  } else {
    DBGF("Weather HTTP error: %d\n", code);
  }
  http.end();
}

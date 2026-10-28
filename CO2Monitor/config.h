#pragma once
#include <Arduino.h>

// Uncomment for sensor-only build (no e-ink display, no LEDs)
// #define LIGHT_VERSION

// ==================== FIRMWARE ====================
#define FW_NAME       "CO2Monitor"
#define FW_VERSION    "1.0.0"
#define FW_VERSION_ID 100

// ==================== HARDWARE PINS ====================
// E-Paper Display (SPI)
#define PIN_EPD_BUSY  16
#define PIN_EPD_CS    20
#define PIN_EPD_RST   18
#define PIN_EPD_DC    19
#define PIN_EPD_SCK   17
#define PIN_EPD_MISO  -1
#define PIN_EPD_MOSI  14

// Dual LED (Red + Green = Yellow)
#define PIN_LED_RED   9
#define PIN_LED_GREEN 8

// SCD41 CO2 Sensor (I2C)
#define PIN_I2C_SDA   6
#define PIN_I2C_SCL   7

// Board LEDs (always off)
#define PIN_LED_BOARD 15

// ==================== TIMING ====================
#define SENSOR_READ_INTERVAL  60000UL   // read sensor every 60s
#define DISPLAY_UPDATE_INTERVAL 300000UL // refresh e-ink every 5 min
#define MQTT_RECONNECT_INTERVAL 30000UL
#define NTP_UPDATE_INTERVAL   3600000UL // sync NTP every hour
#define WEATHER_UPDATE_INTERVAL 900000UL // fetch weather every 15 min
#define WIFI_CONNECT_TIMEOUT  15000UL

// ==================== CO2 THRESHOLDS ====================
#define CO2_GOOD       800
#define CO2_MODERATE   1000
#define CO2_POOR       1400
#define CO2_BAD        2000

// ==================== DISPLAY ====================
#define DISP_WIDTH    400
#define DISP_HEIGHT   300

// ==================== MQTT DEFAULTS ====================
#define MQTT_PORT_DEFAULT   1883
#define MQTT_BUFFER_SIZE    512

// ==================== WEATHER ====================
#define OWM_ENDPOINT "https://api.openweathermap.org/data/2.5/weather"

// ==================== DEBUG ====================
#define DEBUG_SERIAL
#ifdef DEBUG_SERIAL
  #define DBG(x)   Serial.print(x)
  #define DBGLN(x) Serial.println(x)
  #define DBGF(...)  Serial.printf(__VA_ARGS__)
#else
  #define DBG(x)
  #define DBGLN(x)
  #define DBGF(...)
#endif

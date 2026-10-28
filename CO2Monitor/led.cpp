#include <Arduino.h>
#include "config.h"

#ifndef LIGHT_VERSION

#include <time.h>
#include "data.h"
#include "prototypes.h"

extern DeviceConfig cfg;
extern SensorData   sensor;

static uint32_t lastLedUpdate = 0;

void ledSetup() {
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_GREEN, LOW);

  pinMode(PIN_LED_BOARD, OUTPUT);
  digitalWrite(PIN_LED_BOARD, LOW);

  DBGLN("LED initialized");
}

void ledSetColor(uint8_t r, uint8_t g) {
  digitalWrite(PIN_LED_RED, r);
  digitalWrite(PIN_LED_GREEN, g);
}

static bool isNightTime() {
  if (!cfg.ledNightMode || !timeValid()) return false;
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  uint8_t h = t->tm_hour;
  if (cfg.ledOffHour > cfg.ledOnHour) {
    return (h >= cfg.ledOffHour || h < cfg.ledOnHour);
  }
  return (h >= cfg.ledOffHour && h < cfg.ledOnHour);
}

void ledUpdate() {
  if (millis() - lastLedUpdate < 2000) return;
  lastLedUpdate = millis();

  if (isNightTime()) {
    ledSetColor(0, 0);
    return;
  }

  if (!sensor.valid) {
    static bool blink = false;
    blink = !blink;
    ledSetColor(blink, blink);
    return;
  }

  int level = co2_index(sensor.co2);

  if (cfg.co2AlertLevel == 0) {
    ledSetColor(0, 0);
    return;
  }

  int threshold;
  switch (cfg.co2AlertLevel) {
    case 1: threshold = 1; break;
    case 2: threshold = 2; break;
    case 3: threshold = 3; break;
    default: threshold = 2; break;
  }

  if (level >= threshold) {
    ledSetColor(1, 0);
  } else if (level >= 1) {
    ledSetColor(1, 1);
  } else {
    ledSetColor(0, 1);
  }
}

#else // LIGHT_VERSION

void ledSetup() {
  pinMode(PIN_LED_BOARD, OUTPUT);
  digitalWrite(PIN_LED_BOARD, LOW);
}
void ledSetColor(uint8_t r, uint8_t g) { (void)r; (void)g; }
void ledUpdate() {}

#endif

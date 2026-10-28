#include <WiFi.h>
#include "config.h"

extern char deviceId[];

String getMacShort() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char buf[8];
  snprintf(buf, sizeof(buf), "%02X%02X", mac[4], mac[5]);
  return String(buf);
}

String getDeviceId() {
  return String(deviceId);
}

String getUptime() {
  uint32_t sec = millis() / 1000;
  uint32_t m = sec / 60;
  uint32_t h = m / 60;
  uint32_t d = h / 24;
  char buf[24];
  snprintf(buf, sizeof(buf), "%lud %luh %lum", (unsigned long)d,
           (unsigned long)(h % 24), (unsigned long)(m % 60));
  return String(buf);
}

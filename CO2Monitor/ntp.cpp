#include <time.h>
#include <WiFi.h>
#include "config.h"
#include "data.h"
#include "prototypes.h"

extern DeviceConfig cfg;

static bool ntpSynced = false;
static uint32_t lastNtpSync = 0;

void ntpSetup() {
  if (!wifiConnected()) return;
  long gmtOffset = cfg.timezone * 3600L;
  configTime(gmtOffset, 0, cfg.ntpServer, "time.nist.gov");
  DBGF("NTP configured: %s  UTC%+d\n", cfg.ntpServer, cfg.timezone);
}

void ntpLoop() {
  if (!wifiConnected()) return;
  if (ntpSynced && millis() - lastNtpSync < NTP_UPDATE_INTERVAL) return;

  struct tm timeInfo;
  if (getLocalTime(&timeInfo, 2000)) {
    ntpSynced = true;
    lastNtpSync = millis();
    DBGF("NTP time: %02d:%02d:%02d\n", timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
  }
}

bool timeValid() {
  return ntpSynced;
}

String getTimeStr() {
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 100)) return "--:--";
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", timeInfo.tm_hour, timeInfo.tm_min);
  return String(buf);
}

String getDateStr() {
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 100)) return "--/--";
  char buf[12];
  snprintf(buf, sizeof(buf), "%02d.%02d.%04d",
           timeInfo.tm_mday, timeInfo.tm_mon + 1, timeInfo.tm_year + 1900);
  return String(buf);
}

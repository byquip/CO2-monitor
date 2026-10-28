#include <Preferences.h>
#include "config.h"
#include "data.h"
#include "prototypes.h"

extern DeviceConfig cfg;

static Preferences prefs;

void configLoad() {
  prefs.begin("co2mon", true);
  size_t len = prefs.getBytesLength("cfg");
  if (len == sizeof(DeviceConfig)) {
    prefs.getBytes("cfg", &cfg, sizeof(cfg));
    if (cfg.key != CFG_KEY) {
      DBGLN("Config key mismatch, resetting");
      configReset();
    } else {
      DBGLN("Config loaded");
      if (cfg.ledOffHour == 0 && cfg.ledOnHour == 0) {
        cfg.ledOffHour = 22;
        cfg.ledOnHour = 7;
      }
    }
  } else {
    DBGLN("No config found, using defaults");
    configReset();
  }
  prefs.end();
}

void configSave() {
  prefs.begin("co2mon", false);
  prefs.putBytes("cfg", &cfg, sizeof(cfg));
  prefs.end();
  DBGLN("Config saved");
}

void configReset() {
  DeviceConfig def;
  memcpy(&cfg, &def, sizeof(cfg));
  configSave();
  DBGLN("Config reset to defaults");
}

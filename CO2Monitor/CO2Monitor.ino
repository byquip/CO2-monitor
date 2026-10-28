#include "config.h"
#include "data.h"
#include "prototypes.h"

DeviceConfig cfg;
SensorData   sensor;
WeatherData  weather;

char deviceId[8] = "";

void setup() {
#ifdef DEBUG_SERIAL
  Serial.begin(115200);
  delay(1000);
  DBGLN("\n=== CO2 Monitor v" FW_VERSION " ===");
#endif

  configLoad();

  String id = getMacShort();
  strncpy(deviceId, id.c_str(), sizeof(deviceId) - 1);
  DBGF("Device ID: %s\n", deviceId);

  ledSetup();
  sensorSetup();
  displaySetup();
  wifiSetup();
  ntpSetup();
  webSetup();
  mqttSetup();
  weatherSetup();

  displayUpdate(true);
  DBGLN("Setup complete");
}

void loop() {
  wifiLoop();
  sensorLoop();
  ntpLoop();
  weatherLoop();
  mqttLoop();
  webLoop();
  ledUpdate();
  displayUpdate();
  yield();
}

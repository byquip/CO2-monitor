#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include "config.h"
#include "data.h"
#include "prototypes.h"

extern SensorData sensor;
extern DeviceConfig cfg;

#ifdef NO_ERROR
#undef NO_ERROR
#endif
#define NO_ERROR 0

static SensirionI2cScd4x scd;
static uint32_t lastSensorRead = 0;
static uint32_t shotSentAt = 0;
static bool sensorReady = false;
static bool wireStarted = false;
static uint8_t sensorState = 0; // 0=idle, 1=wait-ready, 2=read
static char errMsg[64];
static char sensorInitStatus[48] = "not started";
static uint8_t initAttempts = 0;
static uint16_t readAttempts = 0;
static uint16_t dataReadyCount = 0;
static uint16_t co2ZeroCount = 0;
static uint16_t readErrCount = 0;
static char lastReadErr[48] = "none";
#define HISTORY_INTERVAL 600000UL  // 10 minutes

const char* sensorGetInitStatus() { return sensorInitStatus; }
uint8_t sensorGetInitAttempts() { return initAttempts; }
bool sensorIsReady() { return sensorReady; }
uint16_t sensorGetReadAttempts() { return readAttempts; }
uint16_t sensorGetDataReadyCount() { return dataReadyCount; }
uint16_t sensorGetCo2ZeroCount() { return co2ZeroCount; }
uint16_t sensorGetReadErrCount() { return readErrCount; }
const char* sensorGetLastReadErr() { return lastReadErr; }
uint8_t sensorGetState() { return sensorState; }

static uint8_t co2FailCount = 0;

static bool sensorReadFull(uint16_t &co2, float &temp, float &hum) {
  int16_t err;

  scd.wakeUp();

  err = scd.measureSingleShot();
  if (err != NO_ERROR) {
    DBGF("dummy shot err=%d\n", err);
    return false;
  }

  err = scd.measureSingleShot();
  if (err != NO_ERROR) {
    DBGF("real shot err=%d\n", err);
    return false;
  }

  bool dataReady = false;
  uint32_t pollStart = millis();
  while (!dataReady && millis() - pollStart < 10000) {
    delay(100);
    scd.getDataReadyStatus(dataReady);
  }
  if (!dataReady) {
    DBGLN("CO2 data not ready");
    return false;
  }

  err = scd.readMeasurement(co2, temp, hum);
  if (err != NO_ERROR) {
    DBGF("read err=%d\n", err);
    return false;
  }

  DBGF("Full: co2=%d t=%.1f h=%.1f\n", co2, temp, hum);
  return (co2 > 0);
}

static bool sensorReadRHT(uint16_t &co2, float &temp, float &hum) {
  int16_t err;

  scd.wakeUp();

  err = scd.measureSingleShotRhtOnly();
  if (err != NO_ERROR) return false;
  delay(100);

  bool dataReady = false;
  uint32_t pollStart = millis();
  while (!dataReady && millis() - pollStart < 2000) {
    delay(50);
    scd.getDataReadyStatus(dataReady);
  }
  if (!dataReady) return false;

  err = scd.readMeasurement(co2, temp, hum);
  if (err != NO_ERROR) return false;

  DBGF("RHT: co2=%d t=%.1f h=%.1f\n", co2, temp, hum);
  return true;
}

void sensorSetup() {
  initAttempts++;

  if (!wireStarted) {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    wireStarted = true;
    delay(100);
  }

  scd.begin(Wire, SCD41_I2C_ADDR_62);
  DBGLN("\n=== SCD41 INIT ===");

  int16_t err;

  err = scd.wakeUp();
  err = scd.stopPeriodicMeasurement();
  delay(500);
  err = scd.reinit();
  delay(30);

  uint64_t serial = 0;
  err = scd.getSerialNumber(serial);
  DBGF("serial: 0x%08X%08X\n",
       (uint32_t)(serial >> 32), (uint32_t)(serial & 0xFFFFFFFF));
  if (err != NO_ERROR) {
    errorToString(err, errMsg, sizeof(errMsg));
    snprintf(sensorInitStatus, sizeof(sensorInitStatus), "FAIL serial: %.20s", errMsg);
    return;
  }

  sensorReady = true;
  sensorState = 0;
  strncpy(sensorInitStatus, "OK", sizeof(sensorInitStatus));
  DBGLN("=== SCD41 INIT DONE ===\n");
}

void sensorLoop() {
  if (!sensorReady) {
    if (millis() - lastSensorRead < 10000UL && lastSensorRead != 0) return;
    lastSensorRead = millis();
    Wire.beginTransmission(0x62);
    if (Wire.endTransmission() == 0) sensorSetup();
    return;
  }

  uint32_t interval = sensor.valid ? SENSOR_READ_INTERVAL : 5000UL;
  if (millis() - lastSensorRead < interval && lastSensorRead != 0) return;
  lastSensorRead = millis();

  readAttempts++;
  sensorState = 1;

  uint16_t co2 = 0;
  float temp = 0, hum = 0;
  bool ok = false;

  if (co2FailCount < 3 || readAttempts % 10 == 0) {
    DBGF("=== read #%d full CO2 ===\n", readAttempts);
    ok = sensorReadFull(co2, temp, hum);
    if (ok) {
      co2FailCount = 0;
      strncpy(lastReadErr, "none", sizeof(lastReadErr));
    } else {
      co2FailCount++;
      DBGF("CO2 fail #%d\n", co2FailCount);
      scd.stopPeriodicMeasurement();
      delay(500);
      scd.reinit();
      delay(30);
    }
  }

  if (!ok) {
    DBGF("=== read #%d RHT ===\n", readAttempts);
    ok = sensorReadRHT(co2, temp, hum);
    if (ok) {
      strncpy(lastReadErr, "rht-only", sizeof(lastReadErr));
    }
  }

  sensorState = 0;

  if (!ok) {
    readErrCount++;
    strncpy(lastReadErr, "all failed", sizeof(lastReadErr));
    return;
  }

  dataReadyCount++;

  if (co2 > 6000) {
    DBGF("CO2=%d > 6000, sensor error\n", co2);
    co2ZeroCount++;
    co2 = sensor.co2;
  }

  sensor.co2 = co2;
  sensor.temperature = temp;
  sensor.humidity = hum;
  sensor.valid = true;
  sensor.lastRead = millis();

  DBGF("CO2=%d T=%.1f H=%.1f\n", sensor.co2, sensor.temperature, sensor.humidity);

  sensorRecordHistory();
  mqttPublishState();
}

void sensorRecordHistory() {
  if (millis() - sensor.lastHistoryWrite < HISTORY_INTERVAL && sensor.lastHistoryWrite != 0) return;
  sensor.lastHistoryWrite = millis();

  sensor.co2History[sensor.historyHead] = sensor.co2;
  sensor.historyHead = (sensor.historyHead + 1) % CO2_HISTORY_SIZE;
  if (sensor.historyCount < CO2_HISTORY_SIZE) sensor.historyCount++;

  DBGF("CO2 history: %d points\n", sensor.historyCount);
}

void sensorForceCalibration(uint16_t refCo2) {
  DBGF("SCD41 forced recalibration at %d ppm\n", refCo2);

  uint16_t frcCorrection = 0;
  int16_t err = scd.performForcedRecalibration(refCo2, frcCorrection);
  if (err != NO_ERROR) {
    errorToString(err, errMsg, sizeof(errMsg));
    DBGF("SCD41 calibration FAILED: %s\n", errMsg);
  } else {
    DBGF("SCD41 calibration OK, correction: %d\n", frcCorrection);
  }
  delay(500);
}

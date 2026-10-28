#include "config.h"

#ifndef LIGHT_VERSION

#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include "data.h"
#include "prototypes.h"
#include "icons.h"

extern DeviceConfig cfg;
extern SensorData   sensor;
extern WeatherData  weather;

GxEPD2_BW<GxEPD2_420_GDEY042T81, GxEPD2_420_GDEY042T81::HEIGHT> display(
  GxEPD2_420_GDEY042T81(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));

static uint32_t lastDisplayUpdate = 0;

// ---- helpers ----

static void drawCentered(const char* text, int y) {
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((DISP_WIDTH - w) / 2, y);
  display.print(text);
}

static void drawRightAligned(const char* text, int x, int y) {
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(x - w, y);
  display.print(text);
}

static void drawHLine(int x, int y, int w, int thickness = 1) {
  for (int i = 0; i < thickness; i++)
    display.drawFastHLine(x, y + i, w, GxEPD_BLACK);
}

static void drawVLine(int x, int y, int h) {
  display.drawFastVLine(x, y, h, GxEPD_BLACK);
}

// ---- CO2 history graph ----

static void drawCO2History(int x, int y, int w, int h) {
  if (sensor.historyCount < 2) {
    display.setFont(NULL);
    display.setCursor(x + w / 2 - 30, y + h / 2 - 4);
    display.print("collecting...");
    display.drawRect(x, y, w, h, GxEPD_BLACK);
    return;
  }

  display.drawRect(x, y, w, h, GxEPD_BLACK);

  uint16_t vmin = 65535, vmax = 0;
  for (int i = 0; i < sensor.historyCount; i++) {
    int idx = (sensor.historyHead - sensor.historyCount + i + CO2_HISTORY_SIZE) % CO2_HISTORY_SIZE;
    if (sensor.co2History[idx] < vmin) vmin = sensor.co2History[idx];
    if (sensor.co2History[idx] > vmax) vmax = sensor.co2History[idx];
  }
  if (vmax - vmin < 50) { vmin = (vmin > 25) ? vmin - 25 : 0; vmax += 25; }

  // Threshold dashed lines
  uint16_t thresholds[] = { CO2_GOOD, CO2_MODERATE, CO2_POOR };
  for (int t = 0; t < 3; t++) {
    if (thresholds[t] > vmin && thresholds[t] < vmax) {
      int ty = y + h - 1 - (int)((float)(thresholds[t] - vmin) / (vmax - vmin) * (h - 2));
      for (int dx = x + 1; dx < x + w - 1; dx += 4)
        display.drawPixel(dx, ty, GxEPD_BLACK);
    }
  }

  // Sparkline (2px thick)
  int prevPx = -1, prevPy = -1;
  for (int i = 0; i < sensor.historyCount; i++) {
    int idx = (sensor.historyHead - sensor.historyCount + i + CO2_HISTORY_SIZE) % CO2_HISTORY_SIZE;
    int px = x + 1 + (int)((float)i / (sensor.historyCount - 1) * (w - 3));
    int py = y + h - 2 - (int)((float)(sensor.co2History[idx] - vmin) / (vmax - vmin) * (h - 4));
    if (prevPx >= 0) {
      display.drawLine(prevPx, prevPy, px, py, GxEPD_BLACK);
      display.drawLine(prevPx, prevPy + 1, px, py + 1, GxEPD_BLACK);
    }
    prevPx = px; prevPy = py;
  }

  // Labels
  display.setFont(NULL);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", vmax);
  display.setCursor(x + 2, y + 2);
  display.print(buf);
  snprintf(buf, sizeof(buf), "%d", vmin);
  display.setCursor(x + 2, y + h - 10);
  display.print(buf);
  display.setCursor(x + w - 20, y + h - 10);
  display.print("24h");
}

// ---- status bar ----

static void drawStatusBar() {
  int y = DISP_HEIGHT - 16;
  drawHLine(0, y - 3, DISP_WIDTH, 2);

  display.setFont(NULL);
  display.setTextColor(GxEPD_BLACK);

  if (timeValid()) {
    display.setCursor(4, y);
    display.print(getTimeStr());
    display.setCursor(50, y);
    display.print(getDateStr());
  }

  if (cfg.mqttEnabled && mqttIsConnected()) {
    display.setCursor(DISP_WIDTH - 28, y);
    display.print("MQTT");
  }
  display.drawInvertedBitmap(DISP_WIDTH - 48, y - 1, icon_wifi_16, 16, 16, GxEPD_BLACK);
}

// ==================== THEME 0: FULL ====================

static void drawThemeFull() {
  display.setFont(&FreeMonoBold24pt7b);
  display.setTextColor(GxEPD_BLACK);

  char co2str[12];
  snprintf(co2str, sizeof(co2str), "%d", sensor.co2);
  int16_t x1, y1; uint16_t tw, th;
  display.getTextBounds(co2str, 0, 0, &x1, &y1, &tw, &th);
  int co2x = (DISP_WIDTH - tw - 60) / 2;
  display.setCursor(co2x, 48);
  display.print(co2str);

  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(co2x + tw + 6, 32);
  display.print("ppm");
  display.setCursor(co2x + tw + 6, 48);
  display.print("CO2");

  int barX = 20, barY = 58, barW = DISP_WIDTH - 40, barH = 8;
  display.drawRect(barX, barY, barW, barH, GxEPD_BLACK);
  int level = co2_index(sensor.co2);
  int fillW = map(constrain(level, 0, 4), 0, 4, 0, barW - 2);
  display.fillRect(barX + 1, barY + 1, fillW, barH - 2, GxEPD_BLACK);

  display.setFont(&FreeSans9pt7b);
  drawCentered(get_co2_phrase(sensor.co2), 82);

  drawHLine(0, 90, DISP_WIDTH, 2);

  int midX = DISP_WIDTH / 2;
  drawVLine(midX, 90, 72);

  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(8, 108);
  display.print("Indoor");

  display.setFont(&FreeMonoBold12pt7b);
  display.drawInvertedBitmap(8, 116, icon_thermometer_16, 16, 16, GxEPD_BLACK);
  char buf[24];
  snprintf(buf, sizeof(buf), "%.1f", sensor.temperature);
  display.setCursor(28, 131);
  display.print(buf);
  display.setFont(&FreeMonoBold9pt7b);
  display.print(" C");

  display.setFont(&FreeMonoBold12pt7b);
  display.drawInvertedBitmap(8, 138, icon_humidity_16, 16, 16, GxEPD_BLACK);
  snprintf(buf, sizeof(buf), "%.0f%%", sensor.humidity);
  display.setCursor(28, 153);
  display.print(buf);

  if (weather.valid) {
    display.setFont(&FreeSansBold9pt7b);
    display.setCursor(midX + 8, 108);
    display.print("Outdoor");

    const uint8_t* icon = getWeatherIcon32(weather.weatherId, weather.icon);
    if (icon) {
      display.drawInvertedBitmap(midX + 8, 114, icon, 32, 32, GxEPD_BLACK);
    }

    display.setFont(&FreeMonoBold12pt7b);
    snprintf(buf, sizeof(buf), "%.0f C", weather.tempOutdoor);
    display.setCursor(midX + 46, 131);
    display.print(buf);

    display.setFont(NULL);
    char desc[18];
    strncpy(desc, weather.description, 17);
    desc[17] = 0;
    display.setCursor(midX + 46, 138);
    display.print(desc);

    display.setFont(&FreeMonoBold9pt7b);
    snprintf(buf, sizeof(buf), "%.0f%%", weather.humidityOut);
    display.setCursor(midX + 8, 158);
    display.print(buf);

    snprintf(buf, sizeof(buf), "%.0f hPa", weather.pressure);
    drawRightAligned(buf, DISP_WIDTH - 8, 158);
  }

  drawHLine(0, 162, DISP_WIDTH, 2);
  drawCO2History(8, 168, DISP_WIDTH - 16, 110);
  drawStatusBar();
}

// ==================== THEME 1: BIG NUMBER ====================

static void drawThemeBigNumber() {
  display.setTextColor(GxEPD_BLACK);

  display.setFont(&FreeMonoBold24pt7b);
  char co2str[12];
  snprintf(co2str, sizeof(co2str), "%d", sensor.co2);
  drawCentered(co2str, 70);

  display.setFont(&FreeMonoBold18pt7b);
  drawCentered("ppm", 105);

  display.setFont(&FreeMonoBold12pt7b);
  drawCentered(get_co2_status(sensor.co2), 140);

  int barX = 40, barY = 150, barW = DISP_WIDTH - 80, barH = 10;
  display.drawRect(barX, barY, barW, barH, GxEPD_BLACK);
  int level = co2_index(sensor.co2);
  int fillW = map(constrain(level, 0, 4), 0, 4, 0, barW - 2);
  display.fillRect(barX + 1, barY + 1, fillW, barH - 2, GxEPD_BLACK);

  drawHLine(0, 168, DISP_WIDTH);
  drawCO2History(8, 174, DISP_WIDTH - 16, 80);

  display.setFont(&FreeMonoBold9pt7b);
  char buf[48];
  snprintf(buf, sizeof(buf), "%.1fC  %.0f%%", sensor.temperature, sensor.humidity);
  display.setCursor(8, 270);
  display.print(buf);

  if (weather.valid) {
    snprintf(buf, sizeof(buf), "Out: %.0fC", weather.tempOutdoor);
    drawRightAligned(buf, DISP_WIDTH - 8, 270);
  }

  drawStatusBar();
}

// ==================== THEME 2: MINIMAL ====================

static void drawThemeMinimal() {
  display.setTextColor(GxEPD_BLACK);

  display.setFont(&FreeMonoBold18pt7b);
  char co2str[16];
  snprintf(co2str, sizeof(co2str), "%d ppm", sensor.co2);
  drawCentered(co2str, 40);

  display.setFont(&FreeSans9pt7b);
  drawCentered(get_co2_status(sensor.co2), 60);

  drawHLine(20, 68, DISP_WIDTH - 40);

  display.setFont(&FreeMonoBold9pt7b);
  char buf[24];
  snprintf(buf, sizeof(buf), "%.1f C  %.0f%%", sensor.temperature, sensor.humidity);
  display.setCursor(20, 88);
  display.print(buf);

  if (weather.valid) {
    snprintf(buf, sizeof(buf), "%.0f C  %s", weather.tempOutdoor, weather.description);
    drawRightAligned(buf, DISP_WIDTH - 20, 88);
  }

  drawCO2History(12, 100, DISP_WIDTH - 24, 170);
  drawStatusBar();
}

// ==================== SETUP & UPDATE ====================

void displaySetup() {
  display.init(115200, true, 2, false);
  SPI.end();
  SPI.begin(PIN_EPD_SCK, PIN_EPD_MISO, PIN_EPD_MOSI, PIN_EPD_CS);
  display.setRotation(cfg.displayRotation);
  display.setTextWrap(false);
  DBGLN("Display initialized");
}

static bool showedValidData = false;

void displayUpdate(bool force) {
  if (sensor.valid && !showedValidData) {
    force = true;
    showedValidData = true;
  }
  if (!force && millis() - lastDisplayUpdate < DISPLAY_UPDATE_INTERVAL) return;
  if (!sensor.valid && !force) return;

  lastDisplayUpdate = millis();
  DBGF("Display refresh, heap=%d valid=%d co2=%d\n",
       ESP.getFreeHeap(), sensor.valid, sensor.co2);

  display.init(0, false);
  SPI.end();
  SPI.begin(PIN_EPD_SCK, PIN_EPD_MISO, PIN_EPD_MOSI, PIN_EPD_CS);
  display.setRotation(cfg.displayRotation);

  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    if (sensor.valid) {
      switch (cfg.displayTheme) {
        case 1:  drawThemeBigNumber(); break;
        case 2:  drawThemeMinimal();   break;
        default: drawThemeFull();      break;
      }
    } else {
      display.setFont(&FreeMonoBold12pt7b);
      drawCentered("CO2 Monitor", 80);
      display.setFont(&FreeSans9pt7b);
      drawCentered("Waiting for sensor...", 120);
      drawCentered("v" FW_VERSION, 150);
    }
    yield();
  } while (display.nextPage());
  display.hibernate();
  DBGLN("Display done");
}

#else // LIGHT_VERSION

#include "prototypes.h"
void displaySetup() {}
void displayUpdate(bool force) { (void)force; }

#endif

# CO2 Monitor — ESP32-C6 Smart Home Device

Indoor air quality monitor built on ESP32-C6 with SCD41 CO2 sensor, 4.2" e-ink display, web UI, MQTT and Home Assistant auto-discovery.

## Features

- Real-time CO2, temperature, and humidity monitoring (SCD41 sensor)
- 4.2" e-ink display with 3 themes (full, big number, minimal)
- 24-hour CO2 history graph on display
- Web interface for configuration and live data
- MQTT integration with Home Assistant auto-discovery
- OpenWeatherMap outdoor weather display
- OTA firmware updates via browser
- Dual LED indicator (red/green/yellow) with night mode scheduling
- Captive portal for initial Wi-Fi setup
- JSON API endpoint
- SCD41 forced recalibration via web UI or MQTT

## Hardware

| Component | Connection |
|-----------|------------|
| ESP32-C6 DevKitM-1 | Main MCU |
| SCD41 (CO2/T/H) | I2C: SDA → GPIO 6, SCL → GPIO 7 |
| E-ink GDEY042T81 400x300 | SPI: MOSI→14, SCK→17, CS→20, DC→19, RST→18, BUSY→16 |
| LED Green | GPIO 8 |
| LED Red | GPIO 9 |

### Light Version

A sensor-only build is available for setups without a display or LEDs (just ESP32-C6 + SCD41). Uncomment `#define LIGHT_VERSION` in `config.h` to enable it. The light version keeps Wi-Fi, web UI, MQTT, NTP and weather — only the display and LED code is excluded, so GxEPD2 library is not required.

## Libraries (Arduino IDE → Library Manager)

- **GxEPD2** — e-ink display driver (not needed for light version)
- **Sensirion I2C SCD4x** — CO2 sensor
- **PubSubClient** — MQTT client
- **ArduinoJson** — JSON serialization

## Setup

### Board Manager

1. In Arduino IDE, open **File → Preferences**
2. Add to "Additional boards manager URLs":
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. Open **Tools → Board → Boards Manager**, search `esp32`, install **esp32** by Espressif
4. Select board **ESP32-C6 Dev Module**

### Flash

1. Open `CO2Monitor/CO2Monitor.ino` in Arduino IDE
2. Install the libraries listed above via Library Manager
3. Select the correct COM port
4. Upload

### First Boot

1. The device creates a Wi-Fi access point **CO2Monitor-XXXX** (password: `12345678`)
2. Connect to it — a captive portal opens
3. Enter your home Wi-Fi SSID and password
4. The device reboots and connects to your network
5. Find its IP in your router or Serial Monitor and open it in a browser

## Web Interface

| Page | URL | Description |
|------|-----|-------------|
| Status | `/` | CO2, temperature, humidity, weather, system info |
| Settings | `/settings` | Device name, NTP, weather (OWM), display themes, LED night mode, calibration |
| MQTT | `/mqtt` | Broker connection, HA auto-discovery toggle |
| Firmware Update | `/update` | OTA upload (.bin) |
| Debug | `/debug` | I2C scan, sensor diagnostics, system info |
| API | `/api/status` | JSON with all data |

## Home Assistant Integration

### If MQTT is already set up in HA

Use the same broker and credentials. Skip to step 2.

### If MQTT is not set up yet

1. **Settings → Add-ons → Add-on Store** → install **Mosquitto broker** → Start
2. Create an MQTT user: **Settings → People → Users → Add User**
3. **Settings → Devices & Services → Add Integration → MQTT**
   - Broker: `localhost`, Port: `1883`
   - Username / Password from step 2

### Configure MQTT on the device

1. Open the device's web UI → **MQTT** page
2. Enable MQTT
3. Enter:
   - **Broker:** Home Assistant IP (e.g., `192.168.0.10`)
   - **Port:** `1883`
   - **Username / Password:** MQTT user credentials
   - **HA Auto-Discovery:** enable
4. Click **Save**

### Verify

Go to **Settings → Devices & Services → MQTT** in HA — **CO2 Monitor XXXX** should appear with all sensors.

### Auto-discovered Entities

**Sensors:**

| Entity | Description |
|--------|-------------|
| `sensor.co2_monitor_xxxx_co2` | CO2 concentration (ppm) |
| `sensor.co2_monitor_xxxx_temperature` | Indoor temperature (°C) |
| `sensor.co2_monitor_xxxx_humidity` | Indoor humidity (%) |
| `sensor.co2_monitor_xxxx_temp_outdoor` | Outdoor temperature (°C)* |
| `sensor.co2_monitor_xxxx_humidity_out` | Outdoor humidity (%)* |
| `sensor.co2_monitor_xxxx_pressure` | Atmospheric pressure (hPa)* |

*\* Only when OpenWeatherMap is enabled*

**Binary sensor:**

| Entity | Description |
|--------|-------------|
| `binary_sensor.co2_monitor_xxxx_ventilation` | ON when CO2 exceeds alert threshold |

**Controls:**

| Entity | Range | Description |
|--------|-------|-------------|
| `number.co2_monitor_xxxx_co2_alert` | 0–3 | Alert level: 0=off, 1=moderate, 2=poor, 3=bad |
| `number.co2_monitor_xxxx_display_theme` | 0–2 | Display theme: 0=full, 1=big number, 2=minimal |

### MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `co2mon_XXXX/state` | device → HA | JSON with all sensor data |
| `co2mon_XXXX/set/co2_alert` | HA → device | Set alert level (0–3) |
| `co2mon_XXXX/set/display_theme` | HA → device | Set display theme (0–2) |
| `co2mon_XXXX/set/display_rotation` | HA → device | Set display rotation (0–3) |
| `co2mon_XXXX/set/calibrate` | HA → device | Force SCD41 recalibration (ppm value, 400–2000) |
| `co2mon_XXXX/availability` | device → HA | `online` / `offline` |

*XXXX = last 4 characters of MAC address*

### State JSON Example

```json
{
  "co2": 847,
  "temperature": "23.4",
  "humidity": "48.2",
  "co2_status": "Good",
  "temp_outdoor": "15.3",
  "humidity_out": "72.0",
  "pressure": "1013",
  "weather": "clear sky",
  "co2_alert": 2,
  "display_theme": 0,
  "ventilation": "OFF",
  "uptime": "1d 3h 22m"
}
```

## Automation Examples

### Ventilation notification

```yaml
automation:
  - alias: "CO2 alert"
    trigger:
      - platform: state
        entity_id: binary_sensor.co2_monitor_xxxx_ventilation
        to: "on"
        for: "00:02:00"
    action:
      - service: notify.mobile_app_your_phone
        data:
          title: "CO2 Monitor"
          message: "CO2 {{ states('sensor.co2_monitor_xxxx_co2') }} ppm — open a window!"
```

### Auto-ventilation fan

```yaml
automation:
  - alias: "CO2 fan on"
    trigger:
      - platform: numeric_state
        entity_id: sensor.co2_monitor_xxxx_co2
        above: 1200
        for: "00:05:00"
    action:
      - service: switch.turn_on
        target:
          entity_id: switch.ventilation_fan
  - alias: "CO2 fan off"
    trigger:
      - platform: numeric_state
        entity_id: sensor.co2_monitor_xxxx_co2
        below: 800
        for: "00:10:00"
    action:
      - service: switch.turn_off
        target:
          entity_id: switch.ventilation_fan
```

## Calibration

1. Place the device outdoors or at an open window for **3+ minutes**
2. Open `/settings` → set reference CO2 (default 420 ppm) → click **Calibrate Now**
3. Or via MQTT: publish `420` to `co2mon_XXXX/set/calibrate`

Standard outdoor CO2 is approximately 420 ppm.

## OTA Update

1. Compile in Arduino IDE: **Sketch → Export Compiled Binary**
2. Open `/update` on the device
3. Upload the `.bin` file
4. The device reboots with the new firmware

## Display Themes

| # | Name | Description |
|---|------|-------------|
| 0 | Full | All data: CO2 + 24h graph + indoor + outdoor weather |
| 1 | Big Number | Large CO2 + status + graph + compact info line |
| 2 | Minimal | CO2 + status + indoor/outdoor + large graph |

## LED Indicator

| Color | Meaning |
|-------|---------|
| Green | CO2 level is good |
| Yellow | CO2 level is moderate |
| Red | CO2 level exceeds alert threshold |
| Blinking yellow | Waiting for sensor data |
| Off | Night mode active, or alerts disabled |

Night mode (configurable via Settings) turns the LED off during specified hours (default: 22:00–07:00).

## Project Structure

```
CO2Monitor/
├── CO2Monitor.ino   # Main sketch: setup() and loop()
├── config.h         # Pin definitions, timing, thresholds, LIGHT_VERSION flag
├── data.h           # Data structures (DeviceConfig, SensorData, WeatherData)
├── prototypes.h     # Function declarations
├── sensor.cpp       # SCD41 sensor: init, measurement (full CO2 + RHT fallback)
├── display.cpp      # E-ink display: 3 themes, history graph
├── led.cpp          # Dual LED indicator with night mode
├── web.cpp          # Web server: status, settings, MQTT, OTA, debug, API
├── mqtt.cpp         # MQTT client, HA auto-discovery
├── wifi.cpp         # Wi-Fi STA/AP, captive portal
├── ntp.cpp          # NTP time sync
├── weather.cpp      # OpenWeatherMap client
├── config.cpp       # Config persistence (Preferences/NVS)
├── co2_phrases.cpp  # CO2 level descriptions and phrases
├── icons.h          # Display icons (weather, thermometer, humidity, wifi)
└── utils.cpp        # Helper functions (MAC, uptime)
```

## License

MIT

# Water Monitor v2.0 — Firmware

Complete water monitoring solution for the ESP8266 (Wemos D1 Mini):
- Flow measurement (DN32 + PC817 optocoupler)
- Tank level measurement (JSN-SR04T)
- MQTT publishing + Home Assistant auto-discovery
- Web UI for configuration, calibration, and OTA updates
- Dual WiFi mode (AP + STA simultaneously)
- LittleFS persistent storage

🌐 **[Česká verze](README.cs.md)**

## Project structure

```
water_monitor/
├── water_monitor.ino    # Main setup/loop
├── config.h             # Constants (HW pins, intervals)
├── config_storage.{h,cpp}  # Persistent configuration
├── calibration.{h,cpp}  # Calibration table
├── sensors.{h,cpp}      # Flow meter + ultrasonic
├── mqtt_handler.{h,cpp} # MQTT + HA discovery
├── web_server.{h,cpp}   # Web UI + OTA + REST API
└── data/                # Web assets (uploaded to LittleFS)
    ├── index.html
    ├── style.css
    └── app.js
```

## Installation in Arduino IDE

### 1. ESP8266 board manager
Preferences → Additional boards manager URLs:
```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```
Tools → Board → Board Manager → search for `esp8266` → install **esp8266 by ESP8266 Community** (version 3.1.2 or newer)

Tools → Board → ESP8266 → **LOLIN(WEMOS) D1 R2 & mini**

### 2. Libraries
Tools → Manage Libraries → install:
- **PubSubClient** (Nick O'Leary) ≥ 2.8
- **ArduinoJson** (Benoit Blanchon) ≥ 6.21
- **NewPing** (Tim Eckel) ≥ 1.9

ESP8266WiFi, ESP8266WebServer, ESP8266HTTPUpdateServer, and LittleFS are part of the board package.

### 3. LittleFS upload plugin (critical!)
Download: https://github.com/earlephilhower/arduino-littlefs-upload/releases
- Download the `.vsix` file
- For Arduino IDE 2.x: extract into `~/.arduinoIDE/plugins/` (Linux/Mac) or `%LOCALAPPDATA%\Arduino IDE\plugins\` (Windows)
- Restart the IDE

In the IDE: Ctrl+Shift+P → "Upload LittleFS to Pico/ESP8266/ESP32" — this command uploads the contents of the `data/` folder to the ESP filesystem.

## First boot

1. **Flash the firmware**: Sketch → Upload (Ctrl+U)
2. **Upload LittleFS data**: Ctrl+Shift+P → "Upload LittleFS to..."
3. **Open the Serial Monitor** (115200 baud) — you'll see startup logs
4. The ESP boots in dual mode:
   - AP "WaterMonitor-Setup" (always available)
   - STA — if WiFi is configured
5. **Connect to the AP** (open network), open http://192.168.4.1
6. **Configuration tab**: enter your home WiFi + MQTT broker, save
7. **Restart** (System tab)
8. The ESP connects to your home WiFi; find its IP in the Serial Monitor or your router
9. Continue with **Calibration** setup

## Web UI — what's where

### 📊 Dashboard
- Live flow L/min + total volume
- Tank visualization (level column)
- Level, volume, sensor distance

### ⚙️ Configuration
- WiFi (SSID + password) — the "Scan" button finds nearby networks
- MQTT broker (server, port, login)
- Device name in HA + ID (MQTT topic prefix)
- Tank geometry (distance when full/empty, max volume)

### 📏 Calibration
- Current level + computed volume
- Add a new point (level ↔ volume)
- Table of existing points with a delete button
- Reset to linear approximation

### 🔧 System
- Version, uptime, free heap
- IP addresses (STA + AP), WiFi signal
- Link to **/update** for OTA flashing
- Restart the ESP

## OTA Update

1. Build the firmware: Sketch → Export compiled binary (creates a `.bin` in the project directory)
2. Open `http://<ip>/update`
3. Select the `.bin` file, click Update
4. The ESP restarts with the new firmware

**Note**: OTA updates do NOT upload the files in `data/`. For web UI changes you must use LittleFS upload (over USB).

## MQTT topics

Topics are prefixed with the `device_id` value (default `watertank`):

| Topic | Interval | Value |
|---|---|---|
| `watertank/flowmeter/flow_lpm` | 1s | current flow L/min |
| `watertank/flowmeter/volume_minute` | 60s | volume in the last minute |
| `watertank/flowmeter/volume_hour` | 1h | volume in the last hour |
| `watertank/flowmeter/volume_total` | 60s | cumulative volume since start |
| `watertank/tank/distance_cm` | 60s | sensor-to-surface distance |
| `watertank/tank/level_cm` | 60s | water level from the bottom |
| `watertank/tank/volume_liters` | 60s | volume in the tank |
| `watertank/tank/fill_percent` | 60s | fill level in % |
| `watertank/status` | LWT | online/offline |

## Home Assistant

After the first MQTT connection, the device **appears automatically in HA** as a single "Water Tank" device with 8 sensors. No manual configuration is needed in HA (you just need the MQTT integration active).

## REST API

| Endpoint | Method | Description |
|---|---|---|
| `/api/status` | GET | Live data + system info |
| `/api/config` | GET | Current configuration (passwords hidden) |
| `/api/config` | POST | Update configuration (JSON body) |
| `/api/calibration` | GET | Calibration points |
| `/api/calibration/add` | POST | Add a point `{level_cm, volume_l}` |
| `/api/calibration/remove` | POST | Remove a point `{index}` |
| `/api/calibration/reset` | POST | Reset to linear |
| `/api/wifi/scan` | GET | Scan nearby WiFi |
| `/api/restart` | POST | Restart the ESP |
| `/update` | GET/POST | OTA firmware upload |

## Calibration procedure

1. Fill the tank (after heavy rain)
2. Open the Dashboard — record the level (e.g. 165 cm) + volume estimate (5300 L)
3. Calibration tab → "Use current" (fills in the level) + enter volume 5300, click Add
4. Run the pump, pump out about 500 L (watch `flowmeter/volume_total` in MQTT)
5. The level dropped, e.g. to 150 cm → add a point (150 cm, 4800 L)
6. Continue in 500 L steps until nearly empty
7. After pumping, the calibration will be very accurate

## Troubleshooting

**ESP can't connect to WiFi:**
- Check the Serial Monitor (115200 baud) — you'll see the error
- Connect to the "WaterMonitor-Setup" AP and open http://192.168.4.1
- Verify the SSID/password in Configuration

**MQTT won't connect:**
- Check the broker IP, port (default 1883), credentials
- Test from a PC: `mosquitto_sub -h <broker-ip> -t '#' -v -u user -P password`

**Ultrasonic not measuring (Tank: no valid measurement):**
- Check the D5/D6 wiring + the 5V→3.3V divider on ECHO
- Check the probe power (5V + GND)
- Level > 2 m? Increase `MAX_DISTANCE_CM` in config.h

**Flow meter shows 0 even with water flowing:**
- Check the optocoupler wiring (does the LED light up when the sensor outputs HIGH?)
- Watch the Serial Monitor to see if pulses are arriving
- The signal may be inverted — change `FALLING` to `RISING` in `sensors.cpp`

## Tips

- **Backup config.json**: Download with `wget http://<ip>/config.json` (directly from LittleFS)
- **Watchdog**: The ESP8266 has a SW watchdog that restarts on a hang > 8s
- **Stability**: if you have dropouts, add a 470µF capacitor in the power supply near the Wemos
- **Security**: The WiFi password and MQTT password are stored in **plain text** in LittleFS. The "WaterMonitor-Setup" AP is open — if you have neighbors, you can set AP_PASSWORD in config.h

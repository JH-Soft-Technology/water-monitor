# 💧 Water Monitor

Water flow and tank level monitoring for a home rainwater tank, built on the **ESP8266 (Wemos D1 Mini)**. It measures the current water flow from a pump and the level in an underground tank, then publishes the data over **MQTT** with automatic **Home Assistant** integration. Includes a web interface for configuration, calibration, and OTA updates.

🌐 **[Česká verze README](README.cs.md)**

![Platform](https://img.shields.io/badge/platform-ESP8266-blue)
![Framework](https://img.shields.io/badge/framework-Arduino-teal)
![License](https://img.shields.io/badge/license-MIT-green)

---

## 📸 Screenshots

> Open `docs/web_ui_mockup.html` in a browser to generate screenshots, then add them here. See `docs/SCREENSHOTS.md`.

| Web interface | Wiring (fritzing-style) |
|---|---|
| `docs/web_ui.png` | `docs/wiring.png` |

---

## ✨ Features

- **Flow measurement** using a DN32 pulse flow meter (characteristic `F = 4.5 × Q`)
- **Level measurement** with a JSN-SR04T ultrasonic sensor, median filtering, and temperature correction
- **Calibration table** with linear interpolation — accurate level-to-volume conversion even for irregular tank shapes
- **MQTT publishing** + **Home Assistant auto-discovery** (the device appears automatically)
- **Web interface** for WiFi/MQTT configuration, calibration management, and OTA updates — no re-flashing needed
- **Dual WiFi mode** (AP + STA simultaneously) — setup is available even without a home network
- **Persistent storage** (LittleFS) — configuration and cumulative volume survive reboots
- **Galvanic isolation** of the signal (optocoupler) for reliable operation over a long cable in a noisy environment

---

## 📋 Repository contents

```
water-monitor/
├── firmware/
│   └── water_monitor/        # Arduino sketch (open water_monitor.ino)
│       ├── water_monitor.ino # Main setup/loop
│       ├── config.h          # Constants (pins, intervals)
│       ├── config_storage.*  # Persistent configuration (LittleFS)
│       ├── calibration.*     # Calibration table
│       ├── sensors.*         # Flow meter + ultrasonic
│       ├── mqtt_handler.*    # MQTT + HA discovery
│       ├── web_server.*      # Web UI + REST API + OTA
│       └── data/             # Web assets (upload to LittleFS)
│           ├── index.html
│           ├── style.css
│           └── app.js
├── homeassistant/
│   ├── dashboard.yaml        # Lovelace dashboard
│   ├── helpers.yaml          # Utility meters + template sensors
│   ├── automations.yaml      # Notifications (low level, leaks)
│   └── README.md             # HA integration guide
├── docs/
│   ├── wiring_diagram.html   # Schematic wiring diagram
│   └── wiring_fritzing.html  # Pictorial (fritzing-style) diagram
├── README.md                 # This file
├── README.cs.md              # Czech version
├── LICENSE
└── .gitignore
```

---

## 🔧 Hardware

### Components

| Component | Description | Approx. price |
|---|---|---|
| Wemos D1 Mini | ESP8266 development board | ~$4 |
| DN32 flow meter | Pulse, 1–120 L/min, `F = 4.5 × Q` | ~$11 |
| JSN-SR04T | Ultrasonic distance sensor, IP67 probe | ~$7 |
| PC817 optocoupler (module) | Galvanic signal isolation | ~$2 |
| Resistors 10 kΩ + 20 kΩ | Voltage divider for ECHO pin (5 V → 3.3 V) | ~$0.5 |
| Resistor 1 kΩ | Series resistor on flow meter output | ~$0.1 |
| Capacitor 100 nF | Power supply stabilization at the sensor | ~$0.1 |
| Capacitor 100 nF X2 (275 V AC) | Pump noise suppression | ~$1.5 |
| Ferrite core | On the pump's power cable | ~$2 |

### Pin wiring (Wemos D1 Mini)

| Pin | GPIO | Function | Connection |
|---|---|---|---|
| `D2` | GPIO4 | Flow meter input | PC817 optocoupler OUT |
| `D5` | GPIO14 | Ultrasonic TRIG | JSN-SR04T TRIG (direct) |
| `D6` | GPIO12 | Ultrasonic ECHO | ECHO via 10k+20k divider |
| `5V` | — | 5 V power | Sensor VCC |
| `GND` | — | Common ground | GND of everything |

> ⚠️ **The ECHO pin MUST go through a voltage divider!** The JSN-SR04T outputs 5 V, while the ESP8266 tolerates max 3.3 V.

Detailed schematics are in `docs/wiring_diagram.html` (schematic) and `docs/wiring_fritzing.html` (pictorial).

---

## 🚀 Quick start

### 1. Arduino IDE

Install the **ESP8266 board package** (via Board Manager):
```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Select the board: **Tools → Board → ESP8266 → LOLIN(WEMOS) D1 R2 & mini**

### 2. Libraries (Library Manager)

- [PubSubClient](https://github.com/knolleary/pubsubclient) (Nick O'Leary)
- [ArduinoJson](https://arduinojson.org/) ≥ 6.x (Benoit Blanchon)
- [NewPing](https://bitbucket.org/teckel12/arduino-new-ping/) (Tim Eckel)

### 3. LittleFS upload plugin

Download [arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload) and install it per the instructions (needed to upload the web interface into the filesystem).

### 4. Flashing

```
1. Open firmware/water_monitor/water_monitor.ino
2. Sketch → Upload (flashes firmware)
3. Ctrl+Shift+P → "Upload LittleFS to Pico/ESP8266/ESP32" (uploads web UI)
```

### 5. First-time setup

1. The ESP creates a WiFi network **`WaterMonitor-Setup`** (open network)
2. Connect and open `http://192.168.4.1`
3. In the **Configuration** tab, enter your home WiFi + MQTT broker
4. Restart (System tab)
5. The ESP connects to your home network; proceed with calibration

Detailed guide: [`firmware/water_monitor` README](firmware/water_monitor/README.md)

---

## 📐 Calibration

For accurate level-to-volume conversion (important for irregular tank shapes), a calibration table is used. The easiest method **uses your own flow meter**:

1. Fill the tank (after rain), record the level and max volume as the first point
2. Pump out a known amount (watch `volume_total`)
3. Record the new level → add a point (level, remaining volume)
4. Repeat in ~500 L steps

One drain cycle yields 10+ calibration points and ±1–2% accuracy. Points are added directly in the web interface (Calibration tab).

---

## 🏠 Home Assistant

After the first MQTT connection, the device **appears automatically** in HA as a single device with 8 sensors (flow, volumes, level, fill %…).

The repository also includes:
- **Dashboard** (`homeassistant/dashboard.yaml`) — tank gauge, flow/level charts, consumption statistics
- **Helpers** (`homeassistant/helpers.yaml`) — daily/weekly/monthly consumption, estimated days remaining
- **Automations** (`homeassistant/automations.yaml`) — notifications for low level, abnormal consumption, ESP offline

Guide: [`homeassistant/README.md`](homeassistant/README.md)

---

## 📡 MQTT Topics

Topics are prefixed with the `device_id` value (default `watertank`):

| Topic | Interval | Value |
|---|---|---|
| `watertank/flowmeter/flow_lpm` | 1 s | current flow [L/min] |
| `watertank/flowmeter/volume_minute` | 60 s | volume in the last minute [L] |
| `watertank/flowmeter/volume_hour` | 1 h | volume in the last hour [L] |
| `watertank/flowmeter/volume_total` | 60 s | cumulative volume since start [L] |
| `watertank/tank/distance_cm` | 60 s | sensor-to-surface distance [cm] |
| `watertank/tank/level_cm` | 60 s | water level from the bottom [cm] |
| `watertank/tank/volume_liters` | 60 s | volume in the tank [L] |
| `watertank/tank/fill_percent` | 60 s | fill level [%] |
| `watertank/status` | LWT | online / offline |

---

## 🌐 REST API

The web server on the ESP provides a REST API:

| Endpoint | Method | Description |
|---|---|---|
| `/api/status` | GET | Live data + system info |
| `/api/config` | GET / POST | Read / write configuration |
| `/api/calibration` | GET | Calibration points |
| `/api/calibration/add` | POST | Add a point |
| `/api/calibration/remove` | POST | Remove a point |
| `/api/calibration/reset` | POST | Reset to linear |
| `/api/wifi/scan` | GET | Scan nearby WiFi |
| `/api/restart` | POST | Restart the ESP |
| `/update` | GET / POST | OTA firmware upload |

---

## ⚙️ Measurement principle

### Flow

The flow meter generates pulses at a frequency proportional to the flow (`F = 4.5 × Q`). This means **one pulse corresponds to a constant volume**:

```
1 pulse = 1 / (4.5 × 60) liter = 1/270 L ≈ 3.704 ml
```

Volume is therefore calculated by simply counting pulses, independent of time — clock drift does not affect the accuracy of the total.

### Level

The ultrasonic sensor measures the distance to the surface. Water level = distance when empty − current distance. Volume is derived by interpolating the calibration table. The measurement is median-filtered (against ripples/foam) and corrected for the speed of sound at the given temperature.

---

## ⚠️ Safety notes

- Pump noise suppression and galvanic signal isolation are **recommended** for longer cables routed alongside power wiring
- WiFi and MQTT passwords are stored in LittleFS as **plain text** — operate the device on a trusted local network
- Work with mains voltage (the noise-suppression capacitor at the pump) **must only be performed by a qualified person**

---

## 📄 License

This project is licensed under the [MIT License](LICENSE).

---

## 🤝 Contributing

Pull requests and issues are welcome. The project was created for a specific installation (Atlantis 5300 L rainwater tank + Gardena 4700/2 pump), but it is designed to be easily adapted to other tanks and sensors via configuration and calibration.

## 🙏 Acknowledgments

- [ESP8266 Arduino Core](https://github.com/esp8266/Arduino)
- PubSubClient, ArduinoJson, NewPing libraries
- Home Assistant and its MQTT discovery

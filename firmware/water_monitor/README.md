# Water Monitor v2.0

Kompletní řešení monitoringu vody s ESP8266 (Wemos D1 Mini):
- Měření průtoku (DN32 + optočlen PC817)
- Měření hladiny v nádrži (JSN-SR04T)
- MQTT publikace + Home Assistant auto-discovery
- Webové UI pro konfiguraci, kalibraci a OTA update
- Duální WiFi režim (AP + STA současně)
- LittleFS perzistentní úložiště

## Struktura projektu

```
water_monitor/
├── water_monitor.ino    # Hlavní setup/loop
├── config.h             # Konstanty (HW piny, intervaly)
├── config_storage.{h,cpp}  # Persistentní konfigurace
├── calibration.{h,cpp}  # Kalibrační tabulka
├── sensors.{h,cpp}      # Průtokoměr + ultrazvuk
├── mqtt_handler.{h,cpp} # MQTT + HA discovery
├── web_server.{h,cpp}   # Webové UI + OTA + REST API
└── data/                # Webové assets (nahrávají se do LittleFS)
    ├── index.html
    ├── style.css
    └── app.js
```

## Instalace v Arduino IDE

### 1. ESP8266 board manager
Nastavení → Další URL adresy:
```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```
Tools → Board → Board Manager → vyhledej `esp8266` → nainstaluj **esp8266 by ESP8266 Community** (verze 3.1.2 nebo novější)

Tools → Board → ESP8266 → **LOLIN(WEMOS) D1 R2 & mini**

### 2. Knihovny
Tools → Manage Libraries → nainstaluj:
- **PubSubClient** (Nick O'Leary) ≥ 2.8
- **ArduinoJson** (Benoit Blanchon) ≥ 6.21
- **NewPing** (Tim Eckel) ≥ 1.9

ESP8266WiFi, ESP8266WebServer, ESP8266HTTPUpdateServer, LittleFS jsou součástí board package.

### 3. Plugin pro LittleFS upload (kritické!)
Stáhni: https://github.com/earlephilhower/arduino-littlefs-upload/releases
- Stáhni `.vsix` soubor
- Pro Arduino IDE 2.x: rozbal do `~/.arduinoIDE/plugins/` (Linux/Mac) nebo `%LOCALAPPDATA%\Arduino IDE\plugins\` (Windows)
- Restart IDE

V IDE Ctrl+Shift+P → "Upload LittleFS to Pico/ESP8266/ESP32" - tento příkaz nahraje obsah složky `data/` do filesystému ESP.

## První spuštění

1. **Nahrát firmware**: Sketch → Upload (Ctrl+U)
2. **Nahrát LittleFS data**: Ctrl+Shift+P → "Upload LittleFS to..."
3. **Otevři Serial Monitor** (115200 baud) - uvidíš startup logy
4. ESP se spustí v duálním režimu:
   - AP "WaterMonitor-Setup" (vždy dostupný)
   - STA - pokud je WiFi nakonfigurováno
5. **Připoj se na AP** (otevřená síť), otevři http://192.168.4.1
6. **Záložka Konfigurace**: zadej domácí WiFi + MQTT broker, ulož
7. **Restartuj** (záložka Systém)
8. ESP se připojí k domácí WiFi, IP najdeš v Serial Monitoru nebo routeru
9. Pokračuj na nastavení **Kalibrace**

## Webové UI - co kde najdeš

### 📊 Dashboard
- Live průtok L/min + celkový objem
- Vizualizace nádrže (sloupec s úrovní)
- Hladina, objem, vzdálenost senzoru

### ⚙️ Konfigurace
- WiFi (SSID + heslo) - tlačítko Najít nascanuje okolní sítě
- MQTT broker (server, port, login)
- Název zařízení v HA + ID (prefix MQTT topic)
- Geometrie nádrže (vzdálenost při plné/prázdné, max objem)

### 📏 Kalibrace
- Aktuální hladina + vypočtený objem
- Přidání nového bodu (hladina ↔ objem)
- Tabulka existujících bodů s tlačítkem smazat
- Reset na lineární aproximaci

### 🔧 Systém
- Verze, uptime, free heap
- IP adresy (STA + AP), WiFi signál
- Link na **/update** pro OTA flashing
- Restart ESP

## OTA Update

1. Build firmware: Sketch → Export compiled binary (vytvoří `.bin` v adresáři projektu)
2. Otevři `http://<ip>/update`
3. Vyber soubor `.bin`, klikni Update
4. ESP se restartuje s novým firmwarem

**Pozor**: Při OTA update se NEnahrávají soubory v `data/`. Pro web UI změny musíš použít LittleFS upload (přes USB).

## MQTT topics

Topics jsou prefixované hodnotou `device_id` (default `watertank`):

| Topic | Interval | Hodnota |
|---|---|---|
| `watertank/flowmeter/flow_lpm` | 1s | aktuální průtok L/min |
| `watertank/flowmeter/volume_minute` | 60s | objem za poslední minutu |
| `watertank/flowmeter/volume_hour` | 1h | objem za poslední hodinu |
| `watertank/flowmeter/volume_total` | 60s | kumulativní objem od startu |
| `watertank/tank/distance_cm` | 60s | vzdálenost senzor → hladina |
| `watertank/tank/level_cm` | 60s | výška hladiny od dna |
| `watertank/tank/volume_liters` | 60s | objem v nádrži |
| `watertank/tank/fill_percent` | 60s | naplnění v % |
| `watertank/status` | LWT | online/offline |

## Home Assistant

Po prvním MQTT připojení se zařízení **automaticky objeví v HA** jako jedno zařízení "Water Tank" s 8 senzory. Žádná manuální konfigurace v HA není potřeba (jen MQTT integraci musíš mít aktivní).

## REST API

| Endpoint | Metoda | Popis |
|---|---|---|
| `/api/status` | GET | Live data + system info |
| `/api/config` | GET | Současná konfigurace (hesla skryta) |
| `/api/config` | POST | Aktualizace konfigurace (JSON body) |
| `/api/calibration` | GET | Kalibrační body |
| `/api/calibration/add` | POST | Přidat bod `{level_cm, volume_l}` |
| `/api/calibration/remove` | POST | Smazat bod `{index}` |
| `/api/calibration/reset` | POST | Reset na lineární |
| `/api/wifi/scan` | GET | Sken okolních WiFi |
| `/api/restart` | POST | Restart ESP |
| `/update` | GET/POST | OTA firmware upload |

## Postup kalibrace

1. Naplň nádrž (po vydatném dešti)
2. Otevři Dashboard - zaznamenej hladinu (např. 165 cm) + odhad objemu (5300 L)
3. Záložka Kalibrace → "Použít aktuální" (vyplní hladinu) + zadej objem 5300, klikni Přidat
4. Spusť čerpadlo, vyčerpej cca 500 L (sleduj `flowmeter/volume_total` v MQTT)
5. Hladina klesla, např. na 150 cm → přidej bod (150 cm, 4800 L)
6. Pokračuj po 500 L krocích až do téměř prázdné
7. Po napumpování bude kalibrace velmi přesná

## Troubleshooting

**ESP nelze připojit k WiFi:**
- Zkontroluj Serial Monitor (115200 baud) - uvidíš chybu
- Připoj se na "WaterMonitor-Setup" AP a otevři http://192.168.4.1
- Zkontroluj SSID/heslo v Konfiguraci

**MQTT se nepřipojí:**
- Zkontroluj IP brokeru, port (default 1883), credentials
- Z PC otestuj: `mosquitto_sub -h <broker-ip> -t '#' -v -u user -P password`

**Ultrazvuk neměří (Tank: žádné platné měření):**
- Zkontroluj zapojení D5/D6 + dělič 5V→3.3V na ECHO
- Zkontroluj napájení sondy (5V + GND)
- Hladina > 2 m? Zvyš `MAX_DISTANCE_CM` v config.h

**Průtokoměr ukazuje 0 i při tekoucí vodě:**
- Zkontroluj zapojení optočlenu (LED svítí když senzor dává HIGH?)
- V Serial Monitoru sleduj zda přicházejí pulzy
- Možná je signál invertovaný - změň `FALLING` na `RISING` v `sensors.cpp`

## Tipy

- **Záloha config.json**: Stáhni pomocí `wget http://<ip>/config.json` (přímo z LittleFS)
- **Watchdog**: ESP8266 má SW watchdog, restartuje se při zablokování > 8s
- **Stabilita**: pokud máš výpadky, dej do napájení 470µF kondenzátor blízko Wemos
- **Bezpečnost**: WiFi heslo a MQTT heslo jsou uložená **plain text** v LittleFS. AP režim "WaterMonitor-Setup" je otevřený - pokud máš sousedy, můžeš nastavit AP_PASSWORD v config.h

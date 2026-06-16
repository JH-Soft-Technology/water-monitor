# 💧 Water Monitor

Monitoring průtoku a hladiny vody pro domácí dešťovou nádrž, postavený na **ESP8266 (Wemos D1 Mini)**. Měří aktuální průtok vody z čerpadla, stav hladiny v podzemní nádrži, a publikuje data přes **MQTT** s automatickou integrací do **Home Assistant**. Obsahuje webové rozhraní pro konfiguraci, kalibraci a OTA aktualizace.

🌐 **[English version](README.en.md)**

![Platform](https://img.shields.io/badge/platform-ESP8266-blue)
![Framework](https://img.shields.io/badge/framework-Arduino-teal)
![License](https://img.shields.io/badge/license-MIT-green)

---

## ✨ Funkce

- **Měření průtoku** pomocí pulzního průtokoměru DN32 (charakteristika `F = 4.5 × Q`)
- **Měření hladiny** ultrazvukovým senzorem JSN-SR04T s mediánovým filtrem a teplotní korekcí
- **Kalibrační tabulka** s lineární interpolací — přesný přepočet hladiny na objem i pro nepravidelné tvary nádrží
- **MQTT publikace** + **Home Assistant auto-discovery** (zařízení se objeví automaticky)
- **Webové rozhraní** pro konfiguraci WiFi/MQTT, správu kalibrace a OTA update — bez nutnosti překompilování
- **Duální WiFi režim** (AP + STA současně) — setup je dostupný i bez domácí sítě
- **Perzistentní úložiště** (LittleFS) — konfigurace a kumulativní objem přežijí restart
- **Galvanické oddělení** signálu (optočlen) pro spolehlivý provoz na dlouhém kabelu v rušivém prostředí

---

## 📋 Obsah repozitáře

```
water-monitor/
├── firmware/
│   └── water_monitor/        # Arduino sketch (otevři water_monitor.ino)
│       ├── water_monitor.ino # Hlavní setup/loop
│       ├── config.h          # Konstanty (piny, intervaly)
│       ├── config_storage.*  # Perzistentní konfigurace (LittleFS)
│       ├── calibration.*     # Kalibrační tabulka
│       ├── sensors.*         # Průtokoměr + ultrazvuk
│       ├── mqtt_handler.*    # MQTT + HA discovery
│       ├── web_server.*      # Webové UI + REST API + OTA
│       └── data/             # Web assets (nahrát do LittleFS)
│           ├── index.html
│           ├── style.css
│           └── app.js
├── homeassistant/
│   ├── dashboard.yaml        # Lovelace dashboard
│   ├── helpers.yaml          # Utility meters + template sensors
│   ├── automations.yaml      # Notifikace (nízká hladina, úniky)
│   └── README.md             # Návod na HA integraci
├── docs/
│   ├── wiring_diagram.html   # Schématický wiring diagram
│   └── wiring_fritzing.html  # Pohledový (fritzing-style) diagram
├── README.md                 # Tento soubor
├── LICENSE
└── .gitignore
```

---

## 🔧 Hardware

### Komponenty

| Komponenta | Popis | Orientační cena |
|---|---|---|
| Wemos D1 Mini | ESP8266 vývojová deska | ~80 Kč |
| **DC Power Shield** (7-24 V) | Sendvičový shield s integrovaným step-downem pro Wemos | ~50 Kč |
| **12 V / 1 A spínaný zdroj** | Jediný napájecí zdroj pro celý systém | ~120 Kč |
| Průtokoměr DN32 | Pulzní, 1–120 l/min, `F = 4.5 × Q` (napájen 12 V) | ~250 Kč |
| JSN-SR04T | Ultrazvukový senzor vzdálenosti, sonda IP67 | ~150 Kč |
| Optočlen PC817 (modul) | Galvanické oddělení signálu | ~50 Kč |
| LM2596 step-down (12 V → 5 V) | Lokální napájení JSN-SR04T v krabičce (klasický modul s displejem) | ~50 Kč |
| Rezistor 1 kΩ | Sériový na výstup průtokoměru | ~1 Kč |
| Kondenzátor 100 nF | Stabilizace napájení u senzoru | ~2 Kč |
| Kondenzátor 100 nF X2 (275 V AC) | Odrušení čerpadla | ~30 Kč |
| Ferritové jádro | Na silový kabel čerpadla | ~40 Kč |

> 💡 **Architektura v2.1 (prosinec 2025):** Jediný 12 V zdroj napájí všechno. Wemos je napájen přes DC Power Shield (vstup 7-24 V), senzor DN32 jede přímo na 12 V (optimální proud LED v PC817) a JSN-SR04T má lokální 5 V napájení přes LM2596 step-down modul v krabičce. Oproti v2.0 odpadá externí step-down pro Wemos, dělič napětí 10k+20k pro ECHO a duální napájení (USB + ?).

### Zapojení pinů (Wemos D1 Mini)

| Pin | GPIO | Funkce | Připojení |
|---|---|---|---|
| `+` svorka shieldu | — | 12 V vstup | Hlavní 12 V zdroj |
| `−` svorka shieldu | — | GND | Hlavní zdroj + UTP pár GR |
| `D2` | GPIO4 | Vstup průtokoměru | PC817 optočlen V1 |
| `D5` | GPIO14 | TRIG ultrazvuku | JSN-SR04T TRIG přes UTP BR |
| `D6` | GPIO12 | ECHO ultrazvuku | JSN-SR04T ECHO přes UTP BR/B (přímo, bez děliče) |
| `5V` | — | 5 V výstup (ze shieldu) | rezerva — externě se nepoužívá |
| `GND` | — | Společná zem | PC817 G (výstup), JSN-SR04T GND |

> ✅ **Bez děliče napětí na ECHO** — JSN-SR04T je teď napájen 5 V (ne 12 V), takže ECHO signál je max 5 V, což je v toleranci ESP8266 GPIO.

Podrobné schéma najdeš v `docs/wiring_diagram.html` (schématické) a `docs/wiring_fritzing.html` (pohledové).

---

## 🚀 Rychlý start

### 1. Arduino IDE

Nainstaluj **ESP8266 board package** (přes Board Manager):
```
https://arduino.esp8266.com/stable/package_esp8266com_index.json
```

Vyber desku: **Tools → Board → ESP8266 → LOLIN(WEMOS) D1 R2 & mini**

### 2. Knihovny (Library Manager)

- [PubSubClient](https://github.com/knolleary/pubsubclient) (Nick O'Leary)
- [ArduinoJson](https://arduinojson.org/) ≥ 6.x (Benoit Blanchon)
- [NewPing](https://bitbucket.org/teckel12/arduino-new-ping/) (Tim Eckel)

### 3. Plugin pro LittleFS upload

Stáhni [arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload) a nainstaluj podle návodu (potřeba pro nahrání webového rozhraní do filesystému).

### 4. Nahrání

```
1. Otevři firmware/water_monitor/water_monitor.ino
2. Sketch → Upload (nahraje firmware)
3. Ctrl+Shift+P → "Upload LittleFS to Pico/ESP8266/ESP32" (nahraje web UI)
```

### 5. První konfigurace

1. ESP vytvoří WiFi **`WaterMonitor-Setup`** (otevřená síť)
2. Připoj se a otevři `http://192.168.4.1`
3. V záložce **Konfigurace** zadej domácí WiFi + MQTT broker
4. Restartuj (záložka Systém)
5. ESP se připojí do domácí sítě; pokračuj kalibrací

Detailní návod: [`firmware/water_monitor` README](firmware/water_monitor/README.cs.md)

---

## 📐 Kalibrace

Pro přesný přepočet hladiny na objem (důležité u nepravidelných tvarů nádrží) slouží kalibrační tabulka. Nejjednodušší metoda **využívá vlastní průtokoměr**:

1. Naplň nádrž (po dešti), zaznamenej hladinu a max. objem jako první bod
2. Čerpadlem odeber známé množství (sleduj `volume_total`)
3. Zaznamenej novou hladinu → přidej bod (hladina, zbývající objem)
4. Opakuj po ~500 l krocích

Jeden vyčerpávací cyklus tak dá 10+ kalibračních bodů a přesnost ±1–2 %. Body se přidávají přímo ve webovém rozhraní (záložka Kalibrace).

---

## 🏠 Home Assistant

Po prvním MQTT připojení se zařízení **automaticky objeví** v HA jako jedno zařízení s 8 senzory (průtok, objemy, hladina, naplnění…).

Repozitář navíc obsahuje:
- **Dashboard** (`homeassistant/dashboard.yaml`) — gauge nádrže, grafy průtoku/hladiny, statistiky spotřeby
- **Helpers** (`homeassistant/helpers.yaml`) — denní/týdenní/měsíční spotřeba, odhad zbývajících dnů
- **Automatizace** (`homeassistant/automations.yaml`) — notifikace na nízkou hladinu, abnormální spotřebu, offline ESP

Návod: [`homeassistant/README.md`](homeassistant/README.cs.md)

---

## 📡 MQTT Topics

Topics jsou prefixované hodnotou `device_id` (výchozí `watertank`):

| Topic | Interval | Hodnota |
|---|---|---|
| `watertank/flowmeter/flow_lpm` | 1 s | aktuální průtok [l/min] |
| `watertank/flowmeter/volume_minute` | 60 s | objem za poslední minutu [l] |
| `watertank/flowmeter/volume_hour` | 1 h | objem za poslední hodinu [l] |
| `watertank/flowmeter/volume_total` | 60 s | kumulativní objem od startu [l] |
| `watertank/tank/distance_cm` | 60 s | vzdálenost senzor → hladina [cm] |
| `watertank/tank/level_cm` | 60 s | výška hladiny od dna [cm] |
| `watertank/tank/volume_liters` | 60 s | objem v nádrži [l] |
| `watertank/tank/fill_percent` | 60 s | naplnění [%] |
| `watertank/status` | LWT | online / offline |

---

## 🌐 REST API

Webový server na ESP nabízí REST API:

| Endpoint | Metoda | Popis |
|---|---|---|
| `/api/status` | GET | Live data + system info |
| `/api/config` | GET / POST | Čtení / zápis konfigurace |
| `/api/calibration` | GET | Kalibrační body |
| `/api/calibration/add` | POST | Přidat bod |
| `/api/calibration/remove` | POST | Smazat bod |
| `/api/calibration/reset` | POST | Reset na lineární |
| `/api/wifi/scan` | GET | Sken okolních WiFi |
| `/api/restart` | POST | Restart ESP |
| `/update` | GET / POST | OTA firmware upload |

---

## ⚙️ Princip měření

### Průtok

Průtokoměr generuje pulzy s frekvencí úměrnou průtoku (`F = 4.5 × Q`). Z toho plyne, že **jeden pulz odpovídá konstantnímu objemu**:

```
1 pulz = 1 / (4.5 × 60) litru = 1/270 l ≈ 3.704 ml
```

Objem se tedy počítá prostým čítáním pulzů, nezávisle na čase — drift hodin neovlivní přesnost součtu.

### Hladina

Ultrazvukový senzor měří vzdálenost k hladině. Výška hladiny = vzdálenost při prázdné nádrži − aktuální vzdálenost. Objem se dopočítá interpolací kalibrační tabulky. Měření je filtrované mediánem (proti vlnkám/pěně) a korigované na rychlost zvuku při dané teplotě.

---

## ⚠️ Poznámky k bezpečnosti

- Odrušení čerpadla a galvanické oddělení signálu jsou **doporučené** při delším kabelu vedeném souběžně se silovým vedením
- WiFi a MQTT hesla jsou v LittleFS uložena jako **plain text** — zařízení provozuj v důvěryhodné lokální síti
- Práce se síťovým napětím (odrušovací kondenzátor u čerpadla) **smí provádět jen osoba s odpovídající kvalifikací**

---

## 📄 Licence

Tento projekt je licencován pod [MIT License](LICENSE).

---

## 🤝 Příspěvky

Pull requesty a issue jsou vítány. Projekt vznikl pro konkrétní instalaci (dešťová nádrž Atlantis 5300 l + čerpadlo Gardena 4700/2), ale je navržen tak, aby šel snadno přizpůsobit jiným nádržím a senzorům přes konfiguraci a kalibraci.

## 🙏 Poděkování

- [ESP8266 Arduino Core](https://github.com/esp8266/Arduino)
- Knihovny PubSubClient, ArduinoJson, NewPing
- Home Assistant a jeho MQTT discovery

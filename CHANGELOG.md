# Changelog

Všechny významné změny v tomto projektu jsou dokumentovány zde.

Formát vychází z [Keep a Changelog](https://keepachangelog.com/),
projekt používá [sémantické verzování](https://semver.org/lang/cs/).

## [2.1.0] - 2025

### Změněno
- **Architektura napájení** přepracována na jediný 12V zdroj místo USB
- Wemos D1 Mini napájen přes **DC Power Shield** (vstup 7-24V) — integrovaný step-down
- Senzor DN32 napájen **12V** místo 5V → optimální proud LED v modulu PC817 (~3,6 mA místo ~1,3 mA)
- JSN-SR04T napájen 5V z výstupu Power Shieldu

### Odebráno
- Externí step-down LM2596 (Power Shield to dělá integrovaně)
- Dělič napětí 10k+20k pro ECHO pin (signál ECHO z JSN-SR04T napájeného 5V je max 5V → bezpečné pro ESP8266 GPIO)

### Přidáno
- Mini step-down 12V→5V v krabičce u ultrazvuku (lokální napájení JSN-SR04T)
- Aktualizované wiring diagramy (`docs/wiring_diagram.html`, `docs/wiring_fritzing.html`)
- Aktualizovaná tabulka komponent v README a komentáře v firmware

### Pozn. ke kompatibilitě
- Firmware kód se **nemění** (piny D2/D5/D6 zůstávají stejné, kalibrace, MQTT topics, vše ostatní beze změny)
- Změna je čistě **hardwarová** — kdo má funkční v2.0 setup, nemusí firmware reflashnout
- Pokud staví nový od základu, doporučuji v2.1 architekturu (méně dílů, jeden zdroj, vyšší spolehlivost)

## [2.0.0] - 2025

### Přidáno
- Webové rozhraní pro konfiguraci (WiFi, MQTT, geometrie nádrže)
- Správa kalibrační tabulky přes web UI (přidávání/mazání bodů)
- OTA aktualizace firmwaru přes webové rozhraní (`/update`)
- Duální WiFi režim (AP + STA současně) — setup vždy dostupný
- Perzistentní úložiště konfigurace a kalibrace (LittleFS)
- Perzistence kumulativního objemu (přežije restart)
- REST API pro všechny funkce
- Home Assistant dashboard, helpers a automatizace
- Wiring diagramy (schématický + fritzing-style)

### Změněno
- Refaktor z jednoho `.ino` do modulární struktury (8 souborů)
- Kalibrace přesunuta z konstanty v kódu do LittleFS

## [1.0.0] - 2025

### Přidáno
- Měření průtoku průtokoměrem DN32 (`F = 4.5 × Q`)
- Měření hladiny ultrazvukem JSN-SR04T s mediánovým filtrem
- Teplotní korekce rychlosti zvuku
- Kalibrační tabulka s lineární interpolací (v kódu)
- MQTT publikace
- Home Assistant auto-discovery
- Debouncing pulzů v ISR (odolnost proti rušení na dlouhém kabelu)

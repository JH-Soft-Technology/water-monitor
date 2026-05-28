# Changelog

Všechny významné změny v tomto projektu jsou dokumentovány zde.

Formát vychází z [Keep a Changelog](https://keepachangelog.com/),
projekt používá [sémantické verzování](https://semver.org/lang/cs/).

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

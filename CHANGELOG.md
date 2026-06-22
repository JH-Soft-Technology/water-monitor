# Changelog

Všechny významné změny v tomto projektu jsou dokumentovány zde.

Formát vychází z [Keep a Changelog](https://keepachangelog.com/),
projekt používá [sémantické verzování](https://semver.org/lang/cs/).

## [2.6.0] - 2026

### Přidáno
- **Konfigurovatelný K-faktor průtokoměru** — dříve napevno v `config.h`
  (`K_FACTOR`/`PULSES_PER_LITER`), nově runtime hodnota `pulses_per_liter`
  v `Config` (perzistentní v `config.json`), nastavitelná ve web UI bez reflashe:
  - Konfigurace → **Průtokoměr - kalibrace (K-faktor)**: přímé pole „pulzů/L"
    + kalkulačka „naměřené pulzy + objem [L] → pulzů/L" (např. odčerpat 100 L,
    zadat změřené pulzy a 100 L).
  - `GET/POST /api/config` nově obsahuje `pulses_per_liter` (POST validuje > 0).
  - Výchozí hodnota `DEFAULT_PULSES_PER_LITER = 58.5` (změřeno ~585 pulzů / 10 L
    na DN32; předchozí 270 platilo pro menší senzor a podhodnocovalo objemy ~4,6×).

### Změněno
- **FW_VERSION** `2.5` → `2.6`
- `K_FACTOR` [Hz na L/min] se nově odvozuje jako `pulses_per_liter / 60`.

## [2.5.0] - 2026

### Přidáno
- **Signalizace neplatného měření hladiny** — když ultrazvuk (JSN-SR04T) nevrací
  platné echo (hladina v mrtvé zóně < ~20 cm, výpadek napájení nebo odpojený
  senzor), zařízení už neukazuje zavádějící poslední/nulovou hodnotu:
  - **MQTT / Home Assistant**: nový availability topic `<device_id>/tank/status`
    (`online`/`offline`, retained). Tank senzory (vzdálenost, hladina, objem,
    naplnění) používají `availability_mode: all` přes `<device_id>/status` +
    `tank/status`, takže v HA zešednou na *unavailable* místo zamrzlé hodnoty.
  - **Web UI**: žluté varování v kartě Nádrž + všechny hodnoty nádrže přepnuté
    na `--` (vč. záložky Kalibrace).
  - `mqttPublishTank()` se nově volá i při neúspěšném měření (publikuje `offline`).

### Změněno
- **FW_VERSION** `2.4` → `2.5`

## [2.4.0] - 2026

### Přidáno
- **Backup / restore konfigurace** — ochrana perzistentních dat při flashi
  filesystemu. Web UI a runtime config sdílí jeden LittleFS oddíl, takže
  `uploadfs` / OTA pole `filesystem` jinak smaže `config.json`, `calibration.json`,
  `stats.json` i `history.json`.
  - `GET /api/backup` — stáhne všechny perzistentní soubory jako jeden JSON
    (vč. WiFi/MQTT hesel v plaintextu, na rozdíl od `/api/config`).
  - `POST /api/restore` — zapíše soubory zpět do LittleFS (tělo = výstup z
    `/api/backup`), vrací `restart_required`.
  - Doporučený postup při updatu web UI: `GET /api/backup` → zapéct soubory do
    `data/` před buildem image (zařízení pak zůstane online), nebo flash FS a
    `POST /api/restore` přes AP režim.

### Změněno
- **FW_VERSION** `2.3` → `2.4`

## [2.3.0] - 2026

### Přidáno
- **Period statistics modul** (`period_stats.h/cpp`) — statistika spotřeby vody
  za **den / týden / měsíc / rok** s automatickým resetem dle reálného času.
  Inspirováno modulem v [ha-rain-sensor](https://github.com/JH-Soft-Technology/ha-rain-sensor).
- **NTP synchronizace** (CET/CEST s automatickým DST) — primární `tik.cesnet.cz`,
  fallback `pool.ntp.org`, `time.nist.gov`. POSIX TZ string řeší letní/zimní čas.
- **Skalární akumulátory**: `today_l`, `week_l`, `month_l`, `year_l`.
- **Histogramy pro grafy**: `hourly[24]`, `daily_week[7]` (Po-Ne),
  `daily_month[31]`, `monthly[12]`.
- **Rollover logika** — kontrola každých 30 s, kaskádní reset
  (rok→měsíc→týden→den). Resety jsou perzistentní (přežijí restart).
- **Perzistence v LittleFS**:
  - `/stats.json` — skalární akumulátory + ref. indexy
  - `/history.json` — histogramy (cca 1.5 KB)
- **MQTT publikace nových topics**:
  - `<device_id>/consumption/today` — denní spotřeba [L]
  - `<device_id>/consumption/week` — týdenní spotřeba [L]
  - `<device_id>/consumption/month` — měsíční spotřeba [L]
  - `<device_id>/consumption/year` — roční spotřeba [L]
- **HA auto-discovery** pro 4 nové sensory (mdi:calendar-today/week/month/calendar)
- **Nové REST API endpointy**:
  - `GET /api/stats` — kompletní period stats vč. histogramů
  - `POST /api/stats/reset` — vynulování všech akumulátorů a historie
- **HA dashboard rozšířen** o sekci „Spotřeba (období)" + 4 statistics-graph
  bar charty (den/týden/měsíc/rok).
- **HA helpers.yaml zjednodušeno** — utility_meter sekce odstraněna
  (firmware to teď řídí přímo přes NTP, žádné HA-side workaroundy).

### Změněno
- **FW_VERSION** `2.2` → `2.3`
- **mqtt_handler** rozšířen o `mqttPublishPeriodStats()` (publikuje 4 topics
  jednorázově).
- **water_monitor.ino**: hlavní loop volá `statsLoop()` a `statsAddVolume()`
  z minutového callbacku, periodická publikace každých 60 s.
- **/api/status** vrací navíc objekt `stats` s today/week/month/year.

### Pozn. ke kompatibilitě
- Hardware se **nemění** (v2.1 12V architektura zůstává).
- Po prvním spuštění FW potřebuje NTP odezvu (typicky 5-30 s po WiFi connect)
  pro aktivaci rollover logiky. Před NTP sync se objemy akumulují,
  ale nedistribuují do histogramů (až do okamžiku, kdy NTP odpoví).
- Stávající utility_meter v HA configuration.yaml můžeš odstranit,
  ale není to nutné — entity z firmware mají jiná unique_id.

## [2.2.0] - 2026-06-17

### Změněno
- **Výpočet průtoku přepracován na měření periody** mezi pulzy (kruhový buffer časových razítek posledních 8 pulzů) místo počítání pulzů za 1 s. Spolehlivě měří i nízké průtoky (1–2 pulzy/s), kde předchozí metoda oscilovala mezi 0 a chybnou hodnotou.
- **Interval měření hladiny** zkrácen z 60 s na **30 s** (svěžejší data při napouštění i odčerpávání).
- **OTA stránka** přepracována do designu zbytku UI — drag&drop, indikátor průběhu nahrávání a stavové hlášky, čekání na restart zařízení.

### Přidáno
- **Odhad průtoku z jednoho pulzu** — při velmi pomalém toku se průtok odhadne z doby uplynulé od posledního pulzu (horní hranice).
- **Reset bufferu při výpadku toku** (> 10 s bez pulzu) — stará razítka se nemíchají s novými.
- **Ochrana celkového čítače proti ztrátě:**
  - perzistence **každých 10 min** (max 10 min ztráta při výpadku proudu místo až 60 min),
  - uložení totalu **před plánovaným restartem** (`/api/restart`) a **na začátku OTA** přenosu → plánované reboty a aktualizace firmwaru bez ztráty.

### Opraveno
- Odstraněn diagnostický čítač `flow_raw_interrupts` z `/api/status` — způsoboval souběh (race condition) s výpočtem průtoku a zkresloval debug výpis.

## [2.1.0] - 2025

### Změněno
- **Architektura napájení** přepracována na jediný 12V zdroj místo USB
- Wemos D1 Mini napájen přes **DC Power Shield** (vstup 7-24V) — integrovaný step-down
- Senzor DN32 napájen **12V** místo 5V → optimální proud LED v modulu PC817 (~3,6 mA místo ~1,3 mA)
- JSN-SR04T napájen 5V z výstupu Power Shieldu

### Odebráno
- Externí step-down pro Wemos (Power Shield to dělá integrovaně)
- Dělič napětí 10k+20k pro ECHO pin (signál ECHO z JSN-SR04T napájeného 5V je max 5V → bezpečné pro ESP8266 GPIO)

### Přidáno
- LM2596 step-down 12V→5V v krabičce u ultrazvuku (lokální napájení JSN-SR04T)
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

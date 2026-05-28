# Home Assistant Setup - Water Monitor

Tato složka obsahuje vše pro integraci ESP zařízení do Home Assistant:
- `dashboard.yaml` — Lovelace dashboard
- `helpers.yaml` — utility meters + template sensors  
- `automations.yaml` — automatizace s notifikacemi

🌐 **[English version](README.en.md)**

---

## 1. Příprava

### A) Najdi své entity v HA
Po prvním připojení MQTT z ESP by se v HA měly automaticky objevit entity. Otevři:

**Vývojářské nástroje → Stavy** a vyhledej "watertank" nebo "water". Měl bys vidět 8 entit:

| Entita v HA (typický název) | Co měří |
|---|---|
| `sensor.prutok_aktualni` | aktuální L/min |
| `sensor.objem_za_minutu` | L za poslední minutu |
| `sensor.objem_za_hodinu` | L za poslední hodinu |
| `sensor.objem_celkem` | celkový L od startu (klíčové pro statistiky!) |
| `sensor.hladina_v_nadrzi` | cm hladiny |
| `sensor.naplneni_nadrze` | % naplnění |
| `sensor.objem_v_nadrzi` | L v nádrži |
| `sensor.vzdalenost_senzor` | cm od senzoru |

**Pokud máš jiné názvy** (HA někdy zachovává původní HA discovery jména), poznamenej si je a v YAML soborech ve složce `ha/` proveď find/replace. Např. když máš `sensor.water_tank_flow_lpm`, nahraď `sensor.prutok_aktualni` za tento název.

### B) Najdi název své mobile app notifikace
**Vývojářské nástroje → Akce** → hledej `notify.mobile_app_` → ukáže se ti název tvého telefonu, např. `notify.mobile_app_iphone_jana`.

---

## 2. Helpers a Template senzory

Tyto vytvoří sensory pro denní/měsíční spotřebu a další užitečné věci.

### Krok za krokem:

1. Otevři **soubor `configuration.yaml`** v HA (přes File Editor, Studio Code add-on, SSH, nebo přes Samba)

2. **Vlož celý obsah `helpers.yaml`** na konec `configuration.yaml`

3. Pokud už máš sekci `utility_meter:`, `template:` nebo `sensor:`, **musíš sloučit** (ne duplikovat klíče!). 
   Příklad pro `sensor:`:
   ```yaml
   sensor:
     - platform: existing_thing  # to co už tam máš
       ...
     # NOVÉ z helpers.yaml:
     - platform: statistics
       name: "Průměrný průtok 5 min"
       ...
   ```

4. **Vývojářské nástroje → YAML → Kontrola konfigurace**. Pokud zelená, pokračuj. Pokud červená, oprav syntax.

5. **Vývojářské nástroje → YAML → Restart Home Assistant**

6. Po restartu zkontroluj **Vývojářské nástroje → Stavy**:
   - `sensor.spotreba_dnes` — měla by se objevit (zatím `unknown` nebo `0`)
   - `sensor.spotreba_tento_tyden`
   - `sensor.spotreba_tento_mesic`
   - `sensor.cerpadlo_aktivni`
   - `sensor.zbyvajicich_dnu_vody`

---

## 3. Dashboard

### Krok za krokem:

1. V HA klikni na **Přehled** v levém menu

2. V pravém horním rohu **tři tečky → Upravit dashboard**

3. Vpravo nahoře **tři tečky → Surovinové konfigurace (Raw configuration editor)**

> **Pozor:** Tohle přepíše tvůj aktuální dashboard! Pokud máš důležitý obsah, **udělej si zálohu** (zkopíruj současný YAML jinam).

4. **Vlož celý obsah z `dashboard.yaml`**

5. Klikni **Uložit**

6. Vrať se na Přehled → měl bys vidět novou stránku **Voda** s ikonou kapky

### Alternativa: oddělený dashboard
Pokud nechceš přepsat hlavní dashboard:

1. **Nastavení → Dashboardy → Přidat dashboard → Nový dashboard z koše**
2. Pojmenuj ho např. "Voda", ikona `mdi:water`
3. Klikni **Vytvořit**
4. Otevři tento nový dashboard, **Upravit → tři tečky → Surovinové konfigurace**
5. **Vlož obsah z `dashboard.yaml`**

---

## 4. Automatizace

### Krok za krokem:

1. **Najdi a uprav** v `automations.yaml`:
   - Všechny výskyty `notify.mobile_app_xxx` → nahraď názvem svého telefonu  
     (např. `notify.mobile_app_iphone_jana`)

2. Otevři `automations.yaml` v HA (přes File Editor)

3. **Přidej obsah** ze souboru z `ha/automations.yaml` na konec  
   *Pozor: pokud už soubor obsahuje jiné automatizace, jen přidej tyto navíc, neukončuj `[]` ani nic jiného.*

4. **Vývojářské nástroje → YAML → Reload automations**  
   *(nebo restart HA)*

5. Otevři **Nastavení → Automatizace a scény** a zkontroluj, jestli všech 6 automatizací je vypsaných a zapnutých.

### Test automatizací

Můžeš ručně testovat:
- **Nastavení → Automatizace a scény** → klikni na automatizaci → **Spustit** (spustí akce bez podmínek)

Nebo simuluj trigger (např. dočasně sniž threshold na 90% místo 10% a počkej).

---

## 5. Co dashboard ukazuje

Dashboard má **7 sekcí na jedné stránce** (vestavěné karty Lovelace):

| # | Sekce | Co tam je |
|---|---|---|
| 1 | 💧 **Stav nádrže** | Gauge naplnění %, objem L, hladina cm, vzdálenost cm |
| 2 | 🚰 **Průtok** | Gauge L/min, spotřeba za min/hod, denní/týdenní/měsíční |
| 3 | 📊 **Vizualizace** | Gauge hladiny cm, glance se status hodnotami |
| 4 | 📈 **Průtok 24h** | History graph aktuálního průtoku |
| 5 | 📉 **Hladina 7 dní** | History graph hladiny a objemu |
| 6 | 📊 **Statistika spotřeby** | Bar chart hodinová + denní spotřeba |
| 7 | ⚙️ **Systém** | Online status, linky na web ESP a OTA |

---

## 6. Notifikace - co budeš dostávat

| Automatizace | Kdy se spustí | Co dostaneš |
|---|---|---|
| 🚨 Nízká hladina | < 10% po 5 min | "Nádrž je jen na X %, zbývá Y L" (max 1×/den) |
| ✅ Nádrž plná | > 90% po 10 min | "Nádrž je teď na X %, Y L k dispozici" |
| 🚨 Vysoká spotřeba | 2× větší než 7-denní průměr, po 18:00 | "Dnes spotřebováno X L (průměr Y L), možný únik?" |
| 🚨 Čerpadlo dlouho | běží > 30 min nepřetržitě | "Čerpadlo běží 30+ min, zkontroluj úniky" |
| ❌ ESP offline | offline > 5 min | "ESP je offline, zkontroluj napájení" |
| ✅ ESP online | po offline → online | "ESP je zase online" |

---

## 7. Troubleshooting

### Entity se neobjevují v HA
- Zkontroluj **Nastavení → Zařízení a služby → MQTT** je aktivní
- V HA `Vývojářské nástroje → MQTT → Listen to topic` zadej `watertank/#` a poslouchej, jestli přicházejí data
- Pokud ne, zkontroluj na ESP webovém UI v záložce "Systém", jestli je MQTT connected

### Sensor `spotreba_dnes` ukazuje `unknown` nebo neroste
- Utility meter potřebuje **alespoň 2 hodnoty** ze zdrojového sensoru, aby začal počítat
- Počkej minutu, mělo by se rozjet
- Pokud `sensor.objem_celkem` neexistuje, helper sensor nebude fungovat - zkontroluj název

### Graf historie je prázdný
- Standardní HA recorder uchovává **10 dní historie** by default
- Pro graf 7 dní musíš mít data alespoň několik hodin
- Pokud používáš InfluxDB / další DB, zkontroluj integraci

### Notifikace nepřicházejí na mobil
- Otevři Mobile App na telefonu → Nastavení → Notifikace povoleny?
- Otestuj ručně: **Vývojářské nástroje → Akce → notify.mobile_app_xxx** → Vyplň message → Spustit
- Pokud nepřijde, problém je v Mobile App integraci

### Automatizace spustila, ale ne notifikace
- **Nastavení → Automatizace** → klikni na automatizaci → **Trace** (záznamy spuštění)
- Uvidíš, ve které části selhala (často špatný název notify služby)

---

## 8. Užitečné Lovelace karty pro pokročilé

Pokud později **nainstaluješ HACS** a doplníš pár hezkých karet, můžeš přidat:

- **bar-card** — nádrž jako vertikální vodní sloupec
- **mushroom-cards** — pěkné gauge a entity karty
- **apexcharts-card** — pokročilé grafy spotřeby
- **mini-graph-card** — sparkline grafy

Pokud chceš, můžu ti k tomu udělat upgrade-verzi dashboardu — stačí říct.

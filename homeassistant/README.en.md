# Home Assistant Setup - Water Monitor

This folder contains everything needed to integrate the ESP device into Home Assistant:
- `dashboard.yaml` — Lovelace dashboard
- `helpers.yaml` — utility meters + template sensors
- `automations.yaml` — automations with notifications

🌐 **[Česká verze](README.cs.md)**

---

## 1. Preparation

### A) Find your entities in HA
After the first MQTT connection from the ESP, the entities should appear automatically in HA. Open:

**Developer Tools → States** and search for "watertank" or "water". You should see 8 entities:

| HA entity (typical name) | What it measures |
|---|---|
| `sensor.prutok_aktualni` | current L/min |
| `sensor.objem_za_minutu` | L in the last minute |
| `sensor.objem_za_hodinu` | L in the last hour |
| `sensor.objem_celkem` | total L since start (key for statistics!) |
| `sensor.hladina_v_nadrzi` | level in cm |
| `sensor.naplneni_nadrze` | fill % |
| `sensor.objem_v_nadrzi` | L in the tank |
| `sensor.vzdalenost_senzor` | cm from the sensor |

**If you have different names** (HA sometimes keeps the original HA discovery names), note them and do a find/replace in the YAML files in the `ha/` folder. For example, if you have `sensor.water_tank_flow_lpm`, replace `sensor.prutok_aktualni` with that name.

### B) Find your mobile app notification name
**Developer Tools → Actions** → search for `notify.mobile_app_` → it will show your phone's name, e.g. `notify.mobile_app_iphone_jane`.

---

## 2. Helpers and Template sensors

These create sensors for daily/monthly consumption and other useful values.

### Step by step:

1. Open the **`configuration.yaml`** file in HA (via File Editor, Studio Code add-on, SSH, or Samba)

2. **Paste the entire contents of `helpers.yaml`** at the end of `configuration.yaml`

3. If you already have a `utility_meter:`, `template:`, or `sensor:` section, **you must merge them** (don't duplicate keys!).
   Example for `sensor:`:
   ```yaml
   sensor:
     - platform: existing_thing  # what you already have
       ...
     # NEW from helpers.yaml:
     - platform: statistics
       name: "Average flow 5 min"
       ...
   ```

4. **Developer Tools → YAML → Check Configuration**. If green, continue. If red, fix the syntax.

5. **Developer Tools → YAML → Restart Home Assistant**

6. After the restart, check **Developer Tools → States**:
   - `sensor.spotreba_dnes` — should appear (initially `unknown` or `0`)
   - `sensor.spotreba_tento_tyden`
   - `sensor.spotreba_tento_mesic`
   - `sensor.cerpadlo_aktivni`
   - `sensor.zbyvajicich_dnu_vody`

---

## 3. Dashboard

### Step by step:

1. In HA, click **Overview** in the left menu

2. Top-right corner **three dots → Edit Dashboard**

3. Top-right **three dots → Raw configuration editor**

> **Warning:** This overwrites your current dashboard! If you have important content, **make a backup** (copy the current YAML elsewhere).

4. **Paste the entire contents of `dashboard.yaml`**

5. Click **Save**

6. Return to Overview → you should see a new **Voda** (Water) page with a water-drop icon

### Alternative: separate dashboard
If you don't want to overwrite the main dashboard:

1. **Settings → Dashboards → Add Dashboard → New dashboard from scratch**
2. Name it e.g. "Water", icon `mdi:water`
3. Click **Create**
4. Open this new dashboard, **Edit → three dots → Raw configuration editor**
5. **Paste the contents of `dashboard.yaml`**

---

## 4. Automations

### Step by step:

1. **Find and edit** in `automations.yaml`:
   - All occurrences of `notify.mobile_app_xxx` → replace with your phone's name
     (e.g. `notify.mobile_app_iphone_jane`)

2. Open `automations.yaml` in HA (via File Editor)

3. **Add the contents** from `ha/automations.yaml` to the end
   *Note: if the file already contains other automations, just add these on top, don't close `[]` or anything else.*

4. **Developer Tools → YAML → Reload automations**
   *(or restart HA)*

5. Open **Settings → Automations & scenes** and check that all 6 automations are listed and enabled.

### Testing automations

You can test manually:
- **Settings → Automations & scenes** → click an automation → **Run** (runs the actions without conditions)

Or simulate a trigger (e.g. temporarily lower the threshold to 90% instead of 10% and wait).

---

## 5. What the dashboard shows

The dashboard has **7 sections on one page** (built-in Lovelace cards):

| # | Section | What's there |
|---|---|---|
| 1 | 💧 **Tank status** | Fill % gauge, volume L, level cm, distance cm |
| 2 | 🚰 **Flow** | L/min gauge, consumption per min/hour, daily/weekly/monthly |
| 3 | 📊 **Visualization** | Level gauge cm, glance with status values |
| 4 | 📈 **Flow 24h** | History graph of current flow |
| 5 | 📉 **Level 7 days** | History graph of level and volume |
| 6 | 📊 **Consumption statistics** | Bar chart hourly + daily consumption |
| 7 | ⚙️ **System** | Online status, links to ESP web and OTA |

---

## 6. Notifications - what you'll receive

| Automation | When it triggers | What you get |
|---|---|---|
| 🚨 Low level | < 10% after 5 min | "Tank is only at X%, Y L remaining" (max 1×/day) |
| ✅ Tank full | > 90% after 10 min | "Tank is now at X%, Y L available" |
| 🚨 High consumption | 2× the 7-day average, after 18:00 | "Today you used X L (average Y L), possible leak?" |
| 🚨 Pump running long | running > 30 min continuously | "Pump running 30+ min, check for leaks" |
| ❌ ESP offline | offline > 5 min | "ESP is offline, check power" |
| ✅ ESP online | after offline → online | "ESP is back online" |

---

## 7. Troubleshooting

### Entities don't appear in HA
- Check that **Settings → Devices & Services → MQTT** is active
- In HA `Developer Tools → MQTT → Listen to topic` enter `watertank/#` and listen for incoming data
- If nothing arrives, check on the ESP web UI in the "System" tab whether MQTT is connected

### `spotreba_dnes` sensor shows `unknown` or doesn't grow
- The utility meter needs **at least 2 values** from the source sensor before it starts counting
- Wait a minute, it should start
- If `sensor.objem_celkem` doesn't exist, the helper sensor won't work — check the name

### History graph is empty
- The default HA recorder keeps **10 days of history** by default
- For a 7-day graph you need at least a few hours of data
- If you use InfluxDB / another DB, check the integration

### Notifications don't arrive on the phone
- Open the Mobile App on the phone → Settings → Notifications enabled?
- Test manually: **Developer Tools → Actions → notify.mobile_app_xxx** → Fill in a message → Run
- If nothing arrives, the problem is in the Mobile App integration

### Automation triggered but no notification
- **Settings → Automations** → click the automation → **Trace** (run logs)
- You'll see which part failed (often a wrong notify service name)

---

## 8. Useful Lovelace cards for advanced users

If you later **install HACS** and add a few nice cards, you can add:

- **bar-card** — tank as a vertical water column
- **mushroom-cards** — nice gauges and entity cards
- **apexcharts-card** — advanced consumption charts
- **mini-graph-card** — sparkline graphs

If you'd like, I can make an upgraded version of the dashboard for you — just ask.

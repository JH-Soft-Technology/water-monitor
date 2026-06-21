#include "period_stats.h"
#include <LittleFS.h>
#include <time.h>
#include <ArduinoJson.h>

// ============================================================================
// Persistent files in LittleFS
// ============================================================================
#define STATS_FILE   "/stats.json"
#define HISTORY_FILE "/history.json"

// ============================================================================
// NTP configuration (CET/CEST with automatic DST)
// ============================================================================
// POSIX TZ string for Europe/Prague: CET-1CEST,M3.5.0/2,M10.5.0/3
//   CET-1   = standard offset +1h
//   CEST    = summer time name
//   M3.5.0  = last Sunday of March (transition to CEST)
//   M10.5.0 = last Sunday of October (transition back to CET)
static const char *TZ_CET_CEST = "CET-1CEST,M3.5.0,M10.5.0/3";

// Primary + fallback NTP servers
static const char *NTP1 = "tik.cesnet.cz";
static const char *NTP2 = "pool.ntp.org";
static const char *NTP3 = "time.nist.gov";


// ============================================================================
// State (globals)
// ============================================================================
// Skalární akumulátory
static float today_l = 0.0f;
static float week_l = 0.0f;
static float month_l = 0.0f;
static float year_l = 0.0f;

// Historie - polní akumulátory pro grafy
static float hourly[24]    = {};   // L per hour today
static float daily_week[7] = {};   // L per weekday (Po=0..Ne=6)
static float daily_month[31] = {}; // L per day of month (index = mday-1)
static float monthly[12]   = {};   // L per month of year (index = mon, 0=leden)

// Reference indexy (poslední uložené hodnoty pro rollover)
static int ref_yday  = -1;
static int ref_week  = -1;
static int ref_month = -1;
static int ref_year  = -1;

// Aktivní bucket indexy (aktualizují se v statsLoop)
static int hist_hour  = -1;
static int hist_wday  = -1;
static int hist_mday  = -1;
static int hist_mon   = -1;

// NTP stav
static bool time_valid = false;
static unsigned long last_rollover_check = 0;
static const unsigned long ROLLOVER_CHECK_MS = 30000UL;  // každých 30s


// ============================================================================
// Helpers
// ============================================================================
// ISO týden ve formátu Monday-based (strftime %W).
// Vrací 0..53.
static int week_num(struct tm &ti) {
  char buf[8];
  strftime(buf, sizeof(buf), "%W", &ti);
  return atoi(buf);
}

// Získat lokální čas (po NTP sync). Vrací false pokud NTP ještě neproběhlo.
static bool get_localtime(struct tm &ti) {
  time_t now = time(nullptr);
  // Před NTP sync vrací time(nullptr) hodnoty před 2000-01-01 (typicky 1970..1990s)
  if (now < 1700000000) return false;  // 2023-11-14 = cutoff
  localtime_r(&now, &ti);
  time_valid = true;
  return true;
}


// ============================================================================
// Persistence
// ============================================================================
static void load_stats() {
  if (!LittleFS.exists(STATS_FILE)) return;
  File f = LittleFS.open(STATS_FILE, "r");
  if (!f) return;

  DynamicJsonDocument doc(400);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("stats.json deserialize error: %s\n", err.c_str());
    return;
  }

  today_l  = doc["today"]  | 0.0f;
  week_l   = doc["week"]   | 0.0f;
  month_l  = doc["month"]  | 0.0f;
  year_l   = doc["year"]   | 0.0f;
  ref_yday  = doc["yday"]  | -1;
  ref_week  = doc["wk"]    | -1;
  ref_month = doc["mon"]   | -1;
  ref_year  = doc["yr"]    | -1;

  Serial.printf("stats.json loaded: today=%.1f week=%.1f month=%.1f year=%.1f L\n",
                today_l, week_l, month_l, year_l);
}

static void save_stats() {
  DynamicJsonDocument doc(400);
  doc["today"] = today_l;
  doc["week"]  = week_l;
  doc["month"] = month_l;
  doc["year"]  = year_l;
  doc["yday"]  = ref_yday;
  doc["wk"]    = ref_week;
  doc["mon"]   = ref_month;
  doc["yr"]    = ref_year;

  File f = LittleFS.open(STATS_FILE, "w");
  if (!f) {
    Serial.println("stats.json: cannot open for writing");
    return;
  }
  serializeJson(doc, f);
  f.close();
}

static void load_history() {
  if (!LittleFS.exists(HISTORY_FILE)) return;
  File f = LittleFS.open(HISTORY_FILE, "r");
  if (!f) return;

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.printf("history.json deserialize error: %s\n", err.c_str());
    return;
  }

  JsonArray ah = doc["h"].as<JsonArray>();
  JsonArray aw = doc["w"].as<JsonArray>();
  JsonArray am = doc["m"].as<JsonArray>();
  JsonArray ay = doc["y"].as<JsonArray>();
  for (int i = 0; i < (int)ah.size() && i < 24; i++) hourly[i]      = ah[i] | 0.0f;
  for (int i = 0; i < (int)aw.size() && i < 7;  i++) daily_week[i]  = aw[i] | 0.0f;
  for (int i = 0; i < (int)am.size() && i < 31; i++) daily_month[i] = am[i] | 0.0f;
  for (int i = 0; i < (int)ay.size() && i < 12; i++) monthly[i]     = ay[i] | 0.0f;
}

static void save_history() {
  DynamicJsonDocument doc(2048);
  JsonArray ah = doc.createNestedArray("h");
  JsonArray aw = doc.createNestedArray("w");
  JsonArray am = doc.createNestedArray("m");
  JsonArray ay = doc.createNestedArray("y");
  for (int i = 0; i < 24; i++) ah.add(round(hourly[i]      * 10) / 10.0f);
  for (int i = 0; i < 7;  i++) aw.add(round(daily_week[i]  * 10) / 10.0f);
  for (int i = 0; i < 31; i++) am.add(round(daily_month[i] * 10) / 10.0f);
  for (int i = 0; i < 12; i++) ay.add(round(monthly[i]     * 10) / 10.0f);

  File f = LittleFS.open(HISTORY_FILE, "w");
  if (!f) {
    Serial.println("history.json: cannot open for writing");
    return;
  }
  serializeJson(doc, f);
  f.close();
}


// ============================================================================
// Rollover detection
// ============================================================================
// Volá se z statsLoop() každých ROLLOVER_CHECK_MS. Pokud se změnil den/týden/
// měsíc/rok, vynuluje příslušné akumulátory a histogramy a uloží stav.
static void check_rollover() {
  struct tm ti;
  if (!get_localtime(ti)) return;

  int cur_yday  = ti.tm_yday;
  int cur_week  = week_num(ti);
  int cur_month = ti.tm_mon;
  int cur_year  = ti.tm_year;

  // Aktualizuj aktivní bucket indexy (pro grafy a statsAddVolume)
  hist_hour = ti.tm_hour;
  hist_wday = (ti.tm_wday + 6) % 7;  // Po=0..Ne=6
  hist_mday = ti.tm_mday - 1;
  hist_mon  = ti.tm_mon;

  // První spuštění po nahrání NTP - nastav referenční hodnoty
  if (ref_year == -1) {
    ref_yday  = cur_yday;
    ref_week  = cur_week;
    ref_month = cur_month;
    ref_year  = cur_year;
    save_stats();
    return;
  }

  bool changed = false;

  if (cur_year != ref_year) {
    // Přechod přes Nový rok: reset všeho
    year_l = month_l = week_l = today_l = 0.0f;
    memset(monthly, 0, sizeof(monthly));
    memset(daily_month, 0, sizeof(daily_month));
    memset(daily_week, 0, sizeof(daily_week));
    memset(hourly, 0, sizeof(hourly));
    ref_year  = cur_year;
    ref_month = cur_month;
    ref_week  = cur_week;
    ref_yday  = cur_yday;
    changed = true;
    Serial.println("ROLLOVER: nový rok");
  }
  else if (cur_month != ref_month) {
    // Přechod na další měsíc: reset měsíc + týden + dnes
    month_l = week_l = today_l = 0.0f;
    memset(daily_month, 0, sizeof(daily_month));
    memset(daily_week, 0, sizeof(daily_week));
    memset(hourly, 0, sizeof(hourly));
    ref_month = cur_month;
    ref_week  = cur_week;
    ref_yday  = cur_yday;
    changed = true;
    Serial.println("ROLLOVER: nový měsíc");
  }
  else if (cur_week != ref_week) {
    // Přechod na další týden (pondělí): reset týden + dnes
    week_l = today_l = 0.0f;
    memset(daily_week, 0, sizeof(daily_week));
    memset(hourly, 0, sizeof(hourly));
    ref_week = cur_week;
    ref_yday = cur_yday;
    changed = true;
    Serial.println("ROLLOVER: nový týden");
  }
  else if (cur_yday != ref_yday) {
    // Přechod přes půlnoc: reset dnes
    today_l = 0.0f;
    memset(hourly, 0, sizeof(hourly));
    ref_yday = cur_yday;
    changed = true;
    Serial.println("ROLLOVER: nový den");
  }

  if (changed) {
    save_stats();
    save_history();
  }
}


// ============================================================================
// Public API
// ============================================================================
void statsInit() {
  Serial.println("Period stats: init");
  load_stats();
  load_history();

  // Nastav timezone a spusť NTP
  configTime(TZ_CET_CEST, NTP1, NTP2, NTP3);
  Serial.println("NTP sync started (CET/CEST, tik.cesnet.cz)");

  // Volat check_rollover hned (NTP může nebo nemusí být ready - vyřeší si to)
  last_rollover_check = millis();
}

void statsLoop() {
  unsigned long now = millis();
  if (now - last_rollover_check >= ROLLOVER_CHECK_MS) {
    last_rollover_check = now;
    check_rollover();
  }
}

void statsAddVolume(float liters) {
  if (liters <= 0.0f) return;

  // Distribuce do skalárních akumulátorů
  today_l += liters;
  week_l  += liters;
  month_l += liters;
  year_l  += liters;

  // Distribuce do histogramů (jen pokud máme platný čas)
  if (hist_hour >= 0 && hist_hour < 24)   hourly[hist_hour]       += liters;
  if (hist_wday >= 0 && hist_wday < 7)    daily_week[hist_wday]   += liters;
  if (hist_mday >= 0 && hist_mday < 31)   daily_month[hist_mday]  += liters;
  if (hist_mon  >= 0 && hist_mon  < 12)   monthly[hist_mon]       += liters;
}

void statsResetAll() {
  today_l = week_l = month_l = year_l = 0.0f;
  memset(hourly, 0, sizeof(hourly));
  memset(daily_week, 0, sizeof(daily_week));
  memset(daily_month, 0, sizeof(daily_month));
  memset(monthly, 0, sizeof(monthly));
  save_stats();
  save_history();
  Serial.println("Period stats: reset all");
}

PeriodStats statsGet() {
  PeriodStats s;
  s.today_l    = today_l;
  s.week_l     = week_l;
  s.month_l    = month_l;
  s.year_l     = year_l;
  s.time_valid = time_valid;
  return s;
}

const float* statsGetHourly()     { return hourly; }
const float* statsGetDailyWeek()  { return daily_week; }
const float* statsGetDailyMonth() { return daily_month; }
const float* statsGetMonthly()    { return monthly; }

int statsGetActiveHour()    { return hist_hour; }
int statsGetActiveWeekday() { return hist_wday; }
int statsGetActiveMday()    { return hist_mday; }
int statsGetActiveMonth()   { return hist_mon; }

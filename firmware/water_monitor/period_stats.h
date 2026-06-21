#ifndef PERIOD_STATS_H
#define PERIOD_STATS_H

#include <Arduino.h>

// ============================================================================
// Period statistics module
// ============================================================================
// Vede statistiku spotřeby vody za den/týden/měsíc/rok s automatickým resetem
// dle reálného času (NTP CET/CEST). Histogram polí pro grafy v HA i web UI.
//
// Inspirováno modulem v ha-rain-sensor (https://github.com/JH-Soft-Technology/ha-rain-sensor).
//
// Použití:
//   1) statsInit()                    - po WiFi connect (spustí NTP)
//   2) statsLoop()                    - každý loop (periodický check_rollover)
//   3) statsAddVolume(litry)          - z sensors při minutovém callbacku
//   4) statsGet*()                    - read-only přístupy pro MQTT/web UI
// ============================================================================


// Skalární akumulátory období (čítače spotřeby v litrech)
struct PeriodStats {
  float today_l;       // L spotřebovaných od půlnoci
  float week_l;        // L spotřebovaných od pondělí (ISO týden)
  float month_l;       // L spotřebovaných od 1. dne v měsíci
  float year_l;        // L spotřebovaných od 1. ledna
  bool  time_valid;    // true pokud NTP úspěšně odpovědělo
};


// Inicializace - načte uložené hodnoty z LittleFS, spustí NTP sync.
// Volat po WiFi STA připojení.
void statsInit();

// Periodická obsluha - kontroluje den/týden/měsíc/rok přechody,
// volá save_stats() při změně. Volat v loop().
void statsLoop();

// Přidat objem k aktuálním akumulátorům (volá se z handlerů minutových objemů).
// Distribuuje litry do today/week/month/year a do histogramů (hour, weekday, day, month).
void statsAddVolume(float liters);

// Reset všech statistik (manuálně přes web UI nebo MQTT command).
void statsResetAll();

// ---- Read-only přístup pro publikaci ----
PeriodStats statsGet();

// Historie pro grafy (read-only ukazatel)
const float* statsGetHourly();      // 24 buckets, index = tm_hour
const float* statsGetDailyWeek();   // 7 buckets, index = (wday+6)%7 (Po=0..Ne=6)
const float* statsGetDailyMonth();  // 31 buckets, index = tm_mday-1
const float* statsGetMonthly();     // 12 buckets, index = tm_mon (0=leden)

// Aktivní indexy (pro zvýraznění v grafech)
int statsGetActiveHour();
int statsGetActiveWeekday();
int statsGetActiveMday();
int statsGetActiveMonth();

#endif // PERIOD_STATS_H

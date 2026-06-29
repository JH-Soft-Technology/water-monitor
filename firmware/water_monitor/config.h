#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================================
// HW PINY (nemění se za běhu)
// ============================================================================
#define FLOW_PIN         D2   // GPIO4  - průtokoměr (interrupt)
#define ULTRASONIC_TRIG  D5   // GPIO14 - ultrazvuk TRIG
#define ULTRASONIC_ECHO  D6   // GPIO12 - ultrazvuk ECHO

// ============================================================================
// PRŮTOKOMĚR
// ============================================================================
// K-faktor je nově runtime konfigurace (config.pulses_per_liter), nastavitelná
// ve web UI (Konfigurace → Průtokoměr). Tato hodnota je jen VÝCHOZÍ pro nové /
// nenakonfigurované zařízení. Vztah: K_FACTOR [Hz na L/min] = pulses_per_liter / 60.
#define DEFAULT_PULSES_PER_LITER 62.0f              // kompromis 2 měření: 65 pulzů/L @ 11.5 L/min, 59.5 pulzů/L @ 30 L/min
#define MIN_PULSE_INTERVAL_US    800UL              // Debouncing (max 540 Hz)

// Výpočet průtoku z doby mezi pulzy (místo počítání za sekundu).
// Ukládáme časové razítko posledních N platných pulzů a průměrujeme periodu.
// Výhoda: reaguje okamžitě i na 1 pulz za několik sekund (nízký průtok).
#define FLOW_PERIOD_SAMPLES   8                     // kolik posledních pulzů průměrovat
#define FLOW_STALE_US         10000000UL            // bez pulzu > 10 s → průtok = 0

// ============================================================================
// ULTRAZVUK
// ============================================================================
#define MAX_DISTANCE_CM       250                   // Atlantis 5300: max 213cm + rezerva
#define MEDIAN_SAMPLES        5
#define SOUND_SPEED_TEMP_C    12.0f                 // Průměrná teplota v podzemní nádrži

// ============================================================================
// ČASOVÉ INTERVALY (ms)
// ============================================================================
#define FLOW_CALC_INTERVAL              1000UL
#define MINUTE_INTERVAL                 60000UL
#define HOUR_INTERVAL                   3600000UL
#define TANK_MEASURE_INTERVAL           30000UL     // Hladina každých 30s
#define PERSIST_INTERVAL                600000UL    // Perzistence totalu každých 10 min (ochrana proti výpadku)
#define PERIOD_STATS_PUBLISH_INTERVAL   60000UL     // Publikuj period stats (today/week/month/year) každou minutu

// ============================================================================
// ACCESS POINT (setup režim)
// ============================================================================
#define AP_SSID      "WaterMonitor-Setup"
#define AP_PASSWORD  ""                              // Otevřená síť

// ============================================================================
// CESTY V LITTLEFS
// ============================================================================
#define CONFIG_FILE       "/config.json"
#define CALIBRATION_FILE  "/calibration.json"
#define STATS_FILE        "/stats.json"             // Period stats akumulátory
#define HISTORY_FILE      "/history.json"           // Period stats histogramy

// ============================================================================
// VERZE
// ============================================================================
#define FW_VERSION "2.6"

#endif // CONFIG_H

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
#define K_FACTOR              4.5f                  // Hz na l/min
#define PULSES_PER_LITER      (K_FACTOR * 60.0f)    // = 270
#define MIN_PULSE_INTERVAL_US 800UL                 // Debouncing (max 540 Hz)

// ============================================================================
// ULTRAZVUK
// ============================================================================
#define MAX_DISTANCE_CM       250                   // Atlantis 5300: max 213cm + rezerva
#define MEDIAN_SAMPLES        5
#define SOUND_SPEED_TEMP_C    12.0f                 // Průměrná teplota v podzemní nádrži

// ============================================================================
// ČASOVÉ INTERVALY (ms)
// ============================================================================
#define FLOW_CALC_INTERVAL       1000UL
#define MINUTE_INTERVAL          60000UL
#define HOUR_INTERVAL            3600000UL
#define TANK_MEASURE_INTERVAL    60000UL            // Hladina každých 60s

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

// ============================================================================
// VERZE
// ============================================================================
#define FW_VERSION "2.0"

#endif // CONFIG_H

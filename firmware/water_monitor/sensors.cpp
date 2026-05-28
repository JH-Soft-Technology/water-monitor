#include "sensors.h"
#include "config.h"
#include "config_storage.h"
#include "calibration.h"
#include <NewPing.h>

extern Config config;

// Pulzní čítače - volatile kvůli ISR
static volatile uint32_t pulseCount = 0;
static volatile uint32_t pulseCountMinute = 0;
static volatile uint32_t pulseCountHour = 0;
static volatile uint32_t pulseCountTotal = 0;
static volatile uint32_t lastPulseMicros = 0;

// NewPing instance pro ultrazvuk
static NewPing* sonar = nullptr;

// Poslední změřené hodnoty (pro web UI)
static float lastFlowLpm = 0.0f;
static TankMeasurement lastTank = {0, 0, 0, 0, false};

// ============================================================================
// ISR - detekce pulzu z optočlenu
// ============================================================================
static void IRAM_ATTR onPulse() {
  uint32_t now = micros();
  if (now - lastPulseMicros < MIN_PULSE_INTERVAL_US) {
    return;  // Debouncing
  }
  lastPulseMicros = now;
  
  pulseCount++;
  pulseCountMinute++;
  pulseCountHour++;
  pulseCountTotal++;
}

// ============================================================================
void sensorsInit() {
  // Průtokoměr - vstup s pull-up (PC817 invertuje signál)
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), onPulse, FALLING);
  
  // Načti perzistovaný celkový čítač (přežije restart)
  pulseCountTotal = config.pulse_count_total_persisted;
  
  // Ultrazvuk
  sonar = new NewPing(ULTRASONIC_TRIG, ULTRASONIC_ECHO, MAX_DISTANCE_CM);
  
  Serial.printf("Senzory init: total pulses = %u (= %.3f L)\n",
                pulseCountTotal, pulseCountTotal / PULSES_PER_LITER);
}

// ============================================================================
float sensorsCalculateFlow(unsigned long elapsed_ms) {
  noInterrupts();
  uint32_t pulses = pulseCount;
  pulseCount = 0;
  interrupts();
  
  float frequency = (float)pulses * 1000.0f / (float)elapsed_ms;
  lastFlowLpm = frequency / K_FACTOR;
  return lastFlowLpm;
}

// ============================================================================
void sensorsGetMinuteVolume(float& volume_minute_l, float& volume_total_l) {
  noInterrupts();
  uint32_t pulsesMin = pulseCountMinute;
  pulseCountMinute = 0;
  uint32_t pulsesTotal = pulseCountTotal;
  interrupts();
  
  volume_minute_l = (float)pulsesMin / PULSES_PER_LITER;
  volume_total_l  = (float)pulsesTotal / PULSES_PER_LITER;
}

// ============================================================================
float sensorsGetHourVolume() {
  noInterrupts();
  uint32_t pulsesHour = pulseCountHour;
  pulseCountHour = 0;
  uint32_t pulsesTotal = pulseCountTotal;
  interrupts();
  
  // Periodicky persistuj total (jednou za hodinu)
  configPersistTotalPulses(pulsesTotal);
  
  return (float)pulsesHour / PULSES_PER_LITER;
}

// ============================================================================
float sensorsGetTotalVolume() {
  noInterrupts();
  uint32_t pulsesTotal = pulseCountTotal;
  interrupts();
  
  return (float)pulsesTotal / PULSES_PER_LITER;
}

// ============================================================================
bool sensorsMeasureTank(TankMeasurement& result) {
  result.valid = false;
  
  if (!sonar) return false;
  
  float samples[MEDIAN_SAMPLES];
  uint8_t validCount = 0;
  
  // Rychlost zvuku: 331.4 + 0.6 * T[°C]  [m/s]
  float soundSpeed = 331.4f + 0.6f * SOUND_SPEED_TEMP_C;
  
  for (uint8_t i = 0; i < MEDIAN_SAMPLES; i++) {
    unsigned int us = sonar->ping_median(3);
    if (us > 0) {
      // us → cm:  distance = (čas / 2) * rychlost zvuku v cm/µs
      float distance = (us / 2.0f) * (soundSpeed / 10000.0f);
      samples[validCount++] = distance;
    }
    delay(50);
  }
  
  if (validCount == 0) {
    Serial.println("Tank: žádné platné měření!");
    return false;
  }
  
  // Medián
  for (uint8_t i = 0; i < validCount - 1; i++) {
    for (uint8_t j = i + 1; j < validCount; j++) {
      if (samples[i] > samples[j]) {
        float tmp = samples[i];
        samples[i] = samples[j];
        samples[j] = tmp;
      }
    }
  }
  float distance = samples[validCount / 2];
  
  result.distance_cm = distance;
  result.level_cm = config.tank_distance_empty_cm - distance;
  if (result.level_cm < 0) result.level_cm = 0;
  
  float maxLevel = config.tank_distance_empty_cm - config.tank_distance_full_cm;
  result.fill_percent = (result.level_cm / maxLevel) * 100.0f;
  if (result.fill_percent < 0) result.fill_percent = 0;
  if (result.fill_percent > 100) result.fill_percent = 100;
  
  result.volume_l = calibrationLevelToVolume(result.level_cm);
  result.valid = true;
  
  lastTank = result;
  return true;
}

// ============================================================================
float sensorsGetLastFlow() {
  return lastFlowLpm;
}

TankMeasurement sensorsGetLastTank() {
  return lastTank;
}

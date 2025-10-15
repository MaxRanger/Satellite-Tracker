/*
 * Raspberry Pi Pico 2 (RP2350) Satellite Tracker - Main Program
 */

#include <Arduino.h>
#include "config.h"
#include "shared_data.h"
#include "motor_control.h"
#include "gps_module.h"
#include "compass_module.h"
#include "display_module.h"
#include "web_interface.h"
#include "tracking_logic.h"

// LED "sign of life" indicator
#define LED_BUILTIN 25  // Pico 2 built-in LED

// LED blink patterns
unsigned long ledLastBlink = 0;
unsigned int ledBlinkInterval = 1000;  // Default: 1 second
bool ledState = false;

// Compass calibration state (shared with display module)
extern bool compassCalibrating;
extern unsigned long calibrationStartTime;

void updateLED() {
  unsigned long now = millis();
  
  // Determine blink pattern based on system state
  if (!trackerState.gpsValid) {
    // Fast blink: No GPS lock
    ledBlinkInterval = 200;
  } else if (trackerState.tracking) {
    // Double blink: Tracking active
    if ((now / 100) % 10 < 2) {
      ledBlinkInterval = 100;
    } else {
      ledBlinkInterval = 800;
    }
  } else if (WiFi.status() == WL_CONNECTED) {
    // Slow blink: Idle with WiFi
    ledBlinkInterval = 1000;
  } else {
    // Medium blink: Idle without WiFi
    ledBlinkInterval = 500;
  }
  
  // Update LED state
  if (now - ledLastBlink >= ledBlinkInterval) {
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
    ledLastBlink = now;
  }
}

// ============================================================================
// CORE 0: MAIN SETUP AND LOOP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=== RP2350 Satellite Tracker ===");
  Serial.println("Core 0: Initializing...");
  
  // Initialize LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // Turn on during init
  
  // Initialize subsystems in order
  initSharedData();
  initMotorControl();
  initCompass();
  initGPS();
  initDisplay();
  initWebInterface();
  
  // Home the axes
  homeAxes();
  
  Serial.println("Core 0: Ready!");
  Serial.println("\n=== Serial Commands ===");
  Serial.println("On Settings screen:");
  Serial.println("  Type 'SSID' then enter network name");
  Serial.println("  Type 'PASSWORD' then enter network password");
  Serial.println("  Touch 'Connect' to apply");
  Serial.println("\nFor manual compass calibration:");
  Serial.println("  Call calibrateCompass() from Serial Monitor");
  Serial.println("  Or use Settings screen 'Cal Compass' button\n");
  
  // LED off after init
  digitalWrite(LED_BUILTIN, LOW);
}

void loop() {
  static unsigned long lastControlUpdate = 0;
  static unsigned long lastGPSUpdate = 0;
  static unsigned long lastDisplayUpdate = 0;
  static unsigned long lastCompassUpdate = 0;
  
  unsigned long now = millis();
  
  // Update LED indicator
  updateLED();
  
  // Handle web requests
  handleWebClient();
  
  // Handle touch input
  handleDisplayTouch();
  
  // Update GPS (1 Hz)
  if (now - lastGPSUpdate >= 1000) {
    updateGPS();
    lastGPSUpdate = now;
  }
  
  // Motor control loop (100 Hz)
  if (now - lastControlUpdate >= TRACKING_UPDATE_MS) {
    updateMotorControl();
    lastControlUpdate = now;
  }
  
  // Update display (2 Hz)
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_MS) {
    updateDisplay();
    lastDisplayUpdate = now;
  }
  
  // Handle compass calibration (20 Hz when active)
  // Simply delegates to compass module
  if (now - lastCompassUpdate >= 50) {
    updateBackgroundCalibration();
    lastCompassUpdate = now;
  }
  
  yield();
}

// ============================================================================
// CORE 1: SATELLITE CALCULATION
// ============================================================================

void setup1() {
  Serial.println("Core 1: Satellite calculation engine started");
  initTracking();
}

void loop1() {
  // Process TLE updates and calculate satellite positions
  updateTracking();
  
  // Run at lower rate than motor control (10 Hz)
  delay(100);
}
/*
 * Raspberry Pi Pico 2 (RP2350) Satellite Tracker - Main Program
 * WITH BACKGROUND COMPASS CALIBRATION
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

// Compass calibration data collection
struct CompassCalData {
  int minX, maxX;
  int minY, maxY;
  int minZ, maxZ;
  bool initialized;
};

CompassCalData calData = {32767, -32768, 32767, -32768, 32767, -32768, false};

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

void handleCompassCalibration() {
  if (!compassCalibrating) {
    // Reset calibration data when not calibrating
    if (calData.initialized) {
      calData.initialized = false;
    }
    return;
  }
  
  // Initialize calibration data
  if (!calData.initialized) {
    calData.minX = 32767;
    calData.maxX = -32768;
    calData.minY = 32767;
    calData.maxY = -32768;
    calData.minZ = 32767;
    calData.maxZ = -32768;
    calData.initialized = true;
  }
  
  // Collect compass data
  QMC5883LCompass& compass = getCompass();
  compass.read();
  
  int x = compass.getX();
  int y = compass.getY();
  int z = compass.getZ();
  
  // Update min/max values
  calData.minX = min(calData.minX, x);
  calData.maxX = max(calData.maxX, x);
  calData.minY = min(calData.minY, y);
  calData.maxY = max(calData.maxY, y);
  calData.minZ = min(calData.minZ, z);
  calData.maxZ = max(calData.maxZ, z);
  
  // Print progress every 2 seconds
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 2000) {
    unsigned long elapsed = (millis() - calibrationStartTime) / 1000;
    int rangeX = calData.maxX - calData.minX;
    int rangeY = calData.maxY - calData.minY;
    int rangeZ = calData.maxZ - calData.minZ;
    
    Serial.printf("Calibration: %lus  X:%d Y:%d Z:%d\n", 
                  elapsed, rangeX, rangeY, rangeZ);
    lastPrint = millis();
    displayNeedsUpdate = true;  // Update display with new time
  }
  
  // Check if calibration was stopped
  if (!compassCalibrating && calData.initialized) {
    unsigned long calibrationDuration = millis() - calibrationStartTime;
    
    // Validate calibration
    int rangeX = calData.maxX - calData.minX;
    int rangeY = calData.maxY - calData.minY;
    int rangeZ = calData.maxZ - calData.minZ;
    
    Serial.println("\n=== Compass Calibration Complete ===");
    Serial.printf("Duration: %lu seconds\n", calibrationDuration / 1000);
    
    if (calibrationDuration < 15000) {
      Serial.println("WARNING: Calibration too short (< 15s)");
    }
    
    if (rangeX < 100 || rangeY < 100 || rangeZ < 100) {
      Serial.println("WARNING: Insufficient rotation detected!");
      Serial.println("Some axes have limited range.");
    }
    
    // Apply calibration
    setCompassCalibration(calData.minX, calData.maxX, 
                         calData.minY, calData.maxY, 
                         calData.minZ, calData.maxZ);
    
    Serial.println("Calibration Values:");
    Serial.printf("X: [%d, %d] range=%d\n", calData.minX, calData.maxX, rangeX);
    Serial.printf("Y: [%d, %d] range=%d\n", calData.minY, calData.maxY, rangeY);
    Serial.printf("Z: [%d, %d] range=%d\n", calData.minZ, calData.maxZ, rangeZ);
    Serial.println("\nAdd to initCompass() for permanent calibration:");
    Serial.printf("compass.setCalibration(%d, %d, %d, %d, %d, %d);\n",
                  calData.minX, calData.maxX, 
                  calData.minY, calData.maxY, 
                  calData.minZ, calData.maxZ);
    
    calData.initialized = false;
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
  //initMotorControl();
  //initCompass();
  //initGPS();
  initDisplay();
  //initWebInterface();
  
  // Home the axes
  //homeAxes();
  
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
//    updateGPS();
    lastGPSUpdate = now;
  }
  
  // Motor control loop (100 Hz)
  if (now - lastControlUpdate >= TRACKING_UPDATE_MS) {
//    updateMotorControl();
    lastControlUpdate = now;
  }
  
  // Update display (2 Hz)
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_MS) {
    updateDisplay();
    lastDisplayUpdate = now;
  }
  
  // Handle compass calibration (20 Hz when active)
  if (now - lastCompassUpdate >= 50) {
  //  handleCompassCalibration();
    lastCompassUpdate = now;
  }
  
  yield();
}

// ============================================================================
// CORE 1: SATELLITE CALCULATION
// ============================================================================

void setup1() {
  Serial.println("Core 1: Satellite calculation engine started");
 // initTracking();
}

void loop1() {
  // Process TLE updates and calculate satellite positions
//  updateTracking();
  
  // Run at lower rate than motor control (10 Hz)
  delay(100);
}
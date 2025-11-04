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
#include "serial_interface.h"
#include "joystick_module.h"
#include "button_module.h"
#include "led_module.h"
#include "storage_module.h"


// Pulse LED blink patterns
unsigned long pulseLastBlink = 0;
unsigned int pulseBlinkInterval = 1000;
bool pulseState = false;

void updatePulse() {
  unsigned long now = millis();
  
  // Determine blink pattern based on system state
  if (!trackerState.gpsValid) {
    // Fast blink: No GPS lock
    pulseBlinkInterval = 200;
  } else if (trackerState.tracking) {
    // Double blink: Tracking active
    if ((now / 100) % 10 < 2) {
      pulseBlinkInterval = 100;
    } else {
      pulseBlinkInterval = 800;
    }
  } else if (WiFi.status() == WL_CONNECTED) {
    // Slow blink: Idle with WiFi
    pulseBlinkInterval = 1000;
  } else {
    // Medium blink: Idle without WiFi
    pulseBlinkInterval = 500;
  }
  
  // Update pulse LED state
  if (now - pulseLastBlink >= pulseBlinkInterval) {
    pulseState = !pulseState;
    digitalWrite(LED_BUILTIN, pulseState ? HIGH : LOW);
    pulseLastBlink = now;
  }
}

// ============================================================================
// CORE 0: MAIN SETUP AND LOOP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  // Print banner
  printBanner();
  Serial.println(F("Core 0: Initializing..."));
  
  // Initialize Pulse LED
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  
  // Initialize subsystems in order
  initSharedData();
  initStorage();
  //initMotorControl();
  initCompass();
  initGPS();
  initJoystick();
  //initButtons();
  initLEDs();
  initDisplay();
  initSerialInterface();
  
  // Load saved configuration
  if (isStorageAvailable()) {
    Serial.println(F("Loading saved configuration..."));
    StorageConfig config = {0};
    if (loadConfig(&config)) {
      // WiFi
      if (config.wifiConfigured) {
        strncpy(wifiSSID, config.wifiSSID, sizeof(wifiSSID) - 1);
        strncpy(wifiPassword, config.wifiPassword, sizeof(wifiPassword) - 1);
        wifiConfigured = true;
        Serial.println(F("WiFi credentials loaded"));
      }
      
      // Joystick calibration
      if (config.joyCalibrated) {
        JoystickCalibration joyCal;
        joyCal.xMin = config.joyXMin;
        joyCal.xCenter = config.joyXCenter;
        joyCal.xMax = config.joyXMax;
        joyCal.yMin = config.joyYMin;
        joyCal.yCenter = config.joyYCenter;
        joyCal.yMax = config.joyYMax;
        joyCal.deadband = config.joyDeadband;
        setJoystickCalibration(joyCal);
        Serial.println(F("Joystick calibration loaded"));
      }
      
      // Compass calibration
      if (config.compassCalibrated) {
        setCompassCalibration(config.compassMinX, config.compassMaxX,
                             config.compassMinY, config.compassMaxY,
                             config.compassMinZ, config.compassMaxZ);
        Serial.println(F("Compass calibration loaded"));
      }
      
      // TLE data
      if (config.tleValid) {
        strncpy(satelliteName, config.satelliteName, sizeof(satelliteName) - 1);
        strncpy(tleLine1, config.tleLine1, sizeof(tleLine1) - 1);
        strncpy(tleLine2, config.tleLine2, sizeof(tleLine2) - 1);
        trackerState.tleValid = true;
        Serial.print(F("TLE loaded: "));
        Serial.println(satelliteName);
      }
    }
  }
  
  // Initialize web interface (uses WiFi credentials)
  initWebInterface();
  
  // Home the axes
  //homeAxes();
  
  Serial.println(F("Core 0: Ready!"));
  Serial.println();
  printHelp();  // Print available commands
  Serial.print(F("> "));  // Command prompt
  
  // LED off after init
  digitalWrite(LED_BUILTIN, LOW);
  
  // Set LED mode based on system state
  if (trackerState.gpsValid) {
    setLEDMode(LED_MODE_STEADY_GREEN);
  } else {
    setLEDMode(LED_MODE_FLASH_YELLOW);
  }
}

void loop() {
  static unsigned long lastControlUpdate = 0;
  static unsigned long lastGPSUpdate = 0;
  static unsigned long lastDisplayUpdate = 0;
  static unsigned long lastCompassUpdate = 0;
  static unsigned long lastJoystickUpdate = 0;
  static unsigned long lastLEDUpdate = 0;
  
  unsigned long now = millis();
  
  // Update LED indicator
  updatePulse();
  
  // Update LED ring (20 Hz)
  if (now - lastLEDUpdate >= 150) {
    updateLEDs();
    lastLEDUpdate = now;
  }
  
  // Process serial commands (NEW)
  updateSerialInterface();
  
  // Handle web requests
  handleWebClient();
  
  // Handle touch input
  handleDisplayTouch();
  
  // Poll hardware buttons (NEW)
  //pollButtons();
  
  // Update joystick (NEW - 50 Hz)
  if (now - lastJoystickUpdate >= 200) {
    updateJoystick();
    
    // If joystick manual mode is active, override target position
    if (isJoystickManualMode()) {
      float azSpeed = getJoystickAzimuthSpeed();
      float elSpeed = getJoystickElevationSpeed();
      
      // Update target position based on joystick
      // Speed is normalized -1 to +1, scale to degrees per update
      const float MANUAL_SPEED = 1.0; // degrees per 20ms at full deflection
      
      if (abs(azSpeed) > 0.01) {
        targetPos.azimuth += azSpeed * MANUAL_SPEED;
        while (targetPos.azimuth < 0) targetPos.azimuth += 360.0;
        while (targetPos.azimuth >= 360) targetPos.azimuth -= 360.0;
        trackerState.tracking = false; // Disable tracking when manually controlled
      }
      
      if (abs(elSpeed) > 0.01) {
        targetPos.elevation += elSpeed * MANUAL_SPEED;
        targetPos.elevation = constrain(targetPos.elevation, MIN_ELEVATION, MAX_ELEVATION);
        trackerState.tracking = false;
      }
      
      // Update LED mode for manual control
      setLEDMode(LED_MODE_STEADY_PURPLE);
    } else if (!trackerState.tracking) {
      // Return to normal LED mode when manual mode exits
      if (trackerState.gpsValid) {
        setLEDMode(LED_MODE_STEADY_GREEN);
      } else {
        setLEDMode(LED_MODE_FLASH_YELLOW);
      }
    }
    
    lastJoystickUpdate = now;
  }
  
  // Update GPS (1 Hz)
  if (now - lastGPSUpdate >= 1000) {
    updateGPS();
    
    // Update LED mode based on GPS status
    if (trackerState.gpsValid && !isJoystickManualMode()) {
      if (trackerState.tracking) {
        setLEDMode(LED_MODE_STEADY_GREEN);
      } else {
        setLEDMode(LED_MODE_STEADY_GREEN);
      }
    } else if (!isJoystickManualMode()) {
      setLEDMode(LED_MODE_FLASH_YELLOW);
    }
    
    lastGPSUpdate = now;
  }
  
  // Motor control loop (100 Hz)
  if (now - lastControlUpdate >= TRACKING_UPDATE_MS) {
    //updateMotorControl();
    
    // Update LED for emergency stop
    if (isEmergencyStop()) {
      setLEDMode(LED_MODE_FLASH_RED);
    }
    
    lastControlUpdate = now;
  }
  
  // Update display (2 Hz)
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_MS) {
    updateDisplay();
    lastDisplayUpdate = now;
  }
  
  // Handle compass calibration (20 Hz when active)
  if (now - lastCompassUpdate >= 50) {
    //updateBackgroundCalibration();
    
    // Update LED during compass calibration
    if (isBackgroundCalibrationActive()) {
      setLEDMode(LED_MODE_FLASH_BLUE);
    }
    
    lastCompassUpdate = now;
  }
  
  yield();
}

// ============================================================================
// CORE 1: SATELLITE CALCULATION
// ============================================================================

void setup1() {
  Serial.println(F("Core 1: Satellite calculation engine started"));
  //initTracking();
}

void loop1() {
  // Process TLE updates and calculate satellite positions
  //updateTracking();
  
  // Run at lower rate than motor control (10 Hz)
  delay(100);
}
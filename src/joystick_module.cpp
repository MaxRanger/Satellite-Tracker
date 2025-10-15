// ============================================================================
// joystick_module.cpp - Analog joystick implementation
// ============================================================================

#include "joystick_module.h"

// Current joystick state
static JoystickData currentState = {0, 0, 0.0, 0.0, false, true};

// Calibration data
static JoystickCalibration calibration = {
  0,      // xMin
  2048,   // xCenter (12-bit ADC midpoint)
  4095,   // xMax
  0,      // yMin
  2048,   // yCenter
  4095,   // yMax
  10      // deadband (10% default)
};

// Calibration state
static bool calibrating = false;
static uint16_t calXMin = 4095, calXMax = 0;
static uint16_t calYMin = 4095, calYMax = 0;
static uint32_t calXSum = 0, calYSum = 0;
static uint16_t calSampleCount = 0;

// Manual mode state
static bool manualModeActive = false;
static bool lastButtonState = false;
static unsigned long lastButtonChange = 0;
static JoystickToggleCallback toggleCallback = nullptr;

// Debounce time for button
#define BUTTON_DEBOUNCE_MS 50

// ============================================================================
// INTERNAL FUNCTIONS
// ============================================================================

// Apply deadband and normalize value
static float applyDeadbandAndNormalize(int16_t raw, uint16_t min, uint16_t center, uint16_t max, uint16_t deadbandPercent) {
  // Calculate deadband threshold
  uint16_t rangeHalf = (max - min) / 2;
  uint16_t deadbandRange = (rangeHalf * deadbandPercent) / 100;
  
  // Convert raw to signed offset from center
  int16_t offset = raw - center;
  
  // Apply deadband
  if (abs(offset) < deadbandRange) {
    return 0.0;
  }
  
  // Normalize to -1.0 to +1.0
  float normalized;
  if (offset > 0) {
    // Positive side
    normalized = (float)(offset - deadbandRange) / (float)(max - center - deadbandRange);
  } else {
    // Negative side
    normalized = (float)(offset + deadbandRange) / (float)(center - min - deadbandRange);
  }
  
  // Clamp to -1.0 to +1.0
  return constrain(normalized, -1.0, 1.0);
}

// Check if value is in deadband
static bool isInDeadband(int16_t raw, uint16_t center, uint16_t range, uint16_t deadbandPercent) {
  int16_t offset = abs(raw - center);
  uint16_t deadbandRange = (range * deadbandPercent) / 100;
  return offset < deadbandRange;
}

// Read raw ADC values
static void readRawJoystick(uint16_t* x, uint16_t* y, bool* button) {
  *x = analogRead(JOYSTICK_X_PIN);
  *y = analogRead(JOYSTICK_Y_PIN);
  *button = !digitalRead(JOYSTICK_BTN_PIN);  // Active low
}

// Process button with debouncing and toggle
static void processButton(bool currentButton) {
  unsigned long now = millis();
  
  // Debounce check
  if ((now - lastButtonChange) < BUTTON_DEBOUNCE_MS) {
    return;
  }
  
  // Detect button press (rising edge)
  if (currentButton && !lastButtonState) {
    // Toggle manual mode
    manualModeActive = !manualModeActive;
    lastButtonChange = now;
    
    Serial.print("Joystick manual mode: ");
    Serial.println(manualModeActive ? "ACTIVE" : "INACTIVE");
    
    // Call callback if set
    if (toggleCallback) {
      toggleCallback(manualModeActive);
    }
  }
  
  lastButtonState = currentButton;
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

void initJoystick() {
  Serial.println("Initializing joystick...");
  
  // Configure analog pins (RP2040/RP2350 has 12-bit ADC)
  analogReadResolution(12);  // 0-4095 range
  pinMode(JOYSTICK_X_PIN, INPUT);
  pinMode(JOYSTICK_Y_PIN, INPUT);
  
  // Configure button pin with pullup
  pinMode(JOYSTICK_BTN_PIN, INPUT_PULLUP);
  
  // Read initial center position
  uint16_t x, y;
  bool button;
  
  // Average multiple samples for center calibration
  uint32_t xSum = 0, ySum = 0;
  for (int i = 0; i < 10; i++) {
    readRawJoystick(&x, &y, &button);
    xSum += x;
    ySum += y;
    delay(10);
  }
  
  calibration.xCenter = xSum / 10;
  calibration.yCenter = ySum / 10;
  
  Serial.println("Joystick initialized");
  Serial.printf("  X pin: GPIO %d, Center: %d\n", JOYSTICK_X_PIN, calibration.xCenter);
  Serial.printf("  Y pin: GPIO %d, Center: %d\n", JOYSTICK_Y_PIN, calibration.yCenter);
  Serial.printf("  Button: GPIO %d\n", JOYSTICK_BTN_PIN);
  Serial.printf("  Deadband: %d%%\n", calibration.deadband);
}

JoystickData readJoystick() {
  uint16_t rawX, rawY;
  bool button;
  
  readRawJoystick(&rawX, &rawY, &button);
  
  // Update state
  currentState.x = rawX;
  currentState.y = rawY;
  currentState.buttonPressed = button;
  
  // Process button (toggle mode)
  processButton(button);
  
  // Normalize values with deadband
  currentState.xNormalized = applyDeadbandAndNormalize(
    rawX,
    calibration.xMin,
    calibration.xCenter,
    calibration.xMax,
    calibration.deadband
  );
  
  currentState.yNormalized = applyDeadbandAndNormalize(
    rawY,
    calibration.yMin,
    calibration.yCenter,
    calibration.yMax,
    calibration.deadband
  );
  
  // Check if in deadband
  uint16_t xRange = (calibration.xMax - calibration.xMin) / 2;
  uint16_t yRange = (calibration.yMax - calibration.yMin) / 2;
  bool xInDeadband = isInDeadband(rawX, calibration.xCenter, xRange, calibration.deadband);
  bool yInDeadband = isInDeadband(rawY, calibration.yCenter, yRange, calibration.deadband);
  currentState.inDeadband = xInDeadband && yInDeadband;
  
  return currentState;
}

JoystickData getJoystickState() {
  return currentState;
}

bool isJoystickCentered() {
  return currentState.inDeadband;
}

float getJoystickAzimuthSpeed() {
  // Only return speed if manual mode is active
  if (!manualModeActive) {
    return 0.0;
  }
  
  // X axis controls azimuth
  // Right (higher value) = positive speed (clockwise)
  // Left (lower value) = negative speed (counterclockwise)
  return currentState.xNormalized;
}

float getJoystickElevationSpeed() {
  // Only return speed if manual mode is active
  if (!manualModeActive) {
    return 0.0;
  }
  
  // Y axis controls elevation
  // Up (higher value) = positive speed (up)
  // Down (lower value) = negative speed (down)
  return currentState.yNormalized;
}

bool isJoystickManualMode() {
  return manualModeActive;
}

void setJoystickToggleCallback(JoystickToggleCallback callback) {
  toggleCallback = callback;
}

// ============================================================================
// CALIBRATION IMPLEMENTATION
// ============================================================================

JoystickCalibration getJoystickCalibration() {
  return calibration;
}

void setJoystickCalibration(JoystickCalibration cal) {
  calibration = cal;
  Serial.println("Joystick calibration updated");
  Serial.printf("  X: [%d, %d, %d]\n", cal.xMin, cal.xCenter, cal.xMax);
  Serial.printf("  Y: [%d, %d, %d]\n", cal.yMin, cal.yCenter, cal.yMax);
  Serial.printf("  Deadband: %d%%\n", cal.deadband);
}

void startJoystickCalibration() {
  if (calibrating) {
    Serial.println("Calibration already in progress");
    return;
  }
  
  Serial.println("\n=== Joystick Calibration ===");
  Serial.println("Move joystick through full range");
  Serial.println("Then center it and wait for completion");
  
  calibrating = true;
  calXMin = 4095;
  calXMax = 0;
  calYMin = 4095;
  calYMax = 0;
  calXSum = 0;
  calYSum = 0;
  calSampleCount = 0;
}

void stopJoystickCalibration() {
  if (!calibrating) {
    Serial.println("No calibration in progress");
    return;
  }
  
  calibrating = false;
  
  // Calculate center as average of samples
  if (calSampleCount > 0) {
    calibration.xCenter = calXSum / calSampleCount;
    calibration.yCenter = calYSum / calSampleCount;
  }
  
  // Set min/max
  calibration.xMin = calXMin;
  calibration.xMax = calXMax;
  calibration.yMin = calYMin;
  calibration.yMax = calYMax;
  
  Serial.println("\n=== Calibration Complete ===");
  Serial.printf("X: Min=%d, Center=%d, Max=%d, Range=%d\n",
                calibration.xMin, calibration.xCenter, calibration.xMax,
                calibration.xMax - calibration.xMin);
  Serial.printf("Y: Min=%d, Center=%d, Max=%d, Range=%d\n",
                calibration.yMin, calibration.yCenter, calibration.yMax,
                calibration.yMax - calibration.yMin);
  Serial.printf("Deadband: %d%%\n", calibration.deadband);
  
  Serial.println("\nAdd to config for permanent calibration:");
  Serial.printf("xMin=%d, xCenter=%d, xMax=%d\n", calibration.xMin, calibration.xCenter, calibration.xMax);
  Serial.printf("yMin=%d, yCenter=%d, yMax=%d\n", calibration.yMin, calibration.yCenter, calibration.yMax);
}

bool isJoystickCalibrating() {
  return calibrating;
}

void updateJoystickCalibration() {
  if (!calibrating) {
    return;
  }
  
  uint16_t x, y;
  bool button;
  readRawJoystick(&x, &y, &button);
  
  // Update min/max
  calXMin = min(calXMin, x);
  calXMax = max(calXMax, x);
  calYMin = min(calYMin, y);
  calYMax = max(calYMax, y);
  
  // Accumulate center samples
  calXSum += x;
  calYSum += y;
  calSampleCount++;
  
  // Print progress every 500ms
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint >= 500) {
    Serial.printf("Cal: X:[%d-%d] Y:[%d-%d] Samples:%d\n",
                  calXMin, calXMax, calYMin, calYMax, calSampleCount);
    lastPrint = millis();
  }
}

void resetJoystickCalibration() {
  calibration.xMin = 0;
  calibration.xCenter = 2048;
  calibration.xMax = 4095;
  calibration.yMin = 0;
  calibration.yCenter = 2048;
  calibration.yMax = 4095;
  calibration.deadband = 10;
  
  Serial.println("Joystick calibration reset to defaults");
}

void setJoystickDeadband(uint16_t percent) {
  calibration.deadband = constrain(percent, 0, 50);  // Max 50%
  Serial.printf("Joystick deadband: %d%%\n", calibration.deadband);
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void updateJoystick() {
  // Read and process joystick
  readJoystick();
  
  // Update calibration if active
  if (calibrating) {
    updateJoystickCalibration();
  }
}

void printJoystickState() {
  Serial.print("Joystick: ");
  Serial.print("X=");
  Serial.print(currentState.x);
  Serial.print(" (");
  Serial.print(currentState.xNormalized, 2);
  Serial.print(") Y=");
  Serial.print(currentState.y);
  Serial.print(" (");
  Serial.print(currentState.yNormalized, 2);
  Serial.print(") Btn=");
  Serial.print(currentState.buttonPressed ? "PRESS" : "REL");
  Serial.print(" Mode=");
  Serial.print(manualModeActive ? "MANUAL" : "AUTO");
  Serial.print(" Deadband=");
  Serial.println(currentState.inDeadband ? "YES" : "NO");
}
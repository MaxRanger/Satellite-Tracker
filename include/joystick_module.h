/*
 * joystick_module.h - Analog X/Y joystick with push button
 * Controls azimuth and elevation in manual mode
 */

#ifndef JOYSTICK_MODULE_H
#define JOYSTICK_MODULE_H

#include <Arduino.h>
#include "config.h"

// Joystick data structure
struct JoystickData {
  int16_t x;           // Raw X reading (0-4095 for 12-bit ADC)
  int16_t y;           // Raw Y reading
  float xNormalized;   // Normalized X (-1.0 to +1.0)
  float yNormalized;   // Normalized Y (-1.0 to +1.0)
  bool buttonPressed;  // Button state
  bool inDeadband;     // True if joystick near center
};

// Joystick calibration data
struct JoystickCalibration {
  uint16_t xMin;       // Minimum X reading
  uint16_t xCenter;    // Center X reading
  uint16_t xMax;       // Maximum X reading
  uint16_t yMin;       // Minimum Y reading
  uint16_t yCenter;    // Center Y reading
  uint16_t yMax;       // Maximum Y reading
  uint16_t deadband;   // Deadband radius (0-100, percentage)
};

// Joystick mode toggle callback
typedef void (*JoystickToggleCallback)(bool manualModeActive);

// ============================================================================
// PUBLIC API
// ============================================================================

// Initialize joystick module
void initJoystick();

// Read joystick data
JoystickData readJoystick();

// Get current joystick state
JoystickData getJoystickState();

// Check if joystick is in deadband
bool isJoystickCentered();

// Get speed command for azimuth (-1.0 to +1.0)
// Negative = left, Positive = right, 0 = stop
float getJoystickAzimuthSpeed();

// Get speed command for elevation (-1.0 to +1.0)
// Negative = down, Positive = up, 0 = stop
float getJoystickElevationSpeed();

// Check if manual mode is active (toggled by button)
bool isJoystickManualMode();

// Set manual mode callback (called when button toggles mode)
void setJoystickToggleCallback(JoystickToggleCallback callback);

// ============================================================================
// CALIBRATION API
// ============================================================================

// Get current calibration
JoystickCalibration getJoystickCalibration();

// Set calibration manually
void setJoystickCalibration(JoystickCalibration cal);

// Start automatic calibration (move joystick through full range)
void startJoystickCalibration();

// Stop calibration and apply results
void stopJoystickCalibration();

// Check if calibration is active
bool isJoystickCalibrating();

// Update calibration (call periodically during calibration)
void updateJoystickCalibration();

// Reset to default calibration (centered, full range)
void resetJoystickCalibration();

// Set deadband size (0-100, percentage of range)
void setJoystickDeadband(uint16_t percent);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// Update joystick state (call from main loop)
void updateJoystick();

// Print joystick state to Serial (for debugging)
void printJoystickState();

#endif // JOYSTICK_MODULE_H
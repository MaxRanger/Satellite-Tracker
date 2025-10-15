/*
 * compass_module.h - QMC5883L magnetometer interface
  */

#ifndef COMPASS_MODULE_H
#define COMPASS_MODULE_H

#include <Arduino.h>
#include <Wire.h>
#include <QMC5883LCompass.h>
#include "config.h"

// ============================================================================
// BASIC COMPASS FUNCTIONS
// ============================================================================

// Initialize compass
void initCompass();

// Read compass heading (0-360 degrees)
float readCompassHeading();

// Set calibration values manually
void setCompassCalibration(int minX, int maxX, int minY, int maxY, int minZ, int maxZ);

// Get compass object (for advanced usage)
QMC5883LCompass& getCompass();

// ============================================================================
// BACKGROUND CALIBRATION API (non-blocking, for use with display)
// ============================================================================

// Start background calibration collection
void startBackgroundCalibration();

// Stop background calibration and apply results
void stopBackgroundCalibration();

// Check if background calibration is active
bool isBackgroundCalibrationActive();

// Get calibration duration in seconds (0 if not active)
unsigned long getCalibrationDuration();

// Update background calibration (call periodically from main loop)
// Recommendation: Call at 20 Hz (every 50ms) when active
void updateBackgroundCalibration();

// ============================================================================
// BLOCKING CALIBRATION (for advanced users via serial)
// ============================================================================

// Blocking calibration routine (waits for serial input or timeout)
// Use this for manual calibration via Serial Monitor
void calibrateCompass();

#endif // COMPASS_MODULE_H
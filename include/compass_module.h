/*
 * compass_module.h - QMC5883L magnetometer interface
 */

#ifndef COMPASS_MODULE_H
#define COMPASS_MODULE_H

#include <Arduino.h>
#include <Wire.h>
#include <QMC5883LCompass.h>
#include "config.h"

// Initialize compass
void initCompass();

// Read compass heading
float readCompassHeading();

// Calibrate compass
void calibrateCompass();

// Set calibration values
void setCompassCalibration(int minX, int maxX, int minY, int maxY, int minZ, int maxZ);

// Get compass object (for advanced usage)
QMC5883LCompass& getCompass();

#endif // COMPASS_MODULE_H

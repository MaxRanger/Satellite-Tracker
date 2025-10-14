/*
 * tracking_logic.h - SGP4 satellite tracking calculations (Core 1)
 */

#ifndef TRACKING_LOGIC_H
#define TRACKING_LOGIC_H

#include <Arduino.h>
#include <Sgp4.h>
#include "config.h"
#include "shared_data.h"

// Initialize tracking system
void initTracking();

// Update tracking calculations
void updateTracking();

// Julian date conversion
double dateToJulian(int year, int month, int day, int hour, int minute, int second);

#endif // TRACKING_LOGIC_H


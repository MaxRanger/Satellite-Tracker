/*
 * gps_module.h - GPS receiver interface
 */

#ifndef GPS_MODULE_H
#define GPS_MODULE_H

#include <Arduino.h>
#include <TinyGPS++.h>
#include "config.h"
#include "shared_data.h"

// Initialize GPS module
void initGPS();

// Update GPS data
void updateGPS();

// Get GPS object (for advanced usage)
TinyGPSPlus& getGPS();

// Dump current GPS data to Serial (for debugging)
void printGPSStatus();
void printTLE();

// Run GPS module connection test
void connectionTest();

#endif // GPS_MODULE_H

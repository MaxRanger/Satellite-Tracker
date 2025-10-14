/*
 * shared_data.h - Shared data structures for inter-core communication
 */

#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <Arduino.h>

// ============================================================================
// SHARED DATA STRUCTURES
// ============================================================================

struct MotorPosition {
  volatile int32_t elevation;
  volatile int32_t azimuth;
  volatile bool elevationIndexFound;
  volatile bool azimuthIndexFound;
};

struct TargetPosition {
  volatile float elevation;
  volatile float azimuth;
  volatile bool valid;
};

struct TrackerState {
  volatile double latitude;
  volatile double longitude;
  volatile double altitude;
  volatile uint32_t gpsYear;
  volatile uint8_t gpsMonth;
  volatile uint8_t gpsDay;
  volatile uint8_t gpsHour;
  volatile uint8_t gpsMinute;
  volatile uint8_t gpsSecond;
  volatile bool gpsValid;
  volatile bool tleValid;
  volatile bool tracking;
};

// Global shared data
extern MotorPosition motorPos;
extern TargetPosition targetPos;
extern TrackerState trackerState;

// TLE Storage
extern char tleLine1[70];
extern char tleLine2[70];
extern char satelliteName[25];
extern volatile bool tleUpdatePending;

// PID State
extern float errorIntegralE;
extern float errorIntegralA;
extern float lastErrorE;
extern float lastErrorA;

// Display state
enum DisplayScreen {
  SCREEN_SETUP,
  SCREEN_KEYBOARD,
  SCREEN_MAIN,
  SCREEN_SATELLITE_LIST,
  SCREEN_MANUAL_CONTROL,
  SCREEN_SETTINGS,
  SCREEN_CALIBRATION
};

extern DisplayScreen currentScreen;
extern bool displayNeedsUpdate;

// WiFi credentials
extern char wifiSSID[32];
extern char wifiPassword[64];
extern bool wifiConfigured;

// Function to initialize shared data
void initSharedData();

#endif // SHARED_DATA_H
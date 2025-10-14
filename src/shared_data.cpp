/*
 * shared_data.cpp - Implementation of shared data structures
 */

#include "shared_data.h"

// Global shared data instances
MotorPosition motorPos = {0, 0, false, false};
TargetPosition targetPos = {0.0, 0.0, false};
TrackerState trackerState = {0, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false};

// TLE Storage
char tleLine1[70] = "";
char tleLine2[70] = "";
char satelliteName[25] = "";
volatile bool tleUpdatePending = false;

// PID State
float errorIntegralE = 0.0;
float errorIntegralA = 0.0;
float lastErrorE = 0.0;
float lastErrorA = 0.0;

// Display state
DisplayScreen currentScreen = SCREEN_SETUP;
bool displayNeedsUpdate = true;

// WiFi credentials
char wifiSSID[32] = "";
char wifiPassword[64] = "";
bool wifiConfigured = false;

void initSharedData() {
  // Initialize all shared data to safe defaults
  motorPos.elevation = 0;
  motorPos.azimuth = 0;
  motorPos.elevationIndexFound = false;
  motorPos.azimuthIndexFound = false;
  
  targetPos.elevation = 0.0;
  targetPos.azimuth = 0.0;
  targetPos.valid = false;
  
  trackerState.latitude = 0.0;
  trackerState.longitude = 0.0;
  trackerState.altitude = 0.0;
  trackerState.gpsValid = false;
  trackerState.tleValid = false;
  trackerState.tracking = false;
  
  tleUpdatePending = false;
  
  wifiConfigured = false;
  strcpy(wifiSSID, "");
  strcpy(wifiPassword, "");
  
  currentScreen = SCREEN_SETUP;
  displayNeedsUpdate = true;
  
  Serial.println("Shared data initialized");
}
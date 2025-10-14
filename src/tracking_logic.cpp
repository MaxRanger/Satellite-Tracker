// ============================================================================
// tracking_logic.cpp
// ============================================================================

#include "tracking_logic.h"

// External references to shared data (defined in shared_data.cpp)
extern MotorPosition motorPos;
extern TargetPosition targetPos;
extern TrackerState trackerState;
extern char tleLine1[70];
extern char tleLine2[70];
extern char satelliteName[25];
extern volatile bool tleUpdatePending;

Sgp4 sat;
bool satInitialized = false;

double dateToJulian(int year, int month, int day, int hour, int minute, int second) {
  int a = (14 - month) / 12;
  int y = year + 4800 - a;
  int m = month + 12 * a - 3;
  
  long jdn = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
  
  double fraction = (hour - 12.0) / 24.0 + minute / 1440.0 + second / 86400.0;
  
  return (double)jdn + fraction;
}

void initTracking() {
  Serial.println("Tracking engine initialized on Core 1");
  satInitialized = false;
}

void updateTracking() {
  // Check for TLE update
  if (tleUpdatePending) {
    Serial.println("Core 1: Processing TLE update");
    
    sat.site(trackerState.latitude, trackerState.longitude, trackerState.altitude);
    sat.init(satelliteName, tleLine1, tleLine2);
    
    satInitialized = true;
    trackerState.tleValid = true;
    trackerState.tracking = true;
    tleUpdatePending = false;
    
    Serial.println("Core 1: Satellite initialized");
  }
  
  // Calculate satellite position if tracking
  if (trackerState.tracking && satInitialized && trackerState.gpsValid) {
    int year = trackerState.gpsYear;
    int month = trackerState.gpsMonth;
    int day = trackerState.gpsDay;
    int hour = trackerState.gpsHour;
    int minute = trackerState.gpsMinute;
    int second = trackerState.gpsSecond;
    
    // Calculate Julian date
    double jd = dateToJulian(year, month, day, hour, minute, second);
    
    // Find satellite position
    sat.findsat(jd);
    
    // Get azimuth and elevation
    double az = sat.satAz;
    double el = sat.satEl;
    
    // Update target position
    targetPos.azimuth = (float)az;
    targetPos.elevation = (float)el;
    targetPos.valid = true;
    
    // If satellite below horizon, point to stow position
    if (el < 0) {
      targetPos.elevation = 0.0;
    }
  }
}
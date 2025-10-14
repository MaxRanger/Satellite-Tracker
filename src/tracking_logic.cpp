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

// NOTE ON RACE CONDITIONS:
// The RP2350 (Cortex-M33) uses hardware memory barriers and the volatile
// keyword should be sufficient for single-variable flags between cores.
// However, for complex multi-variable operations, we need to ensure atomicity.
//
// ANALYSIS: 
// - tleUpdatePending is only WRITTEN by Core 0 (web_interface.cpp)
// - tleUpdatePending is only READ/CLEARED by Core 1 (this file)
// - The boolean flag is volatile and aligned
// - The RP2350 ensures write ordering for volatile variables
//
// POTENTIAL RACE: When Core 1 reads tleUpdatePending=true and then reads
// tleLine1, tleLine2, satelliteName - Core 0 might still be writing them!
//
// FIX: Use a memory barrier or ensure Core 0 completes ALL writes before
// setting tleUpdatePending flag. The Pico SDK provides __dmb() for this.

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
  // Check for TLE update with proper memory ordering
  // The volatile read ensures we get the latest value
  if (tleUpdatePending) {
    // Memory barrier to ensure all previous writes are visible
    // This ensures tleLine1, tleLine2, satelliteName are fully written
    __dmb();
    
    Serial.println("Core 1: Processing TLE update");
    
    // Validate GPS before initializing satellite tracking
    if (!trackerState.gpsValid) {
      Serial.println("Core 1: Cannot initialize - no GPS fix");
      tleUpdatePending = false;
      return;
    }
    
    // Initialize satellite with current position
    sat.site(trackerState.latitude, trackerState.longitude, trackerState.altitude);
    sat.init(satelliteName, tleLine1, tleLine2);
    
    satInitialized = true;
    trackerState.tleValid = true;
    trackerState.tracking = true;
    
    // Clear flag last, after all processing complete
    __dmb();  // Ensure all writes complete before clearing flag
    tleUpdatePending = false;
    
    Serial.println("Core 1: Satellite initialized and tracking started");
    Serial.print("Tracking: ");
    Serial.println(satelliteName);
  }
  
  // Calculate satellite position if tracking
  if (trackerState.tracking && satInitialized && trackerState.gpsValid) {
    // Get current time from GPS
    int year = trackerState.gpsYear;
    int month = trackerState.gpsMonth;
    int day = trackerState.gpsDay;
    int hour = trackerState.gpsHour;
    int minute = trackerState.gpsMinute;
    int second = trackerState.gpsSecond;
    
    // Validate time data
    if (year < 2020 || year > 2100 || month < 1 || month > 12 || 
        day < 1 || day > 31) {
      Serial.println("Core 1: Invalid GPS time data");
      return;
    }
    
    // Calculate Julian date
    double jd = dateToJulian(year, month, day, hour, minute, second);
    
    // Find satellite position
    sat.findsat(jd);
    
    // Get azimuth and elevation
    double az = sat.satAz;
    double el = sat.satEl;
    
    // Update target position atomically
    // These are float assignments which are atomic on ARM Cortex-M
    targetPos.azimuth = (float)az;
    targetPos.elevation = (float)el;
    targetPos.valid = true;
    
    // If satellite below horizon, point to stow position
    if (el < 0) {
      targetPos.elevation = 0.0;
      // Could optionally stop tracking when satellite sets
      // trackerState.tracking = false;
    }
  } else if (trackerState.tracking && !trackerState.gpsValid) {
    // GPS was lost during tracking
    Serial.println("Core 1: GPS lost, stopping tracking");
    trackerState.tracking = false;
  }
}
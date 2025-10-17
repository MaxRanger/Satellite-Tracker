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

// Predictive tracking parameters
#define PREDICTION_TIME_SEC 2.0  // Look ahead 2 seconds
static double lastAz = 0.0;
static double lastEl = 0.0;
static unsigned long lastPredictionTime = 0;

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
  lastAz = 0.0;
  lastEl = 0.0;
  lastPredictionTime = 0;
}

void updateTracking() {
  // Check for TLE update with proper memory ordering
  if (tleUpdatePending) {
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
    
    // Calculate Julian date for NOW
    double jdNow = dateToJulian(year, month, day, hour, minute, second);
    
    // Find current satellite position
    sat.findsat(jdNow);
    double azNow = sat.satAz;
    double elNow = sat.satEl;
    
    // Calculate satellite velocity (degrees per second)
    unsigned long now = millis();
    double dt = (now - lastPredictionTime) / 1000.0;
    
    double azVelocity = 0.0;
    double elVelocity = 0.0;
    
    if (lastPredictionTime > 0 && dt > 0.01) {  // Valid previous measurement
      // Calculate angular velocity
      azVelocity = (azNow - lastAz) / dt;
      elVelocity = (elNow - lastEl) / dt;
      
      // Handle azimuth wraparound
      if (azVelocity > 180.0) azVelocity -= 360.0;
      if (azVelocity < -180.0) azVelocity += 360.0;
      
      // Sanity check - satellites don't move faster than ~2 deg/sec
      azVelocity = constrain(azVelocity, -2.0, 2.0);
      elVelocity = constrain(elVelocity, -2.0, 2.0);
    }
    
    // Predict future position
    double predictedAz = azNow + (azVelocity * PREDICTION_TIME_SEC);
    double predictedEl = elNow + (elVelocity * PREDICTION_TIME_SEC);
    
    // Normalize azimuth
    while (predictedAz < 0) predictedAz += 360.0;
    while (predictedAz >= 360) predictedAz -= 360.0;
    
    // Update target position with predicted values
    targetPos.azimuth = (float)predictedAz;
    targetPos.elevation = (float)predictedEl;
    targetPos.valid = true;
    
    // Store for next velocity calculation
    lastAz = azNow;
    lastEl = elNow;
    lastPredictionTime = now;
    
    // If satellite below horizon, point to stow position
    if (elNow < 0) {
      targetPos.elevation = 0.0;
      // Could optionally stop tracking when satellite sets
      // trackerState.tracking = false;
    }
    
    // Debug output every 5 seconds
    static unsigned long lastDebug = 0;
    if (now - lastDebug >= 5000) {
      Serial.printf("Track: Az=%.2f El=%.2f (predicted from %.2f,%.2f) Vel: %.3f,%.3f deg/s\n",
                    predictedAz, predictedEl, azNow, elNow, azVelocity, elVelocity);
      lastDebug = now;
    }
  } else if (trackerState.tracking && !trackerState.gpsValid) {
    // GPS was lost during tracking
    Serial.println("Core 1: GPS lost, stopping tracking");
    trackerState.tracking = false;
  }
}
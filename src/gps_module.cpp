// ============================================================================
// gps_module.cpp
// ============================================================================

#include "gps_module.h"

// External references to shared data (defined in shared_data.cpp)
extern TrackerState trackerState;

TinyGPSPlus gps;
static unsigned long lastValidGPS = 0;

TinyGPSPlus& getGPS() {
  return gps;
}

void initGPS() {
  Serial.println("Initializing GPS...");
  
  Serial1.setRX(GPS_RX);
  Serial1.setTX(GPS_TX);
  Serial1.begin(9600);
  
  lastValidGPS = millis();
  
  Serial.println("GPS initialized");
  Serial.println("Waiting for GPS fix...");
}

void updateGPS() {
  bool hadUpdate = false;
  
  // Process all available GPS data
  while (Serial1.available() > 0) {
    if (gps.encode(Serial1.read())) {
      // Check if we have a complete valid fix
      if (gps.location.isValid() && gps.altitude.isValid() && 
          gps.date.isValid() && gps.time.isValid()) {
        
        // Update position
        trackerState.latitude = gps.location.lat();
        trackerState.longitude = gps.location.lng();
        trackerState.altitude = gps.altitude.meters();
        
        // Update time
        trackerState.gpsYear = gps.date.year();
        trackerState.gpsMonth = gps.date.month();
        trackerState.gpsDay = gps.date.day();
        trackerState.gpsHour = gps.time.hour();
        trackerState.gpsMinute = gps.time.minute();
        trackerState.gpsSecond = gps.time.second();
        
        // Mark as valid and update timestamp
        if (!trackerState.gpsValid) {
          Serial.println("GPS fix acquired!");
          Serial.print("Location: ");
          Serial.print(trackerState.latitude, 6);
          Serial.print(", ");
          Serial.println(trackerState.longitude, 6);
        }
        
        trackerState.gpsValid = true;
        lastValidGPS = millis();
        hadUpdate = true;
      }
    }
  }
  
  // Check for GPS timeout
  if (trackerState.gpsValid) {
    unsigned long timeSinceLastFix = millis() - lastValidGPS;
    
    if (timeSinceLastFix > GPS_TIMEOUT_MS) {
      Serial.println("WARNING: GPS fix lost (timeout)");
      trackerState.gpsValid = false;
      
      // Stop tracking if GPS is lost
      if (trackerState.tracking) {
        Serial.println("Stopping tracking due to GPS loss");
        trackerState.tracking = false;
      }
    }
  }
}
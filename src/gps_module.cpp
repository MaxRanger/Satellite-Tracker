// ============================================================================
// gps_module.cpp
// ============================================================================

#include "gps_module.h"

// External references to shared data (defined in shared_data.cpp)
extern TrackerState trackerState;

TinyGPSPlus gps;

TinyGPSPlus& getGPS() {
  return gps;
}

void initGPS() {
  Serial.println("Initializing GPS...");
  
  Serial1.setRX(GPS_RX);
  Serial1.setTX(GPS_TX);
  Serial1.begin(9600);
  
  Serial.println("GPS initialized");
}

void updateGPS() {
  while (Serial1.available() > 0) {
    if (gps.encode(Serial1.read())) {
      if (gps.location.isValid() && gps.altitude.isValid() && 
          gps.date.isValid() && gps.time.isValid()) {
        
        trackerState.latitude = gps.location.lat();
        trackerState.longitude = gps.location.lng();
        trackerState.altitude = gps.altitude.meters();
        
        trackerState.gpsYear = gps.date.year();
        trackerState.gpsMonth = gps.date.month();
        trackerState.gpsDay = gps.date.day();
        trackerState.gpsHour = gps.time.hour();
        trackerState.gpsMinute = gps.time.minute();
        trackerState.gpsSecond = gps.time.second();
        
        trackerState.gpsValid = true;
      }
    }
  }
}
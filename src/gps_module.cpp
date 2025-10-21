// ============================================================================
// gps_module.cpp
// ============================================================================

#include "gps_module.h"

// External references to shared data (defined in shared_data.cpp)
extern TrackerState trackerState;

TinyGPSPlus gps;
static unsigned long lastValidGPS = 0;

// Use Serial2 for GPS (UART0 on GPIO 0/1)
#define GPS_SERIAL Serial1

TinyGPSPlus& getGPS() {
  return gps;
}

void dumpGPSData() {
  Serial.print("GPS: ");
  Serial.print("Loc-");
  Serial.print(gps.location.isValid() ? "V" : "I");
  Serial.print(" Alt-");
  Serial.print(gps.altitude.isValid() ? "V" : "I");
  Serial.print(" Time-");
  Serial.print(gps.time.isValid() ? "V" : "I");
  Serial.print(" Date-");
  Serial.print(gps.date.isValid() ? "V" : "I");
  
  if (gps.location.isValid()) {
    Serial.print(" (");
    Serial.print(gps.location.lat(), 6);
    Serial.print(",");
    Serial.print(gps.location.lng(), 6);
    Serial.print(")");
  }
  
  if (gps.satellites.isValid()) {
    Serial.print(" Sats:");
    Serial.print(gps.satellites.value());
  }
  
  Serial.println();
}

void initGPS() {
  Serial.println("Initializing GPS...");
  
  GPS_SERIAL.setRX(GPS_RX);
  GPS_SERIAL.setTX(GPS_TX);
  GPS_SERIAL.begin(9600);
  
  lastValidGPS = millis();
  
  Serial.println("GPS initialized on Serial1 (GPIO 0/1)");
  Serial.println("Waiting for GPS fix...");
}

void updateGPS() {
  // Process all available GPS data
  while (GPS_SERIAL.available() > 0) {
    if (gps.encode(GPS_SERIAL.read())) {
      
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
        // Only print on initial acquisition
        if (!trackerState.gpsValid) {
          Serial.println("GPS fix acquired!");
          Serial.print("Location: ");
          Serial.print(trackerState.latitude, 6);
          Serial.print(", ");
          Serial.println(trackerState.longitude, 6);
        }
        
        trackerState.gpsValid = true;
        lastValidGPS = millis();
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
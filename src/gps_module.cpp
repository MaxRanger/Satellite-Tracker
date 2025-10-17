// ============================================================================
// gps_module.cpp
// ============================================================================

#include "gps_module.h"

// External references to shared data (defined in shared_data.cpp)
extern TrackerState trackerState;

TinyGPSPlus gps;
static unsigned long lastValidGPS = 0;

// Use Serial2 for GPS (UART0 on GPIO 0/1)
// Serial is USB, Serial1 is typically UART0, Serial2 is UART1
// On RP2350 we need to configure UART0 on GPIO 0/1
#define GPS_SERIAL Serial1

TinyGPSPlus& getGPS() {
  return gps;
}

void dumpGPSData();

void initGPS() {
  Serial.println("Initializing GPS...");
  
  GPS_SERIAL.setRX(GPS_RX);
  GPS_SERIAL.setTX(GPS_TX);
  GPS_SERIAL.begin(9600);
  
  lastValidGPS = millis();
  
  Serial.println("GPS initialized on Serial 1 (GPIO 0/1)");
  Serial.println("Waiting for GPS fix...");
}

void updateGPS() {
  bool hadUpdate = false;
  
  // Process all available GPS data
  while (GPS_SERIAL.available() > 0) {
    if (gps.encode(GPS_SERIAL.read())) {

      dumpGPSData();

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

void dumpGPSData() {
  Serial.print("GPS data: ");
  Serial.print("Location-");
  Serial.print(gps.location.isValid() ? "VALID, " : "INVALID, ");
  Serial.print("Altitude-");
  Serial.print(gps.altitude.isValid() ? "VALID, " : "INVALID, ");
  Serial.print("Time-");
  Serial.print(gps.time.isValid() ? "VALID, " : "INVALID, ");
  Serial.print("Date-");
  Serial.println(gps.date.isValid() ? "VALID, " : "INVALID, ");
        
  Serial.print("Location (lat,lon,alt): ");
  Serial.print(gps.location.lat(), 6);
  Serial.print(", ");
  Serial.print(gps.location.lng(), 6);
  Serial.print(", ");
  Serial.println(gps.altitude.meters(), 6);

  Serial.print("Date: (yr,mo,day): ");
  Serial.print(gps.date.year());
  Serial.print("/");
  Serial.print(gps.date.month());
  Serial.print("/");
  Serial.println(gps.date.day());

  Serial.print("Time: (HH,MM,SS): ");
  Serial.print(gps.time.hour());
  Serial.print(":");
  Serial.print(gps.time.minute());
  Serial.print(":");
  Serial.println(gps.time.second());
}
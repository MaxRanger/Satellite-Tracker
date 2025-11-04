// ============================================================================
// gps_module.cpp
// ============================================================================

#include "gps_module.h"

// External references to shared data (defined in shared_data.cpp)
extern TrackerState trackerState;

TinyGPSPlus gps;
static unsigned long lastValidGPS = 0;

// Use Serial2 for GPS (UART0 on GPIO 0/1)
#define GPS_SERIAL Serial1  // UART0 on Pico (GP0=TX, GP1=RX)
#define GPS_BAUD 9600



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
  GPS_SERIAL.begin(GPS_BAUD);
  
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


void printTLE() {
  Serial.println(F("\n=== TLE DATA ==="));
  Serial.println();
  
  if (!trackerState.tleValid) {
    Serial.println(F("No TLE loaded"));
    Serial.println();
    return;
  }
  
  Serial.print(F("Satellite: "));
  Serial.println(satelliteName);
  Serial.println(tleLine1);
  Serial.println(tleLine2);
  Serial.println();
}

void printGPSStatus() {
  Serial.println(F("\n=== GPS STATUS ==="));
  Serial.println();
  
  TinyGPSPlus& gps = getGPS();
  
  Serial.print(F("Fix Valid:     "));
  Serial.println(trackerState.gpsValid ? F("YES") : F("NO"));
  
  Serial.print(F("Satellites:    "));
  Serial.println(gps.satellites.isValid() ? gps.satellites.value() : 0);
  
  Serial.print(F("HDOP:          "));
  if (gps.hdop.isValid()) {
    Serial.println(gps.hdop.hdop());
  } else {
    Serial.println(F("N/A"));
  }
  
  if (gps.location.isValid()) {
    Serial.printf("Latitude:      %.6f°\n", gps.location.lat());
    Serial.printf("Longitude:     %.6f°\n", gps.location.lng());
  }
  
  if (gps.altitude.isValid()) {
    Serial.printf("Altitude:      %.1f m\n", gps.altitude.meters());
  }
  
  if (gps.date.isValid() && gps.time.isValid()) {
    Serial.printf("Date/Time:     %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                  gps.date.year(), gps.date.month(), gps.date.day(),
                  gps.time.hour(), gps.time.minute(), gps.time.second());
  }
  
  if (gps.speed.isValid()) {
    Serial.printf("Speed:         %.2f m/s\n", gps.speed.mps());
  }
  
  if (gps.course.isValid()) {
    Serial.printf("Course:        %.2f°\n", gps.course.deg());
  }
  
  Serial.printf("\nCharacters:    %lu\n", gps.charsProcessed());
  Serial.printf("Sentences:     %lu (failed: %lu)\n", 
                gps.sentencesWithFix(), gps.failedChecksum());
  
  Serial.println();
}





// ============================================================================



/*
 * GPS Debugging System for Raspberry Pi Pico 2W (Arduino Framework)
 * Supports multiple testing strategies for indoor development
 * 
 * Wiring:
 * GPS TX -> Pico GP1 (UART0 RX)
 * GPS RX -> Pico GP0 (UART0 TX)
 * GPS VCC -> Pico 3.3V
 * GPS GND -> Pico GND
 */

// NMEA sentence types tracking
struct SentenceStats {
  int gga_count = 0;
  int rmc_count = 0;
  int gsv_count = 0;
  int gsa_count = 0;
  int vtg_count = 0;
  int gll_count = 0;
  int other_count = 0;
};

SentenceStats stats;
String inputBuffer = "";

// Function prototypes
void connectionTest();
void readRawData(int durationSec);
void analyzeSentences(int durationSec);
void processSentence(String sentence);
void parseGGA(String sentence);
void parseRMC(String sentence);
void parseGSV(String sentence);
void parseGSA(String sentence);
void printSummary();
void injectTestData();
void waitForFixAttempt(int timeoutSec);
String extractField(String data, int fieldNum);
int countFields(String data);

#define DEBUG_SERIAL Serial

void connectionTest() {
  DEBUG_SERIAL.println("\n=== GPS Module Connection Test ===");
  DEBUG_SERIAL.println("Checking if GPS module is communicating...\n");
  
  delay(1000);
  
  if (GPS_SERIAL.available()) {
    DEBUG_SERIAL.println("✓ Data detected on UART!");
    DEBUG_SERIAL.print("Sample: ");
    for (int i = 0; i < 50 && GPS_SERIAL.available(); i++) {
      DEBUG_SERIAL.write(GPS_SERIAL.read());
    }
    DEBUG_SERIAL.println("\n");
  } else {
    DEBUG_SERIAL.println("✗ No data on UART");
    DEBUG_SERIAL.println("\nTroubleshooting:");
    DEBUG_SERIAL.println("1. Check wiring (GPS TX->Pico GP1, GPS RX->Pico GP0)");
    DEBUG_SERIAL.println("2. Verify power to GPS module (3.3V)");
    DEBUG_SERIAL.println("3. Check baudrate (try 9600 or 115200)");
    DEBUG_SERIAL.println("4. Confirm GPS module has power LED on");
    DEBUG_SERIAL.println("5. Wait 30-60s for GPS cold start");
  }
}

void readRawData(int durationSec) {
  DEBUG_SERIAL.println("\n=== Reading raw GPS data for " + String(durationSec) + " seconds ===");
  DEBUG_SERIAL.println("You should see NMEA sentences even without fix");
  DEBUG_SERIAL.println("Looking for lines starting with $GP, $GN, $GL, etc.\n");
  
  unsigned long startTime = millis();
  int lineCount = 0;
  
  while ((millis() - startTime) < (durationSec * 1000UL)) {
    if (GPS_SERIAL.available()) {
      char c = GPS_SERIAL.read();
      DEBUG_SERIAL.write(c);
      if (c == '\n') lineCount++;
    }
  }
  
  DEBUG_SERIAL.println("\n\n=== Received " + String(lineCount) + " lines ===");
}

void analyzeSentences(int durationSec) {
  DEBUG_SERIAL.println("\n=== Analyzing NMEA sentences for " + String(durationSec) + " seconds ===\n");
  
  inputBuffer = "";
  unsigned long startTime = millis();
  
  while ((millis() - startTime) < (durationSec * 1000UL)) {
    if (GPS_SERIAL.available()) {
      char c = GPS_SERIAL.read();
      
      if (c == '\n' || c == '\r') {
        if (inputBuffer.length() > 0 && inputBuffer.startsWith("$")) {
          processSentence(inputBuffer);
        }
        inputBuffer = "";
      } else {
        inputBuffer += c;
      }
    }
  }
  
  printSummary();
}

void processSentence(String sentence) {
  sentence.trim();
  
  // Count sentence types
  if (sentence.indexOf("GGA") > 0) {
    stats.gga_count++;
    parseGGA(sentence);
  } else if (sentence.indexOf("RMC") > 0) {
    stats.rmc_count++;
    parseRMC(sentence);
  } else if (sentence.indexOf("GSV") > 0) {
    stats.gsv_count++;
    parseGSV(sentence);
  } else if (sentence.indexOf("GSA") > 0) {
    stats.gsa_count++;
    parseGSA(sentence);
  } else if (sentence.indexOf("VTG") > 0) {
    stats.vtg_count++;
  } else if (sentence.indexOf("GLL") > 0) {
    stats.gll_count++;
  } else {
    stats.other_count++;
  }
}

String extractField(String data, int fieldNum) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;
  
  for (int i = 0; i <= maxIndex && found <= fieldNum; i++) {
    if (data.charAt(i) == ',' || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  
  return found > fieldNum ? data.substring(strIndex[0], strIndex[1]) : "";
}

int countFields(String data) {
  int count = 1;
  for (unsigned int i = 0; i < data.length(); i++) {
    if (data.charAt(i) == ',') count++;
  }
  return count;
}

void parseGGA(String sentence) {
  // $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
  String timeStr = extractField(sentence, 1);
  String lat = extractField(sentence, 2);
  String latDir = extractField(sentence, 3);
  String lon = extractField(sentence, 4);
  String lonDir = extractField(sentence, 5);
  String fix = extractField(sentence, 6);
  String sats = extractField(sentence, 7);
  
  if (timeStr.length() >= 6) {
    DEBUG_SERIAL.print("GGA: Time=");
    DEBUG_SERIAL.print(timeStr.substring(0, 6));
    DEBUG_SERIAL.print(", Fix=");
    DEBUG_SERIAL.print(fix);
    DEBUG_SERIAL.print(", Sats=");
    DEBUG_SERIAL.print(sats);
    DEBUG_SERIAL.print(", Lat=");
    DEBUG_SERIAL.print(lat);
    DEBUG_SERIAL.print(latDir);
    DEBUG_SERIAL.print(", Lon=");
    DEBUG_SERIAL.print(lon);
    DEBUG_SERIAL.println(lonDir);
  }
}

void parseRMC(String sentence) {
  // $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
  String timeStr = extractField(sentence, 1);
  String status = extractField(sentence, 2);
  
  if (timeStr.length() >= 6) {
    DEBUG_SERIAL.print("RMC: Time=");
    DEBUG_SERIAL.print(timeStr.substring(0, 6));
    DEBUG_SERIAL.print(", Status=");
    DEBUG_SERIAL.println(status == "A" ? "VALID" : "INVALID");
  }
}

void parseGSV(String sentence) {
  // $GPGSV,3,1,12,01,45,234,42,02,30,127,38,...
  String totalMsgs = extractField(sentence, 1);
  String msgNum = extractField(sentence, 2);
  String satsInView = extractField(sentence, 3);
  
  if (msgNum == "1") { // Only print on first message
    DEBUG_SERIAL.print("GSV: ");
    DEBUG_SERIAL.print(satsInView);
    DEBUG_SERIAL.println(" satellites in view");
  }
}

void parseGSA(String sentence) {
  // $GPGSA,A,3,01,02,03,04,05,06,07,08,09,10,11,12,1.0,1.0,1.0*30
  String fixType = extractField(sentence, 2);
  
  DEBUG_SERIAL.print("GSA: Fix type=");
  DEBUG_SERIAL.print(fixType);
  DEBUG_SERIAL.println(" (1=none, 2=2D, 3=3D)");
}

void printSummary() {
  DEBUG_SERIAL.println("\n=== NMEA Sentence Summary ===");
  
  int total = stats.gga_count + stats.rmc_count + stats.gsv_count + 
              stats.gsa_count + stats.vtg_count + stats.gll_count + stats.other_count;
  
  if (total > 0) {
    if (stats.gga_count > 0) {
      DEBUG_SERIAL.println("$xxGGA (Position): " + String(stats.gga_count) + " sentences");
    }
    if (stats.rmc_count > 0) {
      DEBUG_SERIAL.println("$xxRMC (Recommended minimum): " + String(stats.rmc_count) + " sentences");
    }
    if (stats.gsv_count > 0) {
      DEBUG_SERIAL.println("$xxGSV (Satellites in view): " + String(stats.gsv_count) + " sentences");
    }
    if (stats.gsa_count > 0) {
      DEBUG_SERIAL.println("$xxGSA (DOP and active sats): " + String(stats.gsa_count) + " sentences");
    }
    if (stats.vtg_count > 0) {
      DEBUG_SERIAL.println("$xxVTG (Track/speed): " + String(stats.vtg_count) + " sentences");
    }
    if (stats.gll_count > 0) {
      DEBUG_SERIAL.println("$xxGLL (Geographic position): " + String(stats.gll_count) + " sentences");
    }
    if (stats.other_count > 0) {
      DEBUG_SERIAL.println("Other sentences: " + String(stats.other_count));
    }
    DEBUG_SERIAL.println("\nTotal: " + String(total) + " sentences");
  } else {
    DEBUG_SERIAL.println("No NMEA sentences received!");
    DEBUG_SERIAL.println("\nPossible issues:");
    DEBUG_SERIAL.println("- GPS still performing cold start (wait 60s)");
    DEBUG_SERIAL.println("- Wrong baud rate");
    DEBUG_SERIAL.println("- Incorrect wiring");
  }
  DEBUG_SERIAL.println();
}

void injectTestData() {
  DEBUG_SERIAL.println("\n=== Injecting test NMEA data ===");
  DEBUG_SERIAL.println("This simulates GPS data for parser testing\n");
  
  const char* testSentences[] = {
    "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47",
    "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A",
    "$GPGGA,123520,,,,,0,00,,,M,,M,,*46",
    "$GPRMC,123520,V,,,,,,,230394,,,N*71",
    "$GPGSV,3,1,12,01,45,234,42,02,30,127,38,03,15,045,35,04,60,315,40*7E",
    "$GPGSA,A,3,01,02,03,04,05,06,07,08,09,10,11,12,1.0,1.0,1.0*30"
  };
  
  int numSentences = sizeof(testSentences) / sizeof(testSentences[0]);
  
  for (int i = 0; i < numSentences; i++) {
    DEBUG_SERIAL.print("Injecting: ");
    DEBUG_SERIAL.println(testSentences[i]);
    processSentence(String(testSentences[i]));
    delay(200);
  }
}

void waitForFixAttempt(int timeoutSec) {
  DEBUG_SERIAL.println("\n=== Waiting for GPS fix (timeout: " + String(timeoutSec) + "s) ===");
  DEBUG_SERIAL.println("Take device outdoors with clear sky view\n");
  
  unsigned long startTime = millis();
  String buffer = "";
  bool fixAcquired = false;
  
  while ((millis() - startTime) < (timeoutSec * 1000UL) && !fixAcquired) {
    if (GPS_SERIAL.available()) {
      char c = GPS_SERIAL.read();
      
      if (c == '\n' || c == '\r') {
        if (buffer.length() > 0 && buffer.startsWith("$")) {
          if (buffer.indexOf("GGA") > 0) {
            String fix = extractField(buffer, 6);
            String sats = extractField(buffer, 7);
            
            if (fix == "1" || fix == "2") {
              DEBUG_SERIAL.println("✓ FIX ACQUIRED! Type: " + fix);
              DEBUG_SERIAL.println("Full sentence: " + buffer);
              fixAcquired = true;
            } else {
              int elapsed = (millis() - startTime) / 1000;
              DEBUG_SERIAL.print("[");
              DEBUG_SERIAL.print(elapsed);
              DEBUG_SERIAL.print("s] Waiting... Sats: ");
              DEBUG_SERIAL.print(sats);
              DEBUG_SERIAL.print(", Fix: ");
              DEBUG_SERIAL.println(fix);
            }
          }
        }
        buffer = "";
      } else {
        buffer += c;
      }
    }
    
    delay(100);
  }
  
  if (!fixAcquired) {
    DEBUG_SERIAL.println("\n✗ No fix acquired - this is normal indoors!");
  }
}
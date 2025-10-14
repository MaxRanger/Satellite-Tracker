// ============================================================================
// web_interface.cpp
// ============================================================================

#include "web_interface.h"

// External references to shared data (defined in shared_data.cpp)
extern MotorPosition motorPos;
extern TargetPosition targetPos;
extern TrackerState trackerState;
extern char tleLine1[70];
extern char tleLine2[70];
extern char satelliteName[25];
extern volatile bool tleUpdatePending;
extern char wifiSSID[32];
extern char wifiPassword[64];
extern bool wifiConfigured;

WebServer server(80);

void handleRoot() {
  float currentEl = motorPos.elevation * DEGREES_PER_PULSE;
  float currentAz = motorPos.azimuth * DEGREES_PER_PULSE;
  while (currentAz < 0) currentAz += 360.0;
  while (currentAz >= 360) currentAz -= 360.0;
  
  String html = "<!DOCTYPE html><html><head><title>Sat Tracker</title>";
  html += "<meta http-equiv='refresh' content='2'>";
  html += "<style>body{font-family:Arial;margin:20px;}";
  html += "table{border-collapse:collapse;}td,th{border:1px solid #ddd;padding:8px;}";
  html += "input[type=text]{width:500px;}</style></head><body>";
  html += "<h1>Satellite Tracker Control</h1>";
  
  html += "<h2>Status</h2><table>";
  html += "<tr><td>GPS Valid</td><td>" + String(trackerState.gpsValid ? "Yes" : "No") + "</td></tr>";
  html += "<tr><td>Location</td><td>" + String(trackerState.latitude, 6) + ", " + String(trackerState.longitude, 6) + "</td></tr>";
  html += "<tr><td>Altitude</td><td>" + String(trackerState.altitude, 1) + " m</td></tr>";
  html += "<tr><td>Time (UTC)</td><td>" + String(trackerState.gpsYear) + "-" + 
          String(trackerState.gpsMonth) + "-" + String(trackerState.gpsDay) + " " +
          String(trackerState.gpsHour) + ":" + String(trackerState.gpsMinute) + ":" + 
          String(trackerState.gpsSecond) + "</td></tr>";
  html += "<tr><td>TLE Loaded</td><td>" + String(trackerState.tleValid ? "Yes" : "No") + "</td></tr>";
  html += "<tr><td>Tracking</td><td>" + String(trackerState.tracking ? "Active" : "Idle") + "</td></tr>";
  html += "</table>";
  
  html += "<h2>Position</h2><table>";
  html += "<tr><td>Current Azimuth</td><td>" + String(currentAz, 2) + "&deg;</td></tr>";
  html += "<tr><td>Current Elevation</td><td>" + String(currentEl, 2) + "&deg;</td></tr>";
  html += "<tr><td>Target Azimuth</td><td>" + String(targetPos.azimuth, 2) + "&deg;</td></tr>";
  html += "<tr><td>Target Elevation</td><td>" + String(targetPos.elevation, 2) + "&deg;</td></tr>";
  html += "</table>";
  
  html += "<h2>Commands</h2>";
  html += "<form action='/tle' method='POST'>";
  html += "Satellite Name: <input type='text' name='name' value='" + String(satelliteName) + "'><br><br>";
  html += "TLE Line 1: <input type='text' name='line1' value='" + String(tleLine1) + "'><br><br>";
  html += "TLE Line 2: <input type='text' name='line2' value='" + String(tleLine2) + "'><br><br>";
  html += "<input type='submit' value='Update TLE and Track'></form><br>";
  
  html += "<form action='/home' method='POST'>";
  html += "<input type='submit' value='Home Axes'></form><br>";
  
  html += "<form action='/stop' method='POST'>";
  html += "<input type='submit' value='Stop Tracking'></form>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleTLE() {
  if (server.hasArg("name") && server.hasArg("line1") && server.hasArg("line2")) {
    String name = server.arg("name");
    String line1 = server.arg("line1");
    String line2 = server.arg("line2");
    
    if (line1.length() == 69 && line2.length() == 69) {
      name.toCharArray(satelliteName, 25);
      line1.toCharArray(tleLine1, 70);
      line2.toCharArray(tleLine2, 70);
      tleUpdatePending = true;
      
      server.send(200, "text/plain", "TLE updated - Core 1 will initialize tracking");
      Serial.println("TLE received, signaling Core 1");
    } else {
      server.send(400, "text/plain", "Invalid TLE format (lines must be 69 chars)");
    }
  } else {
    server.send(400, "text/plain", "Missing parameters");
  }
}

void handleHome() {
  trackerState.tracking = false;
  targetPos.elevation = 0.0;
  targetPos.azimuth = 0.0;
  delay(100);
  homeAxes();
  server.send(200, "text/plain", "Homing complete");
}

void handleStop() {
  trackerState.tracking = false;
  stopAllMotors();
  server.send(200, "text/plain", "Tracking stopped");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void initWebInterface() {
  Serial.println("Initializing web interface...");
  
  // Only connect if WiFi is configured
  if (!wifiConfigured || strlen(wifiSSID) == 0) {
    Serial.println("WiFi not configured - skipping connection");
    Serial.println("Use display setup screen to configure WiFi");
    return;
  }
  
  WiFi.begin(wifiSSID, wifiPassword);
  Serial.print("Connecting to WiFi: ");
  Serial.println(wifiSSID);
  
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed");
    Serial.println("Check credentials on display setup screen");
    wifiConfigured = false;  // Mark as failed so user can reconfigure
    return;
  }
  
  server.on("/", handleRoot);
  server.on("/tle", HTTP_POST, handleTLE);
  server.on("/home", HTTP_POST, handleHome);
  server.on("/stop", HTTP_POST, handleStop);
  server.onNotFound(handleNotFound);
  server.begin();
  
  Serial.println("Web server started");
}

void handleWebClient() {
  // Only handle web requests if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }
}
// ============================================================================
// web_interface.cpp
// ============================================================================

#include "web_interface.h"
#include <LEAmDNS.h>

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

// Authentication credentials (TODO: Move to EEPROM/config)
const char* www_username = "admin";
const char* www_password = "changeme";  // Change this!

// HTML sanitization to prevent XSS
String sanitizeHTML(const String& input) {
  String output = input;
  output.replace("&", "&amp;");
  output.replace("<", "&lt;");
  output.replace(">", "&gt;");
  output.replace("\"", "&quot;");
  output.replace("'", "&#39;");
  return output;
}

void handleRoot() {
  // Require authentication
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }
  
  // Sanitize user-provided data
  String safeSatName = sanitizeHTML(String(satelliteName));
  String safeTLE1 = sanitizeHTML(String(tleLine1));
  String safeTLE2 = sanitizeHTML(String(tleLine2));
  
  String html = "<!DOCTYPE html><html><head><title>Sat Tracker</title>";
  html += "<meta charset='UTF-8'>";
  html += "<style>body{font-family:Arial;margin:20px;background:#f0f0f0;}";
  html += "table{border-collapse:collapse;background:white;}td,th{border:1px solid #ddd;padding:8px;}";
  html += "th{background:#4CAF50;color:white;}";
  html += "input[type=text]{width:500px;max-width:100%;padding:5px;}";
  html += "input[type=submit]{background:#4CAF50;color:white;padding:10px 20px;border:none;cursor:pointer;margin:5px;}";
  html += "input[type=submit]:hover{background:#45a049;}";
  html += ".status-good{color:green;font-weight:bold;}";
  html += ".status-bad{color:red;font-weight:bold;}";
  html += "h1,h2{color:#333;}</style>";
  
  // JavaScript for AJAX updates
  html += "<script>";
  html += "function updateStatus(){";
  html += "  fetch('/status').then(r=>r.json()).then(d=>{";
  html += "    document.getElementById('gpsValid').textContent=d.gpsValid?'Yes':'No';";
  html += "    document.getElementById('gpsValid').className=d.gpsValid?'status-good':'status-bad';";
  html += "    document.getElementById('location').textContent=d.lat.toFixed(6)+', '+d.lon.toFixed(6);";
  html += "    document.getElementById('altitude').textContent=d.alt.toFixed(1)+' m';";
  html += "    document.getElementById('time').textContent=d.time;";
  html += "    document.getElementById('tleLoaded').textContent=d.tleValid?'Yes':'No';";
  html += "    document.getElementById('tleLoaded').className=d.tleValid?'status-good':'status-bad';";
  html += "    document.getElementById('tracking').textContent=d.tracking?'Active':'Idle';";
  html += "    document.getElementById('tracking').className=d.tracking?'status-good':'status-bad';";
  html += "    document.getElementById('currentAz').textContent=d.curAz.toFixed(2)+'°';";
  html += "    document.getElementById('currentEl').textContent=d.curEl.toFixed(2)+'°';";
  html += "    document.getElementById('targetAz').textContent=d.tgtAz.toFixed(2)+'°';";
  html += "    document.getElementById('targetEl').textContent=d.tgtEl.toFixed(2)+'°';";
  html += "  }).catch(e=>console.log('Update failed',e));";
  html += "}";
  html += "setInterval(updateStatus,1000);"; // Update every 1 second
  html += "window.onload=updateStatus;";
  html += "</script>";
  
  html += "</head><body>";
  html += "<h1>Satellite Tracker Control</h1>";
  
  html += "<h2>Status</h2><table>";
  html += "<tr><td>GPS Valid</td><td id='gpsValid'>...</td></tr>";
  html += "<tr><td>Location</td><td id='location'>...</td></tr>";
  html += "<tr><td>Altitude</td><td id='altitude'>...</td></tr>";
  html += "<tr><td>Time (UTC)</td><td id='time'>...</td></tr>";
  html += "<tr><td>TLE Loaded</td><td id='tleLoaded'>...</td></tr>";
  html += "<tr><td>Tracking</td><td id='tracking'>...</td></tr>";
  html += "</table>";
  
  html += "<h2>Position</h2><table>";
  html += "<tr><td>Current Azimuth</td><td id='currentAz'>...</td></tr>";
  html += "<tr><td>Current Elevation</td><td id='currentEl'>...</td></tr>";
  html += "<tr><td>Target Azimuth</td><td id='targetAz'>...</td></tr>";
  html += "<tr><td>Target Elevation</td><td id='targetEl'>...</td></tr>";
  html += "</table>";
  
  html += "<h2>Commands</h2>";
  html += "<form action='/tle' method='POST'>";
  html += "Satellite Name: <input type='text' name='name' value='" + safeSatName + "' maxlength='24'><br><br>";
  html += "TLE Line 1: <input type='text' name='line1' value='" + safeTLE1 + "' maxlength='69'><br><br>";
  html += "TLE Line 2: <input type='text' name='line2' value='" + safeTLE2 + "' maxlength='69'><br><br>";
  html += "<input type='submit' value='Update TLE and Track'></form><br>";
  
  html += "<form action='/home' method='POST'>";
  html += "<input type='submit' value='Home Axes'></form><br>";
  
  html += "<form action='/stop' method='POST'>";
  html += "<input type='submit' value='Stop Tracking'></form>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

void handleStatus() {
  // Require authentication
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }
  
  float currentEl = motorPos.elevation * DEGREES_PER_PULSE;
  float currentAz = motorPos.azimuth * DEGREES_PER_PULSE;
  while (currentAz < 0) currentAz += 360.0;
  while (currentAz >= 360) currentAz -= 360.0;
  
  // Build JSON response
  String json = "{";
  json += "\"gpsValid\":" + String(trackerState.gpsValid ? "true" : "false") + ",";
  json += "\"lat\":" + String(trackerState.latitude, 6) + ",";
  json += "\"lon\":" + String(trackerState.longitude, 6) + ",";
  json += "\"alt\":" + String(trackerState.altitude, 1) + ",";
  json += "\"time\":\"" + String(trackerState.gpsYear) + "-" + 
          String(trackerState.gpsMonth) + "-" + String(trackerState.gpsDay) + " " +
          String(trackerState.gpsHour) + ":" + String(trackerState.gpsMinute) + ":" + 
          String(trackerState.gpsSecond) + "\",";
  json += "\"tleValid\":" + String(trackerState.tleValid ? "true" : "false") + ",";
  json += "\"tracking\":" + String(trackerState.tracking ? "true" : "false") + ",";
  json += "\"curAz\":" + String(currentAz, 2) + ",";
  json += "\"curEl\":" + String(currentEl, 2) + ",";
  json += "\"tgtAz\":" + String(targetPos.azimuth, 2) + ",";
  json += "\"tgtEl\":" + String(targetPos.elevation, 2);
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleTLE() {
  // Require authentication
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }
  
  if (!server.hasArg("name") || !server.hasArg("line1") || !server.hasArg("line2")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }
  
  String name = server.arg("name");
  String line1 = server.arg("line1");
  String line2 = server.arg("line2");
  
  // Input validation - length checks
  if (name.length() == 0 || name.length() >= sizeof(satelliteName)) {
    server.send(400, "text/plain", "Invalid satellite name (max 24 chars)");
    return;
  }
  
  if (line1.length() != 69 || line2.length() != 69) {
    server.send(400, "text/plain", "Invalid TLE format (lines must be 69 chars)");
    return;
  }
  
  // Basic TLE format validation
  if (line1[0] != '1' || line2[0] != '2') {
    server.send(400, "text/plain", "Invalid TLE format (must start with '1' and '2')");
    return;
  }
  
  // Safe copy with bounds checking and null termination
  memset(satelliteName, 0, sizeof(satelliteName));
  strncpy(satelliteName, name.c_str(), sizeof(satelliteName) - 1);
  satelliteName[sizeof(satelliteName) - 1] = '\0';
  
  memset(tleLine1, 0, sizeof(tleLine1));
  strncpy(tleLine1, line1.c_str(), sizeof(tleLine1) - 1);
  tleLine1[sizeof(tleLine1) - 1] = '\0';
  
  memset(tleLine2, 0, sizeof(tleLine2));
  strncpy(tleLine2, line2.c_str(), sizeof(tleLine2) - 1);
  tleLine2[sizeof(tleLine2) - 1] = '\0';
  
  // Memory barrier to ensure all writes complete before setting flag
  // This prevents Core 1 from seeing tleUpdatePending=true before
  // the TLE data is fully written
  __dmb();
  
  tleUpdatePending = true;
  
  server.send(200, "text/plain", "TLE updated - Core 1 will initialize tracking");
  Serial.println("TLE received, signaling Core 1");
}

void handleHome() {
  // Require authentication
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }
  
  trackerState.tracking = false;
  targetPos.elevation = 0.0;
  targetPos.azimuth = 0.0;
  delay(100);
  homeAxes();
  server.send(200, "text/plain", "Homing complete");
}

void handleStop() {
  // Require authentication
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }
  
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

    // Start mDNS
    if (MDNS.begin("sattracker")) {
      Serial.println("mDNS responder started");
      Serial.println("Access at: http://sattracker.local");
      MDNS.addService("http", "tcp", 80);
    }

    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("Default credentials: admin / changeme");
    Serial.println("CHANGE THE PASSWORD!");
  } else {
    Serial.println("\nWiFi connection failed");
    Serial.println("Check credentials on display setup screen");
    wifiConfigured = false;
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
    MDNS.update();  // Keep mDNS alive
    server.handleClient();
  }
}
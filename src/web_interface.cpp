// ============================================================================
// web_interface.cpp - FIXED: Secure credential storage
// ============================================================================

#include "web_interface.h"
#include "storage_module.h"  // Added for secure storage
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

// Credentials are loaded from flash storage on initialization
char www_username[32] = "";
char www_password[64] = "";
bool credentialsConfigured = false;

// Default credentials for first-time setup only
const char* DEFAULT_USERNAME = "admin";
const char* DEFAULT_PASSWORD = "setup";  // Temporary, must be changed

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

bool loadWebCredentials() {
  StorageConfig config = {0};
  
  if (!loadConfig(&config)) {
    Serial.println("No saved web credentials - using defaults");
    strncpy(www_username, DEFAULT_USERNAME, sizeof(www_username) - 1);
    strncpy(www_password, DEFAULT_PASSWORD, sizeof(www_password) - 1);
    credentialsConfigured = false;
    return false;
  }
  
  // Check if web credentials are stored
  if (strlen(config.webUsername) > 0 && strlen(config.webPassword) > 0) {
    strncpy(www_username, config.webUsername, sizeof(www_username) - 1);
    strncpy(www_password, config.webPassword, sizeof(www_password) - 1);
    www_username[sizeof(www_username) - 1] = '\0';
    www_password[sizeof(www_password) - 1] = '\0';
    credentialsConfigured = true;
    Serial.println("Web credentials loaded from storage");
    return true;
  }
  
  // Use defaults if not configured
  strncpy(www_username, DEFAULT_USERNAME, sizeof(www_username) - 1);
  strncpy(www_password, DEFAULT_PASSWORD, sizeof(www_password) - 1);
  credentialsConfigured = false;
  Serial.println("Using default credentials - PLEASE CHANGE!");
  return false;
}

bool saveWebCredentials(const char* username, const char* password) {
  StorageConfig config = {0};
  loadConfig(&config);
  
  // Update web credentials
  strncpy(config.webUsername, username, sizeof(config.webUsername) - 1);
  config.webUsername[sizeof(config.webUsername) - 1] = '\0';
  
  strncpy(config.webPassword, password, sizeof(config.webPassword) - 1);
  config.webPassword[sizeof(config.webPassword) - 1] = '\0';
  
  // Save to storage
  if (saveConfig(&config)) {
    // Update runtime variables
    strncpy(www_username, username, sizeof(www_username) - 1);
    strncpy(www_password, password, sizeof(www_password) - 1);
    www_username[sizeof(www_username) - 1] = '\0';
    www_password[sizeof(www_password) - 1] = '\0';
    credentialsConfigured = true;
    
    Serial.println("Web credentials saved successfully");
    return true;
  }
  
  Serial.println("Failed to save web credentials");
  return false;
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
  html += "input[type=text],input[type=password]{width:500px;max-width:100%;padding:5px;}";
  html += "input[type=submit]{background:#4CAF50;color:white;padding:10px 20px;border:none;cursor:pointer;margin:5px;}";
  html += "input[type=submit]:hover{background:#45a049;}";
  html += ".status-good{color:green;font-weight:bold;}";
  html += ".status-bad{color:red;font-weight:bold;}";
  html += ".warning{background:#ffeb3b;padding:10px;margin:10px 0;border-left:4px solid #ff9800;}";
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
  html += "setInterval(updateStatus,1000);";
  html += "window.onload=updateStatus;";
  html += "</script>";
  
  html += "</head><body>";
  html += "<h1>Satellite Tracker Control</h1>";
  
  if (!credentialsConfigured) {
    html += "<div class='warning'><strong>⚠️ Security Warning:</strong> You are using default credentials. ";
    html += "Please change your password immediately using the form below!</div>";
  }
  
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
  
  // FIXED: Add password change form
  html += "<h2>Change Web Password</h2>";
  html += "<form action='/changepass' method='POST'>";
  html += "Current Password: <input type='password' name='oldpass' required><br><br>";
  html += "New Username: <input type='text' name='newuser' value='" + String(www_username) + "' maxlength='31'><br><br>";
  html += "New Password: <input type='password' name='newpass' required minlength='8' maxlength='63'><br><br>";
  html += "Confirm Password: <input type='password' name='confirm' required minlength='8' maxlength='63'><br><br>";
  html += "<input type='submit' value='Change Credentials'></form>";
  
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
  
  if (line1.length() >= sizeof(tleLine1) || line2.length() >= sizeof(tleLine2)) {
    server.send(400, "text/plain", "TLE data too long");
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
  
  // Memory barrier
  __dmb();
  
  tleUpdatePending = true;
  
  server.send(200, "text/plain", "TLE updated - Core 1 will initialize tracking");
  
  Serial.print("TLE updated via web: ");
  Serial.println(satelliteName);
}

void handleChangePassword() {
  // Require authentication with current password
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }
  
  if (!server.hasArg("oldpass") || !server.hasArg("newuser") || 
      !server.hasArg("newpass") || !server.hasArg("confirm")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }
  
  String oldPass = server.arg("oldpass");
  String newUser = server.arg("newuser");
  String newPass = server.arg("newpass");
  String confirm = server.arg("confirm");
  
  // Verify current password
  if (oldPass != String(www_password)) {
    server.send(403, "text/plain", "Current password incorrect");
    return;
  }
  
  // Validate new username
  if (newUser.length() == 0 || newUser.length() >= 32) {
    server.send(400, "text/plain", "Invalid username (1-31 chars)");
    return;
  }
  
  // Validate new password
  if (newPass.length() < 8 || newPass.length() >= 64) {
    server.send(400, "text/plain", "Invalid password (8-63 chars)");
    return;
  }
  
  // Check password confirmation
  if (newPass != confirm) {
    server.send(400, "text/plain", "Passwords do not match");
    return;
  }
  
  // Check password strength (basic check)
  bool hasUpper = false, hasLower = false, hasDigit = false;
  for (char c : newPass) {
    if (isupper(c)) hasUpper = true;
    if (islower(c)) hasLower = true;
    if (isdigit(c)) hasDigit = true;
  }
  
  if (!hasUpper || !hasLower || !hasDigit) {
    server.send(400, "text/plain", 
                "Password must contain uppercase, lowercase, and digit");
    return;
  }
  
  // Save new credentials
  if (saveWebCredentials(newUser.c_str(), newPass.c_str())) {
    server.send(200, "text/plain", "Credentials changed successfully. Please log in again.");
    Serial.println("Web credentials changed successfully");
  } else {
    server.send(500, "text/plain", "Failed to save credentials");
    Serial.println("Failed to save new web credentials");
  }
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
  
  Serial.println("Home command via web");
}

void handleStop() {
  // Require authentication
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }
  
  trackerState.tracking = false;
  stopAllMotors();
  server.send(200, "text/plain", "Tracking stopped");
  
  Serial.println("Stop command via web");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

void initWebInterface() {
  Serial.println("Initializing web interface...");
  
  loadWebCredentials();
  
  // Only connect if WiFi is configured
  if (!wifiConfigured || strlen(wifiSSID) == 0) {
    Serial.println("WiFi not configured - skipping");
    Serial.println("Use: SETWIFI <ssid> <password>");
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
      Serial.println("mDNS started: http://sattracker.local");
      MDNS.addService("http", "tcp", 80);
    }

    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    if (credentialsConfigured) {
      Serial.println("Login with your configured credentials");
    } else {
      Serial.println("⚠️ WARNING: Using default credentials!");
      Serial.print("Login: ");
      Serial.print(www_username);
      Serial.print(" / ");
      Serial.println(www_password);
      Serial.println("CHANGE PASSWORD IMMEDIATELY!");
    }
  } else {
    Serial.println("\nWiFi connection failed");
    wifiConfigured = false;
    return;
  }
  
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/tle", HTTP_POST, handleTLE);
  server.on("/home", HTTP_POST, handleHome);
  server.on("/stop", HTTP_POST, handleStop);
  server.on("/changepass", HTTP_POST, handleChangePassword);
  server.onNotFound(handleNotFound);
  server.begin();
  
  Serial.println("Web server started");
}

void handleWebClient() {
  // Only handle web requests if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    MDNS.update();
    server.handleClient();
  }
}
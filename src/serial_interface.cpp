// ============================================================================
// serial_interface.cpp - Serial command interface implementation
// ============================================================================

#include "serial_interface.h"

// External references to shared data
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
void setLEDMode(LEDMode mode);
extern LEDMode getLEDMode();

// Command buffer
static char cmdBuffer[SERIAL_BUFFER_SIZE];
static uint8_t cmdBufferPos = 0;

// Mode flags
static bool streamingGPS = false;
static unsigned long streamStartTime = 0;
static unsigned long streamDuration = 0;

// ============================================================================
// INTERNAL FUNCTIONS
// ============================================================================

// Parse command line into command and arguments
static bool parseCommand(const char* input, SerialCommand* cmd) {
  memset(cmd, 0, sizeof(SerialCommand));
  
  // Find first space or end of string
  const char* space = strchr(input, ' ');
  
  if (space) {
    // Command with arguments
    size_t cmdLen = space - input;
    if (cmdLen >= sizeof(cmd->command)) cmdLen = sizeof(cmd->command) - 1;
    
    strncpy(cmd->command, input, cmdLen);
    cmd->command[cmdLen] = '\0';
    
    // Skip leading spaces in arguments
    space++;
    while (*space == ' ') space++;
    
    strncpy(cmd->args, space, sizeof(cmd->args) - 1);
    cmd->args[sizeof(cmd->args) - 1] = '\0';
  } else {
    // Command without arguments
    strncpy(cmd->command, input, sizeof(cmd->command) - 1);
    cmd->command[sizeof(cmd->command) - 1] = '\0';
  }
  
  return true;
}

// Convert string to uppercase
static void toUpperCase(char* str) {
  while (*str) {
    *str = toupper(*str);
    str++;
  }
}

// Check if strings match (case insensitive)
static bool commandMatches(const char* cmd, const char* match) {
  char temp[32];
  strncpy(temp, cmd, sizeof(temp) - 1);
  temp[sizeof(temp) - 1] = '\0';
  toUpperCase(temp);
  return strcmp(temp, match) == 0;
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

static void handleHelpCommand() {
  Serial.println(F("\n=== AVAILABLE COMMANDS ==="));
  Serial.println();
  
  Serial.println(F("System Status:"));
  Serial.println(F("  STATUS       - Full system status"));
  Serial.println(F("  GPS          - GPS status and data"));
  Serial.println(F("  COMPASS      - Compass status and heading"));
  Serial.println(F("  JOYSTICK     - Joystick status and values"));
  Serial.println(F("  MOTORS       - Motor positions and status"));
  Serial.println(F("  WIFI         - WiFi status"));
  Serial.println(F("  STORAGE      - Storage info"));
  Serial.println();
  
  Serial.println(F("WiFi Configuration:"));
  Serial.println(F("  SETWIFI <ssid> <password>  - Set WiFi credentials"));
  Serial.println(F("  Example: SETWIFI MyNetwork MyPassword123"));
  Serial.println();
  
  Serial.println(F("Calibration:"));
  Serial.println(F("  CALCMP       - Start compass calibration"));
  Serial.println(F("  CALSTOP      - Stop compass calibration"));
  Serial.println(F("  CALJOY       - Start joystick calibration"));
  Serial.println(F("  CALJOYSTOP   - Stop joystick calibration"));
  Serial.println();
  
  Serial.println(F("Configuration:"));
  Serial.println(F("  SAVE         - Save config to storage"));
  Serial.println(F("  LOAD         - Load config from storage"));
  Serial.println(F("  ERASE        - Erase stored config"));
  Serial.println();
  
  Serial.println(F("Control:"));
  Serial.println(F("  HOME         - Home all axes"));
  Serial.println(F("  STOP         - Stop tracking"));
  Serial.println(F("  ESTOP        - Emergency stop"));
  Serial.println(F("  RESET        - Reset emergency stop"));
  Serial.println(F("  GOTO <az> <el>  - Move to position (deg)"));
  Serial.println(F("  Example: GOTO 180 45"));
  Serial.println();
  
  Serial.println(F("TLE Management:"));
  Serial.println(F("  SHOWTLE      - Display current TLE"));
  Serial.println(F("  SETTLE <name>  - Enter TLE (next 2 lines)"));
  Serial.println(F("  Example: SETTLE ISS"));
  Serial.println(F("           1 25544U 98067A   ...(line 1)"));
  Serial.println(F("           2 25544  51.6416 ...(line 2)"));
  Serial.println();
  
  Serial.println(F("Diagnostics:"));
  Serial.println(F("  RAWCMP <n>   - Print n compass readings"));
  Serial.println(F("  RAWJOY <n>   - Print n joystick readings"));
  Serial.println(F("  ENCODER      - Print encoder counts"));
  Serial.println(F("  STREAM <sec> - Stream GPS data for n seconds"));
  Serial.println();
  
  Serial.println(F("Other:"));
  Serial.println(F("  HELP         - This help message"));
  Serial.println(F("  BANNER       - System banner"));
  Serial.println();
}

static void handleStatusCommand() {
  printSystemStatus();
}

static void handleGPSCommand() {
  printGPSStatus();
}

static void handleCompassCommand() {
  printCompassStatus();
}

static void handleJoystickCommand() {
  printJoystickStatus();
}

static void handleMotorsCommand() {
  printMotorStatus();
}

static void handleWiFiCommand() {
  printWiFiStatus();
}

static void handleStorageCommand() {
  printStorageStatus();
}

static void handleSetWiFiCommand(const char* args) {
  // Parse SSID and password from arguments
  char ssid[32] = "";
  char password[64] = "";
  
  // Find first space (separates SSID from password)
  const char* space = strchr(args, ' ');
  
  if (!space) {
    Serial.println(F("ERROR: Usage: SETWIFI <ssid> <password>"));
    return;
  }
  
  size_t ssidLen = space - args;
  if (ssidLen >= sizeof(ssid)) ssidLen = sizeof(ssid) - 1;
  
  strncpy(ssid, args, ssidLen);
  ssid[ssidLen] = '\0';
  
  // Skip spaces
  space++;
  while (*space == ' ') space++;
  
  strncpy(password, space, sizeof(password) - 1);
  password[sizeof(password) - 1] = '\0';
  
  if (strlen(ssid) == 0 || strlen(password) == 0) {
    Serial.println(F("ERROR: SSID and password cannot be empty"));
    return;
  }
  
  setWiFiCredentials(ssid, password);
  Serial.println(F("WiFi credentials updated"));
  Serial.println(F("Use SAVE to persist, or restart to apply"));
}

static void handleSaveCommand() {
  saveConfiguration();
}

static void handleLoadCommand() {
  loadConfiguration();
}

static void handleEraseCommand() {
  Serial.println(F("WARNING: This will erase all stored configuration!"));
  Serial.println(F("Type 'YES' to confirm:"));
  
  unsigned long timeout = millis() + 10000; // 10 second timeout
  String response = "";
  
  while (millis() < timeout) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        break;
      }
      response += c;
    }
  }
  
  response.trim();
  if (response.equalsIgnoreCase("YES")) {
    eraseConfiguration();
    Serial.println(F("Configuration erased"));
  } else {
    Serial.println(F("Cancelled"));
  }
}

static void handleCalCmpCommand() {
  beginCompassCalibration();
}

static void handleCalStopCommand() {
  endCompassCalibration();
}

static void handleCalJoyCommand() {
  beginJoystickCalibration();
}

static void handleCalJoyStopCommand() {
  stopJoystickCalibration();
}

static void handleHomeCommand() {
  Serial.println(F("Homing axes..."));
  beginHomeAxes();
}

static void handleStopCommand() {
  Serial.println(F("Stopping tracking..."));
  endTracking();
}

static void handleEStopCommand() {
  Serial.println(F("EMERGENCY STOP ACTIVATED"));
  beginEmergencyStop();
}

static void handleResetCommand() {
  Serial.println(F("Resetting emergency stop..."));
  beginResetEmergencyStop();
}

static void handleGotoCommand(const char* args) {
  float az, el;
  
  if (sscanf(args, "%f %f", &az, &el) != 2) {
    Serial.println(F("ERROR: Usage: GOTO <azimuth> <elevation>"));
    Serial.println(F("Example: GOTO 180 45"));
    return;
  }
  
  if (az < 0 || az >= 360) {
    Serial.println(F("ERROR: Azimuth must be 0-359.99"));
    return;
  }
  
  if (el < MIN_ELEVATION || el > MAX_ELEVATION) {
    Serial.printf("ERROR: Elevation must be %.1f-%.1f\n", MIN_ELEVATION, MAX_ELEVATION);
    return;
  }
  
  setManualPosition(az, el);
  Serial.printf("Moving to Az=%.2f El=%.2f\n", az, el);
}

static void handleShowTLECommand() {
  printTLE();
}

static void handleSetTLECommand(const char* args) {
  if (strlen(args) == 0) {
    Serial.println(F("ERROR: Usage: SETTLE <satellite name>"));
    Serial.println(F("Then enter TLE line 1 and line 2"));
    return;
  }
  
  char name[25];
  strncpy(name, args, sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';
  
  Serial.println(F("Enter TLE Line 1:"));
  
  // Wait for line 1
  unsigned long timeout = millis() + 30000;
  String line1 = "";
  while (millis() < timeout && line1.length() == 0) {
    if (Serial.available()) {
      line1 = Serial.readStringUntil('\n');
      line1.trim();
    }
  }
  
  if (line1.length() != 69) {
    Serial.println(F("ERROR: TLE line 1 must be exactly 69 characters"));
    return;
  }
  
  Serial.println(F("Enter TLE Line 2:"));
  
  // Wait for line 2
  timeout = millis() + 30000;
  String line2 = "";
  while (millis() < timeout && line2.length() == 0) {
    if (Serial.available()) {
      line2 = Serial.readStringUntil('\n');
      line2.trim();
    }
  }
  
  if (line2.length() != 69) {
    Serial.println(F("ERROR: TLE line 2 must be exactly 69 characters"));
    return;
  }
  
  if (line1[0] != '1' || line2[0] != '2') {
    Serial.println(F("ERROR: Invalid TLE format"));
    return;
  }
  
  setTLE(name, line1.c_str(), line2.c_str());
  Serial.println(F("TLE updated"));
}

static void handleRawCmpCommand(const char* args) {
  int samples = 10;
  if (strlen(args) > 0) {
    samples = atoi(args);
  }
  samples = constrain(samples, 1, 1000);
  
  printRawCompassData(samples);
}

static void handleRawJoyCommand(const char* args) {
  int samples = 10;
  if (strlen(args) > 0) {
    samples = atoi(args);
  }
  samples = constrain(samples, 1, 1000);
  
  printRawJoystickData(samples);
}

static void handleEncoderCommand() {
  printEncoderCounts();
}

static void handleStreamCommand(const char* args) {
  unsigned long duration = 10; // Default 10 seconds
  if (strlen(args) > 0) {
    duration = atoi(args);
  }
  duration = constrain(duration, 1, 300); // Max 5 minutes
  
  streamGPSData(duration);
}

// Process a complete command
static void processCommand(const char* input) {
  SerialCommand cmd;
  if (!parseCommand(input, &cmd)) {
    return;
  }
  
  // Convert command to uppercase for comparison
  toUpperCase(cmd.command);
  
  // Dispatch to appropriate handler
  if (commandMatches(cmd.command, "HELP") || commandMatches(cmd.command, "?")) {
    handleHelpCommand();
  }
  else if (commandMatches(cmd.command, "BANNER")) {
    printBanner();
  }
  else if (commandMatches(cmd.command, "STATUS")) {
    handleStatusCommand();
  }
  else if (commandMatches(cmd.command, "GPS")) {
    handleGPSCommand();
  }
  else if (commandMatches(cmd.command, "COMPASS")) {
    handleCompassCommand();
  }
  else if (commandMatches(cmd.command, "JOYSTICK")) {
    handleJoystickCommand();
  }
  else if (commandMatches(cmd.command, "MOTORS")) {
    handleMotorsCommand();
  }
  else if (commandMatches(cmd.command, "WIFI")) {
    handleWiFiCommand();
  }
  else if (commandMatches(cmd.command, "STORAGE")) {
    handleStorageCommand();
  }
  else if (commandMatches(cmd.command, "SETWIFI")) {
    handleSetWiFiCommand(cmd.args);
  }
  else if (commandMatches(cmd.command, "SAVE")) {
    handleSaveCommand();
  }
  else if (commandMatches(cmd.command, "LOAD")) {
    handleLoadCommand();
  }
  else if (commandMatches(cmd.command, "ERASE")) {
    handleEraseCommand();
  }
  else if (commandMatches(cmd.command, "CALCMP")) {
    handleCalCmpCommand();
  }
  else if (commandMatches(cmd.command, "CALSTOP")) {
    handleCalStopCommand();
  }
  else if (commandMatches(cmd.command, "CALJOY")) {
    handleCalJoyCommand();
  }
  else if (commandMatches(cmd.command, "CALJOYSTOP")) {
    handleCalJoyStopCommand();
  }
  else if (commandMatches(cmd.command, "HOME")) {
    handleHomeCommand();
  }
  else if (commandMatches(cmd.command, "STOP")) {
    handleStopCommand();
  }
  else if (commandMatches(cmd.command, "ESTOP")) {
    handleEStopCommand();
  }
  else if (commandMatches(cmd.command, "RESET")) {
    handleResetCommand();
  }
  else if (commandMatches(cmd.command, "GOTO")) {
    handleGotoCommand(cmd.args);
  }
  else if (commandMatches(cmd.command, "SHOWTLE")) {
    handleShowTLECommand();
  }
  else if (commandMatches(cmd.command, "SETTLE")) {
    handleSetTLECommand(cmd.args);
  }
  else if (commandMatches(cmd.command, "RAWCMP")) {
    handleRawCmpCommand(cmd.args);
  }
  else if (commandMatches(cmd.command, "RAWJOY")) {
    handleRawJoyCommand(cmd.args);
  }
  else if (commandMatches(cmd.command, "ENCODER")) {
    handleEncoderCommand();
  }
  else if (commandMatches(cmd.command, "STREAM")) {
    handleStreamCommand(cmd.args);
  }
  else if (commandMatches(cmd.command, "LEDTEST")) {
    handleLedTest();
  }
  else if (commandMatches(cmd.command, "LEDMODE")) {
    if (strlen(cmd.args) > 0) {
      int mode = atoi(cmd.args);
      handleLedMode(mode);
    }
    
  }
  else if (commandMatches(cmd.command, "LEDINFO")) {
    handleLedPrintInfo();
  }
  else {
    Serial.print(F("Unknown command: "));
    Serial.println(cmd.command);
    Serial.println(F("Type HELP for available commands"));
  }
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

void initSerialInterface() {
  Serial.println(F("\n=== Serial Interface Initialized ==="));
  Serial.println(F("Type HELP for available commands"));
  Serial.println();
  
  cmdBufferPos = 0;
  memset(cmdBuffer, 0, sizeof(cmdBuffer));
}

void updateSerialInterface() {
  // Handle GPS streaming mode
  if (streamingGPS) {
    if (millis() - streamStartTime >= streamDuration) {
      streamingGPS = false;
      Serial.println(F("\n=== GPS Stream Complete ==="));
    }
    return;
  }
  
  // Check for incoming data
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    // Handle newline/carriage return
    if (c == '\n' || c == '\r') {
      // Ignore empty lines
      if (cmdBufferPos == 0) {
        continue;
      }
      
      // Echo newline
      Serial.println();
      
      // Null terminate
      cmdBuffer[cmdBufferPos] = '\0';
      
      // Process command
      processCommand(cmdBuffer);
      
      // Reset buffer
      cmdBufferPos = 0;
      memset(cmdBuffer, 0, sizeof(cmdBuffer));
      
      // Print prompt
      Serial.print(F("> "));
    }
    else if (c == '\b' || c == 127) {
      // Backspace
      if (cmdBufferPos > 0) {
        cmdBufferPos--;
        cmdBuffer[cmdBufferPos] = '\0';
        // Echo backspace
        Serial.print("\b \b");
      }
    }
    else if (c >= 32 && c <= 126) {
      // Printable character
      if (cmdBufferPos < SERIAL_BUFFER_SIZE - 1) {
        cmdBuffer[cmdBufferPos++] = c;
        // Echo character
        Serial.print(c);
      }
    }
  }
}

void printBanner() {
  Serial.println(F("\n"));
   Serial.println(F("\n\n"));
  Serial.println(F("╔═════════════════════════════════════╗"));
  Serial.println(F("║   RP2350 Satellite Tracker System   ║"));
  Serial.printf (  "║    build: %s - %s    ║\r\n", __DATE__, __TIME__); // Mmm dd yyyy - hh:mm:ss (22 characters)
  Serial.println(F("╚═════════════════════════════════════╝"));
  Serial.println();
}

void printHelp() {
  handleHelpCommand();
}

void printSystemStatus() {
  Serial.println(F("\n=== SYSTEM STATUS ==="));
  Serial.println();
  
  // GPS Status
  Serial.print(F("GPS:          "));
  Serial.println(trackerState.gpsValid ? F("VALID") : F("NO FIX"));
  
  if (trackerState.gpsValid) {
    Serial.printf("  Location:   %.6f, %.6f\n", trackerState.latitude, trackerState.longitude);
    Serial.printf("  Altitude:   %.1f m\n", trackerState.altitude);
    Serial.printf("  Time (UTC): %04d-%02d-%02d %02d:%02d:%02d\n",
                  trackerState.gpsYear, trackerState.gpsMonth, trackerState.gpsDay,
                  trackerState.gpsHour, trackerState.gpsMinute, trackerState.gpsSecond);
  }
  
  // Tracking Status
  Serial.println();
  Serial.print(F("TLE Loaded:   "));
  Serial.println(trackerState.tleValid ? F("YES") : F("NO"));
  
  if (trackerState.tleValid) {
    Serial.print(F("  Satellite:  "));
    Serial.println(satelliteName);
  }
  
  Serial.print(F("Tracking:     "));
  Serial.println(trackerState.tracking ? F("ACTIVE") : F("IDLE"));
  
  // Motor Positions
  Serial.println();
  float currentEl = motorPos.elevation * DEGREES_PER_PULSE;
  float currentAz = motorPos.azimuth * DEGREES_PER_PULSE;
  while (currentAz < 0) currentAz += 360.0;
  while (currentAz >= 360) currentAz -= 360.0;
  
  Serial.printf("Current Pos:  Az=%.2f° El=%.2f°\n", currentAz, currentEl);
  Serial.printf("Target Pos:   Az=%.2f° El=%.2f°\n", targetPos.azimuth, targetPos.elevation);
  
  // Emergency Stop
  Serial.println();
  Serial.print(F("E-Stop:       "));
  Serial.println(isEmergencyStop() ? F("ACTIVE") : F("OK"));
  
  // WiFi Status
  Serial.println();
  Serial.print(F("WiFi:         "));
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("CONNECTED ("));
    Serial.print(WiFi.localIP());
    Serial.println(F(")"));
  } else {
    Serial.println(wifiConfigured ? F("CONFIGURED (not connected)") : F("NOT CONFIGURED"));
  }
  
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

void printCompassStatus() {
  Serial.println(F("\n=== COMPASS STATUS ==="));
  Serial.println();
  
  QMC5883LCompass& compass = getCompass();
  compass.read();
  
  Serial.print(F("Calibrating:   "));
  Serial.println(isBackgroundCalibrationActive() ? F("YES") : F("NO"));
  
  if (isBackgroundCalibrationActive()) {
    Serial.printf("Duration:      %lu seconds\n", getCalibrationDuration());
  }
  
  Serial.println();
  Serial.print(F("Raw Values:"));
  Serial.printf("\n  X: %d\n", compass.getX());
  Serial.printf("  Y: %d\n", compass.getY());
  Serial.printf("  Z: %d\n", compass.getZ());
  
  Serial.println();
  float heading = readCompassHeading();
  Serial.printf("Heading:       %.2f°\n", heading);
  
  // Cardinal direction
  const char* direction;
  if (heading < 22.5 || heading >= 337.5) direction = "N";
  else if (heading < 67.5) direction = "NE";
  else if (heading < 112.5) direction = "E";
  else if (heading < 157.5) direction = "SE";
  else if (heading < 202.5) direction = "S";
  else if (heading < 247.5) direction = "SW";
  else if (heading < 292.5) direction = "W";
  else direction = "NW";
  
  Serial.print(F("Direction:     "));
  Serial.println(direction);
  
  Serial.println();
}

void printJoystickStatus() {
  Serial.println(F("\n=== JOYSTICK STATUS ==="));
  Serial.println();
  
  JoystickData joy = getJoystickState();
  JoystickCalibration cal = getJoystickCalibration();
  
  Serial.print(F("Manual Mode:   "));
  Serial.println(isJoystickManualMode() ? F("ACTIVE") : F("INACTIVE"));
  
  Serial.print(F("Calibrating:   "));
  Serial.println(isJoystickCalibrating() ? F("YES") : F("NO"));
  
  Serial.print(F("Centered:      "));
  Serial.println(joy.inDeadband ? F("YES") : F("NO"));
  
  Serial.println();
  Serial.printf("Raw Values:\n");
  Serial.printf("  X: %d (norm: %.3f)\n", joy.x, joy.xNormalized);
  Serial.printf("  Y: %d (norm: %.3f)\n", joy.y, joy.yNormalized);
  //Serial.printf("  Button: %s\n", joy.buttonPressed ? "PRESSED" : "RELEASED");
  
  Serial.println();
  Serial.printf("Calibration:\n");
  Serial.printf("  X: Min=%d Center=%d Max=%d\n", cal.xMin, cal.xCenter, cal.xMax);
  Serial.printf("  Y: Min=%d Center=%d Max=%d\n", cal.yMin, cal.yCenter, cal.yMax);
  Serial.printf("  Deadband: %d%%\n", cal.deadband);
  
  Serial.println();
  Serial.printf("Speed Commands:\n");
  Serial.printf("  Azimuth:   %.3f\n", getJoystickAzimuthSpeed());
  Serial.printf("  Elevation: %.3f\n", getJoystickElevationSpeed());
  
  Serial.println();
}

void printMotorStatus() {
  Serial.println(F("\n=== MOTOR STATUS ==="));
  Serial.println();
  
  float currentEl = motorPos.elevation * DEGREES_PER_PULSE;
  float currentAz = motorPos.azimuth * DEGREES_PER_PULSE;
  while (currentAz < 0) currentAz += 360.0;
  while (currentAz >= 360) currentAz -= 360.0;
  
  Serial.printf("Current Position:\n");
  Serial.printf("  Azimuth:   %.2f° (encoder: %ld)\n", currentAz, motorPos.azimuth);
  Serial.printf("  Elevation: %.2f° (encoder: %ld)\n", currentEl, motorPos.elevation);
  
  Serial.println();
  Serial.printf("Target Position:\n");
  Serial.printf("  Azimuth:   %.2f°\n", targetPos.azimuth);
  Serial.printf("  Elevation: %.2f°\n", targetPos.elevation);
  Serial.printf("  Valid:     %s\n", targetPos.valid ? "YES" : "NO");
  
  Serial.println();
  Serial.printf("Position Error:\n");
  float errorAz = targetPos.azimuth - currentAz;
  if (errorAz > 180) errorAz -= 360;
  if (errorAz < -180) errorAz += 360;
  float errorEl = targetPos.elevation - currentEl;
  Serial.printf("  Azimuth:   %.2f°\n", errorAz);
  Serial.printf("  Elevation: %.2f°\n", errorEl);
  
  Serial.println();
  Serial.printf("Index Found:\n");
  Serial.printf("  Azimuth:   %s\n", motorPos.azimuthIndexFound ? "YES" : "NO");
  Serial.printf("  Elevation: %s\n", motorPos.elevationIndexFound ? "YES" : "NO");
  
  Serial.println();
  Serial.printf("Emergency Stop: %s\n", isEmergencyStop() ? "ACTIVE" : "OK");
  
  Serial.println();
}

void printWiFiStatus() {
  Serial.println(F("\n=== WIFI STATUS ==="));
  Serial.println();
  
  Serial.print(F("Configured:    "));
  Serial.println(wifiConfigured ? F("YES") : F("NO"));
  
  if (wifiConfigured) {
    Serial.print(F("SSID:          "));
    Serial.println(wifiSSID);
    Serial.print(F("Password:      "));
    for (int i = 0; i < strlen(wifiPassword); i++) {
      Serial.print('*');
    }
    Serial.println();
  }
  
  Serial.println();
  Serial.print(F("Connection:    "));
  
  switch (WiFi.status()) {
    case WL_CONNECTED:
      Serial.println(F("CONNECTED"));
      Serial.print(F("IP Address:    "));
      Serial.println(WiFi.localIP());
      Serial.print(F("Signal (RSSI): "));
      Serial.print(WiFi.RSSI());
      Serial.println(F(" dBm"));
      Serial.print(F("Web Access:    http://"));
      Serial.println(WiFi.localIP());
      Serial.println(F("               http://sattracker.local"));
      break;
    case WL_NO_SHIELD:
      Serial.println(F("NO WIFI HARDWARE"));
      break;
    case WL_IDLE_STATUS:
      Serial.println(F("IDLE"));
      break;
    case WL_NO_SSID_AVAIL:
      Serial.println(F("SSID NOT FOUND"));
      break;
    case WL_SCAN_COMPLETED:
      Serial.println(F("SCAN COMPLETE"));
      break;
    case WL_CONNECT_FAILED:
      Serial.println(F("CONNECTION FAILED"));
      break;
    case WL_CONNECTION_LOST:
      Serial.println(F("CONNECTION LOST"));
      break;
    case WL_DISCONNECTED:
      Serial.println(F("DISCONNECTED"));
      break;
    default:
      Serial.println(F("UNKNOWN"));
      break;
  }
  
  Serial.println();
}

void printStorageStatus() {
  Serial.println(F("\n=== STORAGE STATUS ==="));
  Serial.println();
  
  if (!isStorageAvailable()) {
    Serial.println(F("No storage available"));
    Serial.println(F("Configuration will not persist across reboots"));
    Serial.println();
    return;
  }
  
  StorageType type = getStorageType();
  Serial.print(F("Type:          "));
  
  switch (type) {
    case STORAGE_TYPE_W25Q_FLASH:
      Serial.println(F("W25Q SPI Flash"));
      break;
    case STORAGE_TYPE_SD_CARD:
      Serial.println(F("SD Card"));
      break;
    default:
      Serial.println(F("Unknown"));
      break;
  }
  
  printStorageInfo();
}

void setWiFiCredentials(const char* ssid, const char* password) {
  memset(wifiSSID, 0, sizeof(wifiSSID));
  strncpy(wifiSSID, ssid, sizeof(wifiSSID) - 1);
  wifiSSID[sizeof(wifiSSID) - 1] = '\0';
  
  memset(wifiPassword, 0, sizeof(wifiPassword));
  strncpy(wifiPassword, password, sizeof(wifiPassword) - 1);
  wifiPassword[sizeof(wifiPassword) - 1] = '\0';
  
  wifiConfigured = true;
}

void saveConfiguration() {
  Serial.println(F("Saving configuration..."));
  
  if (!isStorageAvailable()) {
    Serial.println(F("ERROR: No storage available"));
    return;
  }
  
  StorageConfig config = {0};
  
  // WiFi credentials
  strncpy(config.wifiSSID, wifiSSID, sizeof(config.wifiSSID) - 1);
  strncpy(config.wifiPassword, wifiPassword, sizeof(config.wifiPassword) - 1);
  config.wifiConfigured = wifiConfigured;
  
  // Compass calibration (if available)
  QMC5883LCompass& compass = getCompass();
  // Note: QMC5883L library doesn't expose calibration values directly
  // This would need to be tracked separately or extracted from the library
  config.compassCalibrated = false; // Placeholder
  
  // Joystick calibration
  JoystickCalibration joyCal = getJoystickCalibration();
  config.joyXMin = joyCal.xMin;
  config.joyXCenter = joyCal.xCenter;
  config.joyXMax = joyCal.xMax;
  config.joyYMin = joyCal.yMin;
  config.joyYCenter = joyCal.yCenter;
  config.joyYMax = joyCal.yMax;
  config.joyDeadband = joyCal.deadband;
  config.joyCalibrated = true;
  
  // TLE data
  strncpy(config.satelliteName, satelliteName, sizeof(config.satelliteName) - 1);
  strncpy(config.tleLine1, tleLine1, sizeof(config.tleLine1) - 1);
  strncpy(config.tleLine2, tleLine2, sizeof(config.tleLine2) - 1);
  config.tleValid = trackerState.tleValid;
  
  if (saveConfig(&config)) {
    Serial.println(F("Configuration saved successfully"));
  } else {
    Serial.println(F("ERROR: Failed to save configuration"));
  }
}

void loadConfiguration() {
  Serial.println(F("Loading configuration..."));
  
  if (!isStorageAvailable()) {
    Serial.println(F("ERROR: No storage available"));
    return;
  }
  
  StorageConfig config = {0};
  
  if (!loadConfig(&config)) {
    Serial.println(F("No saved configuration found"));
    return;
  }
  
  // WiFi credentials
  strncpy(wifiSSID, config.wifiSSID, sizeof(wifiSSID) - 1);
  strncpy(wifiPassword, config.wifiPassword, sizeof(wifiPassword) - 1);
  wifiConfigured = config.wifiConfigured;
  
  // Joystick calibration
  if (config.joyCalibrated) {
    JoystickCalibration joyCal;
    joyCal.xMin = config.joyXMin;
    joyCal.xCenter = config.joyXCenter;
    joyCal.xMax = config.joyXMax;
    joyCal.yMin = config.joyYMin;
    joyCal.yCenter = config.joyYCenter;
    joyCal.yMax = config.joyYMax;
    joyCal.deadband = config.joyDeadband;
    setJoystickCalibration(joyCal);
    Serial.println(F("Joystick calibration loaded"));
  }
  
  // Compass calibration
  if (config.compassCalibrated) {
    setCompassCalibration(config.compassMinX, config.compassMaxX,
                         config.compassMinY, config.compassMaxY,
                         config.compassMinZ, config.compassMaxZ);
    Serial.println(F("Compass calibration loaded"));
  }
  
  // TLE data
  if (config.tleValid) {
    strncpy(satelliteName, config.satelliteName, sizeof(satelliteName) - 1);
    strncpy(tleLine1, config.tleLine1, sizeof(tleLine1) - 1);
    strncpy(tleLine2, config.tleLine2, sizeof(tleLine2) - 1);
    trackerState.tleValid = true;
    tleUpdatePending = true;
    Serial.println(F("TLE data loaded"));
  }
  
  Serial.println(F("Configuration loaded successfully"));
}

void eraseConfiguration() {
  if (!isStorageAvailable()) {
    Serial.println(F("ERROR: No storage available"));
    return;
  }
  
  if (eraseConfig()) {
    Serial.println(F("Configuration erased"));
  } else {
    Serial.println(F("ERROR: Failed to erase configuration"));
  }
}

void beginCompassCalibration() {
  Serial.println(F("\n=== COMPASS CALIBRATION ==="));
  Serial.println(F("Starting calibration..."));
  Serial.println(F("Rotate device through ALL orientations"));
  Serial.println(F("Recommended: 30+ seconds"));
  Serial.println(F("Type CALSTOP when done"));
  Serial.println();
  
  startBackgroundCalibration();
}

void endCompassCalibration() {
  if (!isBackgroundCalibrationActive()) {
    Serial.println(F("No calibration in progress"));
    return;
  }
  
  stopBackgroundCalibration();
  
  // Save to storage
  if (isStorageAvailable()) {
    Serial.println(F("Save calibration? (Y/N):"));
    unsigned long timeout = millis() + 10000;
    String response = "";
    
    while (millis() < timeout) {
      if (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') break;
        response += c;
      }
    }
    
    response.trim();
    if (response.equalsIgnoreCase("Y") || response.equalsIgnoreCase("YES")) {
      saveConfiguration();
    }
  }
}

void beginJoystickCalibration() {
  Serial.println(F("\n=== JOYSTICK CALIBRATION ==="));
  Serial.println(F("Starting calibration..."));
  Serial.println(F("1. Move joystick through full range (circles)"));
  Serial.println(F("2. Return to center and hold"));
  Serial.println(F("3. Type CALJOYSTOP when done"));
  Serial.println();
  
  beginJoystickCalibration();
}

void endJoystickCalibration() {
  if (!isJoystickCalibrating()) {
    Serial.println(F("No calibration in progress"));
    return;
  }
  
  stopJoystickCalibration();
  
  // Save to storage
  if (isStorageAvailable()) {
    Serial.println(F("Save calibration? (Y/N):"));
    unsigned long timeout = millis() + 10000;
    String response = "";
    
    while (millis() < timeout) {
      if (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') break;
        response += c;
      }
    }
    
    response.trim();
    if (response.equalsIgnoreCase("Y") || response.equalsIgnoreCase("YES")) {
      saveConfiguration();
    }
  }
}

void beginHomeAxes() {
  trackerState.tracking = false;
  ::homeAxes(); // Call the motor control function
}

void endTracking() {
  trackerState.tracking = false;
  stopAllMotors();
}

void setManualPosition(float az, float el) {
  trackerState.tracking = false;
  targetPos.azimuth = az;
  targetPos.elevation = el;
  targetPos.valid = true;
}

void beginEmergencyStop() {
  // Emergency stop is handled by motor_control module
  // This is just a wrapper for serial interface
  stopAllMotors();
}

void beginResetEmergencyStop() {
  ::beginResetEmergencyStop(); // Call motor control function
}

void setTLE(const char* name, const char* line1, const char* line2) {
  memset(satelliteName, 0, sizeof(satelliteName));
  strncpy(satelliteName, name, sizeof(satelliteName) - 1);
  satelliteName[sizeof(satelliteName) - 1] = '\0';
  
  memset(tleLine1, 0, sizeof(tleLine1));
  strncpy(tleLine1, line1, sizeof(tleLine1) - 1);
  tleLine1[sizeof(tleLine1) - 1] = '\0';
  
  memset(tleLine2, 0, sizeof(tleLine2));
  strncpy(tleLine2, line2, sizeof(tleLine2) - 1);
  tleLine2[sizeof(tleLine2) - 1] = '\0';
  
  __dmb(); // Memory barrier
  tleUpdatePending = true;
  trackerState.tleValid = true;
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

void printRawCompassData(int samples) {
  Serial.println(F("\n=== RAW COMPASS DATA ==="));
  Serial.printf("Collecting %d samples...\n", samples);
  Serial.println();
  Serial.println(F("Sample    X       Y       Z     Heading"));
  Serial.println(F("------  ------  ------  ------  -------"));
  
  QMC5883LCompass& compass = getCompass();
  
  for (int i = 0; i < samples; i++) {
    compass.read();
    float heading = readCompassHeading();
    
    Serial.printf("%4d    %6d  %6d  %6d  %7.2f\n",
                  i + 1,
                  compass.getX(),
                  compass.getY(),
                  compass.getZ(),
                  heading);
    
    delay(100);
  }
  
  Serial.println();
}

void printRawJoystickData(int samples) {
  Serial.println(F("\n=== RAW JOYSTICK DATA ==="));
  Serial.printf("Collecting %d samples...\n", samples);
  Serial.println();
  Serial.println(F("Sample    X     Y     X_norm  Y_norm  Button"));
  Serial.println(F("------  ----  ----   ------  ------  ------"));
  
  for (int i = 0; i < samples; i++) {
    JoystickData joy = readJoystick();
    
    Serial.printf("%4d    %4d  %4d   %6.3f  %6.3f   %s\n",
                  i + 1,
                  joy.x,
                  joy.y,
                  joy.xNormalized,
                  joy.yNormalized);
//                  joy.buttonPressed ? "PRESS" : "REL");
    
    delay(100);
  }
  
  Serial.println();
}

void printEncoderCounts() {
  Serial.println(F("\n=== ENCODER COUNTS ==="));
  Serial.println();
  
  Serial.printf("Azimuth Encoder:   %ld counts (%.2f°)\n",
                motorPos.azimuth,
                motorPos.azimuth * DEGREES_PER_PULSE);
  
  Serial.printf("Elevation Encoder: %ld counts (%.2f°)\n",
                motorPos.elevation,
                motorPos.elevation * DEGREES_PER_PULSE);
  
  Serial.println();
  Serial.printf("Degrees per count: %.6f°\n", DEGREES_PER_PULSE);
  Serial.printf("Gear ratio:        %.1f:1\n", GEAR_RATIO);
  Serial.printf("Encoder PPR:       %d\n", ENCODER_PPR);
  Serial.println();
}

void streamGPSData(unsigned long duration) {
  Serial.println(F("\n=== GPS DATA STREAM ==="));
  Serial.printf("Streaming for %lu seconds...\n", duration);
  Serial.println(F("Press any key to stop early"));
  Serial.println();
  
  streamingGPS = true;
  streamStartTime = millis();
  streamDuration = duration * 1000;
  
  // GPS data will be printed by GPS module's dumpGPSData()
  // The stream will continue until duration expires or key pressed
}

void handleLedTest() {
  testLEDs();
}

void handleLedMode(int mode) {
  if (mode > 0 && mode < LED_MODE_CUSTOM) {
    setLEDMode((LEDMode)mode);
    Serial.print("LED mode set to: ");
    Serial.println(mode);
  }
}

void handleLedPrintInfo() {
  Serial.print("Current mode: ");
  Serial.println((int)getLEDMode());
  Serial.print("Brightness: ");
  Serial.println(globalBrightness);
  Serial.print("Buffer[0]: 0x");
  Serial.println(ledBuffer[0], HEX);
}



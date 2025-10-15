// ============================================================================
// display_module.cpp
// ============================================================================

#include "display_module.h"
#include "motor_control.h"
#include "compass_module.h"

// External references to shared data
extern MotorPosition motorPos;
extern TargetPosition targetPos;
extern TrackerState trackerState;
extern char satelliteName[25];
extern DisplayScreen currentScreen;
extern bool displayNeedsUpdate;
extern char wifiSSID[32];
extern char wifiPassword[64];
extern bool wifiConfigured;

// TFT and Touch objects
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);
Adafruit_FT6206 touch = Adafruit_FT6206();

// Touch debouncing
#define TOUCH_DEBOUNCE_MS 250
static unsigned long lastTouchTime = 0;
int16_t lastTouchX = -1;
int16_t lastTouchY = -1;
bool wasTouched = false;
uint8_t lastTag = TAG_NONE;

// WiFi setup state
enum SetupField {
  FIELD_SSID,
  FIELD_PASSWORD
};
SetupField currentField = FIELD_SSID;
char tempSSID[32] = "";
char tempPassword[64] = "";

// Compass calibration state
bool compassCalibrating = false;
unsigned long calibrationStartTime = 0;

// Helper function to draw a button
void drawButton(const Button &btn) {
  tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 5, btn.color);
  tft.drawRoundRect(btn.x, btn.y, btn.w, btn.h, 5, WHITE);
  
  // Center text
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  int16_t x1, y1;
  uint16_t tw, th;
  tft.getTextBounds(btn.label, 0, 0, &x1, &y1, &tw, &th);
  tft.setCursor(btn.x + (btn.w - tw) / 2, btn.y + (btn.h - th) / 2);
  tft.print(btn.label);
}

// Helper to check if touch is within button bounds
uint8_t getTouchedTag(int16_t x, int16_t y, const Button* buttons, uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    if (x >= buttons[i].x && x <= (buttons[i].x + buttons[i].w) &&
        y >= buttons[i].y && y <= (buttons[i].y + buttons[i].h)) {
      return buttons[i].tag;
    }
  }
  return TAG_NONE;
}

void initDisplay() {
  Serial.println("Initializing display...");
  
  // Initialize TFT
  tft.begin();
  tft.setRotation(3);  // Landscape mode (320x240)
  tft.fillScreen(BLACK);
  
  Serial.println("ILI9341 TFT initialized");
  
  // Initialize Touch (FT6206)
  if (!touch.begin(40)) {
    Serial.println("FT6206 touch controller not found!");
  } else {
    Serial.println("FT6206 touch initialized");
  }
  
  // Start with setup screen if WiFi not configured
  if (!wifiConfigured) {
    currentScreen = SCREEN_SETUP;
  }
  
  // Splash screen
  tft.fillScreen(BLACK);
  tft.setTextColor(WHITE);
  tft.setTextSize(3);
  tft.setCursor(20, 100);
  tft.print("Sat Tracker");
  
  tft.setTextSize(2);
  tft.setCursor(40, 140);
  if (!wifiConfigured) {
    tft.print("WiFi Setup Required");
  } else {
    tft.print("Initializing...");
  }
  
  delay(2000);
  
  displayNeedsUpdate = true;
}

void drawSetupScreen() {
  tft.fillScreen(BLACK);
  
  // Header
  tft.fillRect(0, 0, SCREEN_WIDTH, 30, BLUE);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(80, 8);
  tft.print("WiFi Setup");
  
  // Instructions
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.print("Configure WiFi for web access");
  
  // SSID field
  tft.setTextSize(2);
  tft.setCursor(10, 60);
  tft.print("SSID:");
  
  tft.drawRect(10, 80, 300, 30, currentField == FIELD_SSID ? CYAN : GRAY);
  tft.setTextColor(strlen(tempSSID) > 0 ? WHITE : GRAY);
  tft.setTextSize(2);
  tft.setCursor(15, 88);
  if (strlen(tempSSID) > 0) {
    char displaySSID[20];
    strncpy(displaySSID, tempSSID, sizeof(displaySSID) - 1);
    displaySSID[sizeof(displaySSID) - 1] = '\0';
    tft.print(displaySSID);
  } else {
    tft.setTextSize(1);
    tft.print("<enter via Serial Monitor>");
  }
  
  // Password field
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 120);
  tft.print("Password:");
  
  tft.drawRect(10, 140, 300, 30, currentField == FIELD_PASSWORD ? CYAN : GRAY);
  tft.setTextColor(strlen(tempPassword) > 0 ? WHITE : GRAY);
  tft.setTextSize(2);
  tft.setCursor(15, 148);
  if (strlen(tempPassword) > 0) {
    size_t len = strlen(tempPassword);
    for (size_t i = 0; i < len && i < 20; i++) {
      tft.print('*');
    }
  } else {
    tft.setTextSize(1);
    tft.print("<enter via Serial Monitor>");
  }
  
  // Buttons
  Button buttons[2] = {
    {10, 185, 140, 40, TAG_SETUP_CONNECT, "Connect", strlen(tempSSID) > 0 ? GREEN : GRAY},
    {170, 185, 140, 40, TAG_SETUP_SKIP, "Skip WiFi", ORANGE}
  };
  
  for (int i = 0; i < 2; i++) {
    drawButton(buttons[i]);
  }
  
  // Help text
  tft.setTextSize(1);
  tft.setTextColor(GRAY);
  tft.setCursor(10, 230);
  tft.print("Enter credentials via Serial Monitor");
}

void drawMainScreen() {
  tft.fillScreen(BLACK);
  
  // Status bar
  tft.fillRect(0, 0, SCREEN_WIDTH, 25, BLUE);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 5);
  tft.print("SAT TRACKER");
  
  // Connection indicators
  tft.setTextSize(1);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setCursor(260, 8);
    tft.print("WiFi");
  }
  if (trackerState.gpsValid) {
    tft.setCursor(260, 16);
    tft.print("GPS");
  }
  
  // Current position
  float currentEl = motorPos.elevation * DEGREES_PER_PULSE;
  float currentAz = motorPos.azimuth * DEGREES_PER_PULSE;
  while (currentAz < 0) currentAz += 360.0;
  while (currentAz >= 360) currentAz -= 360.0;
  
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  
  tft.setCursor(10, 35);
  tft.print("Az:");
  tft.print(currentAz, 1);
  tft.print((char)247);  // degree symbol
  
  tft.setCursor(10, 55);
  tft.print("El:");
  tft.print(currentEl, 1);
  tft.print((char)247);
  
  tft.setCursor(170, 35);
  tft.print("T:");
  tft.print(targetPos.azimuth, 1);
  tft.print((char)247);
  
  tft.setCursor(170, 55);
  tft.print("T:");
  tft.print(targetPos.elevation, 1);
  tft.print((char)247);
  
  // Status
  tft.setTextSize(2);
  tft.setCursor(10, 80);
  if (trackerState.tracking) {
    tft.setTextColor(CYAN);
    tft.print("TRACK: ");
    tft.setTextSize(1);
    char displayName[15];
    strncpy(displayName, satelliteName, sizeof(displayName) - 1);
    displayName[sizeof(displayName) - 1] = '\0';
    tft.print(displayName);
  } else {
    tft.setTextColor(GRAY);
    tft.print("IDLE");
  }
  
  // Buttons
  Button buttons[5] = {
    {10, 105, 70, 40, TAG_HOME, "HOME", GREEN},
    {90, 105, 70, 40, TAG_TRACK, "TRACK", GREEN},
    {170, 105, 70, 40, TAG_STOP, "STOP", RED},
    {250, 105, 60, 40, TAG_MANUAL, "MAN", GREEN},
    {10, 155, 300, 35, TAG_SETTINGS, "SETTINGS", BLUE}
  };
  
  for (int i = 0; i < 5; i++) {
    drawButton(buttons[i]);
  }
  
  // GPS info
  tft.setTextSize(1);
  tft.setTextColor(WHITE);
  tft.setCursor(10, 205);
  tft.print("Lat:");
  tft.print(trackerState.latitude, 4);
  tft.setCursor(10, 215);
  tft.print("Lon:");
  tft.print(trackerState.longitude, 4);
  tft.setCursor(10, 225);
  tft.print("Alt:");
  tft.print(trackerState.altitude, 0);
  tft.print("m");
}

void drawSettingsScreen() {
  tft.fillScreen(BLACK);
  
  // Header
  tft.fillRect(0, 0, SCREEN_WIDTH, 30, BLUE);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(90, 8);
  tft.print("SETTINGS");
  
  // WiFi Status Section
  tft.setTextSize(2);
  tft.setCursor(10, 40);
  tft.print("WiFi:");
  tft.setTextSize(1);
  tft.setCursor(70, 45);
  if (WiFi.status() == WL_CONNECTED) {
    tft.setTextColor(GREEN);
    tft.print("Connected");
    tft.setTextColor(WHITE);
    tft.setCursor(70, 55);
    tft.print(WiFi.localIP());
  } else if (wifiConfigured) {
    tft.setTextColor(ORANGE);
    tft.print("Configured but not connected");
  } else {
    tft.setTextColor(RED);
    tft.print("Not configured");
  }
  tft.setTextColor(WHITE);
  
  // Current SSID
  if (strlen(wifiSSID) > 0) {
    tft.setTextSize(1);
    tft.setCursor(70, 65);
    tft.print("SSID: ");
    char displaySSID[16];
    strncpy(displaySSID, wifiSSID, sizeof(displaySSID) - 1);
    displaySSID[sizeof(displaySSID) - 1] = '\0';
    tft.print(displaySSID);
  }
  
  // Compass Status Section
  tft.setTextSize(2);
  tft.setCursor(10, 85);
  tft.print("Compass:");
  tft.setTextSize(1);
  tft.setCursor(100, 90);
  
  if (compassCalibrating) {
    tft.setTextColor(CYAN);
    unsigned long elapsed = getCalibrationDuration();  // From compass module
    tft.print("Calibrating... ");
    tft.print(elapsed);
    tft.print("s");
  } else {
    tft.setTextColor(GREEN);
    tft.print("Ready");
  }
  tft.setTextColor(WHITE);
  
  // Current heading display
  tft.setTextSize(1);
  tft.setCursor(100, 100);
  float heading = readCompassHeading();
  tft.print("Heading: ");
  tft.print(heading, 1);
  tft.print((char)247);
  
  // Settings Buttons
  Button buttons[4] = {
    {10, 120, 300, 35, TAG_WIFI_CONFIG, "Configure WiFi", BLUE},
    {10, 160, 145, 35, TAG_COMPASS_CAL, compassCalibrating ? "Stop Cal" : "Cal Compass", compassCalibrating ? ORANGE : GREEN},
    {165, 160, 145, 35, TAG_COMPASS_TEST, "Test Heading", CYAN},
    {10, 205, 300, 30, TAG_BACK, "BACK TO MAIN", ORANGE}
  };
  
  for (int i = 0; i < 4; i++) {
    drawButton(buttons[i]);
  }
}

void drawManualControlScreen() {
  tft.fillScreen(BLACK);
  
  // Header
  tft.fillRect(0, 0, SCREEN_WIDTH, 25, BLUE);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(40, 5);
  tft.print("MANUAL CONTROL");
  
  // Current position
  float currentEl = motorPos.elevation * DEGREES_PER_PULSE;
  float currentAz = motorPos.azimuth * DEGREES_PER_PULSE;
  while (currentAz < 0) currentAz += 360.0;
  while (currentAz >= 360) currentAz -= 360.0;
  
  tft.setTextSize(2);
  tft.setCursor(20, 35);
  tft.print("Az:");
  tft.print(currentAz, 1);
  tft.print("  El:");
  tft.print(currentEl, 1);
  
  // Azimuth controls
  tft.setTextSize(2);
  tft.setCursor(10, 65);
  tft.print("Azimuth:");
  
  Button azButtons[2] = {
    {10, 90, 90, 45, TAG_AZ_LEFT, "<<", GREEN},
    {220, 90, 90, 45, TAG_AZ_RIGHT, ">>", GREEN}
  };
  
  for (int i = 0; i < 2; i++) {
    drawButton(azButtons[i]);
  }
  
  // Elevation controls
  tft.setCursor(10, 145);
  tft.print("Elevation:");
  
  Button elButtons[2] = {
    {10, 170, 90, 45, TAG_EL_UP, "UP", GREEN},
    {220, 170, 90, 45, TAG_EL_DOWN, "DOWN", GREEN}
  };
  
  for (int i = 0; i < 2; i++) {
    drawButton(elButtons[i]);
  }
  
  // Back button
  Button backBtn = {110, 220, 100, 18, TAG_BACK, "BACK", ORANGE};
  drawButton(backBtn);
}

// Serial command handler for WiFi setup
void handleSerialWiFiSetup() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.length() == 0) return;
    
    // Check for field selection commands
    if (input.equalsIgnoreCase("SSID")) {
      currentField = FIELD_SSID;
      Serial.println("Enter SSID:");
      return;
    } else if (input.equalsIgnoreCase("PASSWORD") || input.equalsIgnoreCase("PASS")) {
      currentField = FIELD_PASSWORD;
      Serial.println("Enter Password:");
      return;
    }
    
    // Store input in appropriate field
    if (currentField == FIELD_SSID) {
      if (input.length() < sizeof(tempSSID)) {
        strncpy(tempSSID, input.c_str(), sizeof(tempSSID) - 1);
        tempSSID[sizeof(tempSSID) - 1] = '\0';
        Serial.print("SSID set to: ");
        Serial.println(tempSSID);
        Serial.println("Type 'PASSWORD' then enter password");
      } else {
        Serial.println("ERROR: SSID too long (max 31 chars)");
      }
    } else if (currentField == FIELD_PASSWORD) {
      if (input.length() < sizeof(tempPassword)) {
        strncpy(tempPassword, input.c_str(), sizeof(tempPassword) - 1);
        tempPassword[sizeof(tempPassword) - 1] = '\0';
        Serial.println("Password set (hidden)");
        Serial.println("Touch 'Connect' button to apply");
      } else {
        Serial.println("ERROR: Password too long (max 63 chars)");
      }
    }
    
    displayNeedsUpdate = true;
  }
}

void handleDisplayTouch() {
  unsigned long now = millis();
  
  // Handle serial input for WiFi setup
  if (currentScreen == SCREEN_SETUP || currentScreen == SCREEN_SETTINGS) {
    handleSerialWiFiSetup();
  }
  
  // Check for touch
  if (!touch.touched()) {
    wasTouched = false;
    return;
  }
  
  // Debounce check
  if (wasTouched || (now - lastTouchTime) < TOUCH_DEBOUNCE_MS) {
    return;
  }
  
  // Get touch point
  TS_Point p = touch.getPoint();
  
  // Map coordinates
  int16_t x = map(p.y, 0, 320, 0, 320);
  int16_t y = map(p.x, 0, 240, 239, 0);
  
  // Constrain to screen bounds
  x = constrain(x, 0, 319);
  y = constrain(y, 0, 239);
  
  wasTouched = true;
  lastTouchTime = now;
  
  // Determine which button was touched based on screen
  uint8_t tag = TAG_NONE;
  
  if (currentScreen == SCREEN_SETUP) {
    Button buttons[2] = {
      {10, 185, 140, 40, TAG_SETUP_CONNECT, "", GREEN},
      {170, 185, 140, 40, TAG_SETUP_SKIP, "", ORANGE}
    };
    tag = getTouchedTag(x, y, buttons, 2);
  }
  else if (currentScreen == SCREEN_MAIN) {
    Button buttons[5] = {
      {10, 105, 70, 40, TAG_HOME, "", GREEN},
      {90, 105, 70, 40, TAG_TRACK, "", GREEN},
      {170, 105, 70, 40, TAG_STOP, "", RED},
      {250, 105, 60, 40, TAG_MANUAL, "", GREEN},
      {10, 155, 300, 35, TAG_SETTINGS, "", BLUE}
    };
    tag = getTouchedTag(x, y, buttons, 5);
  }
  else if (currentScreen == SCREEN_SETTINGS) {
    Button buttons[4] = {
      {10, 120, 300, 35, TAG_WIFI_CONFIG, "", BLUE},
      {10, 160, 145, 35, TAG_COMPASS_CAL, "", GREEN},
      {165, 160, 145, 35, TAG_COMPASS_TEST, "", CYAN},
      {10, 205, 300, 30, TAG_BACK, "", ORANGE}
    };
    tag = getTouchedTag(x, y, buttons, 4);
  }
  else if (currentScreen == SCREEN_MANUAL_CONTROL) {
    Button buttons[5] = {
      {10, 90, 90, 45, TAG_AZ_LEFT, "", GREEN},
      {220, 90, 90, 45, TAG_AZ_RIGHT, "", GREEN},
      {10, 170, 90, 45, TAG_EL_UP, "", GREEN},
      {220, 170, 90, 45, TAG_EL_DOWN, "", GREEN},
      {110, 220, 100, 18, TAG_BACK, "", ORANGE}
    };
    tag = getTouchedTag(x, y, buttons, 5);
  }
  
  if (tag == TAG_NONE) {
    return;
  }
  
  lastTag = tag;
  
  Serial.print("Touch at X:");
  Serial.print(x);
  Serial.print(" Y:");
  Serial.print(y);
  Serial.print(" Tag:");
  Serial.println(tag);
  
  // Handle button press
  switch (tag) {
    case TAG_HOME:
      Serial.println("Home button");
      trackerState.tracking = false;
      homeAxes();
      displayNeedsUpdate = true;
      break;
      
    case TAG_TRACK:
      Serial.println("Track button");
      if (trackerState.tleValid) {
        trackerState.tracking = true;
      } else {
        Serial.println("No TLE loaded!");
      }
      displayNeedsUpdate = true;
      break;
      
    case TAG_STOP:
      Serial.println("Stop button");
      trackerState.tracking = false;
      stopAllMotors();
      displayNeedsUpdate = true;
      break;
      
    case TAG_MANUAL:
      Serial.println("Manual button");
      trackerState.tracking = false;
      currentScreen = SCREEN_MANUAL_CONTROL;
      displayNeedsUpdate = true;
      break;
      
    case TAG_SETTINGS:
      Serial.println("Settings button");
      currentScreen = SCREEN_SETTINGS;
      displayNeedsUpdate = true;
      break;
      
    case TAG_WIFI_CONFIG:
      Serial.println("WiFi Config button");
      Serial.println("\n=== WiFi Configuration ===");
      Serial.println("Type 'SSID' and press Enter, then enter your SSID");
      Serial.println("Type 'PASSWORD' and press Enter, then enter your password");
      Serial.println("Then touch 'Connect' on screen");
      currentField = FIELD_SSID;
      currentScreen = SCREEN_SETUP;
      displayNeedsUpdate = true;
      break;
      
    case TAG_COMPASS_CAL:
      if (!compassCalibrating) {
        Serial.println("Starting compass calibration");
        compassCalibrating = true;
        calibrationStartTime = millis();
        startBackgroundCalibration();  // Compass module handles the work
        Serial.println("Rotate device through all orientations");
        Serial.println("Touch 'Stop Cal' when done (15+ seconds recommended)");
      } else {
        Serial.println("Stopping compass calibration");
        compassCalibrating = false;
        stopBackgroundCalibration();  // Compass module processes and applies
      }
      displayNeedsUpdate = true;
      break;
      
    case TAG_COMPASS_TEST:
      Serial.println("Compass Test");
      for (int i = 0; i < 10; i++) {
        float heading = readCompassHeading();
        Serial.print("Heading: ");
        Serial.print(heading, 2);
        Serial.println(" degrees");
        delay(200);
      }
      displayNeedsUpdate = true;
      break;
      
    case TAG_AZ_LEFT:
      Serial.println("Azimuth left");
      targetPos.azimuth -= 5.0;
      if (targetPos.azimuth < 0) targetPos.azimuth += 360.0;
      displayNeedsUpdate = true;
      break;
      
    case TAG_AZ_RIGHT:
      Serial.println("Azimuth right");
      targetPos.azimuth += 5.0;
      if (targetPos.azimuth >= 360) targetPos.azimuth -= 360.0;
      displayNeedsUpdate = true;
      break;
      
    case TAG_EL_UP:
      Serial.println("Elevation up");
      targetPos.elevation += 5.0;
      targetPos.elevation = constrain(targetPos.elevation, MIN_ELEVATION, MAX_ELEVATION);
      displayNeedsUpdate = true;
      break;
      
    case TAG_EL_DOWN:
      Serial.println("Elevation down");
      targetPos.elevation -= 5.0;
      targetPos.elevation = constrain(targetPos.elevation, MIN_ELEVATION, MAX_ELEVATION);
      displayNeedsUpdate = true;
      break;
      
    case TAG_BACK:
      currentScreen = SCREEN_MAIN;
      displayNeedsUpdate = true;
      break;
      
    case TAG_SETUP_CONNECT:
      Serial.println("Connect button");
      memset(wifiSSID, 0, sizeof(wifiSSID));
      strncpy(wifiSSID, tempSSID, sizeof(wifiSSID) - 1);
      wifiSSID[sizeof(wifiSSID) - 1] = '\0';
      
      memset(wifiPassword, 0, sizeof(wifiPassword));
      strncpy(wifiPassword, tempPassword, sizeof(wifiPassword) - 1);
      wifiPassword[sizeof(wifiPassword) - 1] = '\0';
      
      wifiConfigured = true;
      currentScreen = SCREEN_MAIN;
      displayNeedsUpdate = true;
      
      // Trigger WiFi reconnection
      Serial.println("WiFi credentials updated - reconnecting...");
      WiFi.disconnect();
      delay(100);
      WiFi.begin(wifiSSID, wifiPassword);
      break;
      
    case TAG_SETUP_SKIP:
      Serial.println("Skip WiFi");
      wifiConfigured = false;
      currentScreen = SCREEN_MAIN;
      displayNeedsUpdate = true;
      break;
  }
}

void updateDisplay() {
  if (!displayNeedsUpdate) return;
  
  switch (currentScreen) {
    case SCREEN_SETUP:
      drawSetupScreen();
      break;
    case SCREEN_MAIN:
      drawMainScreen();
      break;
    case SCREEN_SETTINGS:
      drawSettingsScreen();
      break;
    case SCREEN_MANUAL_CONTROL:
      drawManualControlScreen();
      break;
    default:
      drawMainScreen();
      break;
  }
  
  displayNeedsUpdate = false;
}
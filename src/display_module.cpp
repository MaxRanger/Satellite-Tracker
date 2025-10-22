// ============================================================================
// display_module.cpp
// ============================================================================

#include "display_module.h"
#include "motor_control.h"
#include "compass_module.h"
#include "web_interface.h"

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

// WiFi setup state - REMOVED serial input handling
// WiFi configuration now done ONLY via serial_interface module
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

static bool keyboardVisible = false;
static bool shiftActive = false;
static const char keyboardChars[] = "1234567890qwertyuiopasdfghjklzxcvbnm ";
static const char keyboardCharsShift[] = "!@#$%^&*()QWERTYUIOPASDFGHJKLZXCVBNM ";

// Forward declarations
uint8_t getKeyboardTag(int16_t x, int16_t y);

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
  
  // Current field indicator
  tft.setTextSize(1);
  tft.setCursor(10, 35);
  tft.print(currentField == FIELD_SSID ? "SSID:" : "Password:");
  
  // Input field
  tft.drawRect(10, 48, 300, 25, CYAN);
  tft.setTextColor(WHITE);
  tft.setTextSize(2);
  tft.setCursor(15, 55);
  
  if (currentField == FIELD_SSID) {
    if (strlen(tempSSID) > 0) {
      char displaySSID[19];
      strncpy(displaySSID, tempSSID, sizeof(displaySSID) - 1);
      displaySSID[sizeof(displaySSID) - 1] = '\0';
      tft.print(displaySSID);
    }
  } else {
    if (strlen(tempPassword) > 0) {
      for (size_t i = 0; i < strlen(tempPassword) && i < 18; i++) {
        tft.print('*');
      }
    }
  }
  
  // Smaller buttons to make room for keyboard
  Button buttons[4] = {
    {10, 80, 60, 28, TAG_FIELD_SSID, "SSID", currentField == FIELD_SSID ? CYAN : BLUE},
    {75, 80, 60, 28, TAG_FIELD_PASSWORD, "Pass", currentField == FIELD_PASSWORD ? CYAN : BLUE},
    {140, 80, 60, 28, TAG_KEYBOARD, "Keys", GREEN},
    {205, 80, 50, 28, TAG_SETUP_CONNECT, "OK", (strlen(tempSSID) > 0) ? GREEN : GRAY}
  };
  
  for (int i = 0; i < 4; i++) {
    drawButton(buttons[i]);
  }
  
  // Serial help text at bottom (only if keyboard not visible)
  if (!keyboardVisible) {
    tft.setTextSize(1);
    tft.setTextColor(GRAY);
    tft.setCursor(10, 220);
    tft.print("Or use Serial: SETWIFI <ssid> <pass>");
  }
}

void drawKeyboard() {
  // Keyboard background - starts at y=115
  tft.fillRect(0, 115, SCREEN_WIDTH, 125, BLACK);
  
  const char* chars = shiftActive ? keyboardCharsShift : keyboardChars;
  
  // Draw keyboard keys - adjusted to fit below smaller buttons
  int keyWidth = 30;
  int keyHeight = 22;
  int startY = 118;
  
  // Row 1: 1234567890
  for (int i = 0; i < 10; i++) {
    int x = 5 + i * 31;
    Button btn = {(int16_t)x, (int16_t)startY, (int16_t)keyWidth, (int16_t)keyHeight, 
                  (uint8_t)(TAG_KB_CHAR_START + i), "", BLUE};
    drawButton(btn);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.setCursor(x + 10, startY + 4);
    tft.print(chars[i]);
  }
  
  // Row 2: qwertyuiop
  for (int i = 0; i < 10; i++) {
    int x = 5 + i * 31;
    Button btn = {(int16_t)x, (int16_t)(startY + 24), (int16_t)keyWidth, (int16_t)keyHeight,
                  (uint8_t)(TAG_KB_CHAR_START + 10 + i), "", BLUE};
    drawButton(btn);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.setCursor(x + 10, startY + 28);
    tft.print(chars[10 + i]);
  }
  
  // Row 3: asdfghjkl
  for (int i = 0; i < 9; i++) {
    int x = 20 + i * 31;
    Button btn = {(int16_t)x, (int16_t)(startY + 48), (int16_t)keyWidth, (int16_t)keyHeight,
                  (uint8_t)(TAG_KB_CHAR_START + 20 + i), "", BLUE};
    drawButton(btn);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.setCursor(x + 10, startY + 52);
    tft.print(chars[20 + i]);
  }
  
  // Row 4: zxcvbnm
  for (int i = 0; i < 7; i++) {
    int x = 35 + i * 31;
    Button btn = {(int16_t)x, (int16_t)(startY + 72), (int16_t)keyWidth, (int16_t)keyHeight,
                  (uint8_t)(TAG_KB_CHAR_START + 29 + i), "", BLUE};
    drawButton(btn);
    tft.setTextColor(WHITE);
    tft.setTextSize(2);
    tft.setCursor(x + 10, startY + 76);
    tft.print(chars[29 + i]);
  }
  
  // Bottom row: Shift, Space, Backspace, Done
  Button bottomRow[4] = {
    {5, 218, 45, 20, TAG_KB_SHIFT, "Shift", shiftActive ? ORANGE : GRAY},
    {55, 218, 120, 20, TAG_KB_SPACE, "Space", BLUE},
    {180, 218, 60, 20, TAG_KB_BACKSPACE, "Back", RED},
    {245, 218, 70, 20, TAG_KB_DONE, "Done", GREEN}
  };
  
  for (int i = 0; i < 4; i++) {
    drawButton(bottomRow[i]);
  }
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
    unsigned long elapsed = getCalibrationDuration();
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
    {10, 160, 145, 35, TAG_COMPASS_CAL, compassCalibrating ? "Stop Cal" : "Cal Compass", (uint16_t)(compassCalibrating ? ORANGE : GREEN)},
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

void handleDisplayTouch() {
  unsigned long now = millis();
  
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
    if (keyboardVisible) {
      // Check keyboard keys
      tag = getKeyboardTag(x, y);
    } else {
      Button buttons[4] = {
        {10, 80, 60, 28, TAG_FIELD_SSID, "", BLUE},
        {75, 80, 60, 28, TAG_FIELD_PASSWORD, "", BLUE},
        {140, 80, 60, 28, TAG_KEYBOARD, "", GREEN},
        {205, 80, 50, 28, TAG_SETUP_CONNECT, "", GREEN}
      };
      tag = getTouchedTag(x, y, buttons, 4);
    }
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
    case TAG_FIELD_SSID:
      currentField = FIELD_SSID;
      displayNeedsUpdate = true;
      break;
      
    case TAG_FIELD_PASSWORD:
      currentField = FIELD_PASSWORD;
      displayNeedsUpdate = true;
      break;
      
    case TAG_KEYBOARD:
      keyboardVisible = !keyboardVisible;
      if (keyboardVisible) {
        drawKeyboard();
      } else {
        displayNeedsUpdate = true;
      }
      break;
      
    case TAG_KB_SHIFT:
      shiftActive = !shiftActive;
      drawKeyboard();
      break;
      
    case TAG_KB_SPACE:
      if (currentField == FIELD_SSID && strlen(tempSSID) < sizeof(tempSSID) - 1) {
        strncat(tempSSID, " ", 1);
        displayNeedsUpdate = true;
      } else if (currentField == FIELD_PASSWORD && strlen(tempPassword) < sizeof(tempPassword) - 1) {
        strncat(tempPassword, " ", 1);
        displayNeedsUpdate = true;
      }
      break;
      
    case TAG_KB_BACKSPACE:
      if (currentField == FIELD_SSID && strlen(tempSSID) > 0) {
        tempSSID[strlen(tempSSID) - 1] = '\0';
        displayNeedsUpdate = true;
      } else if (currentField == FIELD_PASSWORD && strlen(tempPassword) > 0) {
        tempPassword[strlen(tempPassword) - 1] = '\0';
        displayNeedsUpdate = true;
      }
      break;
      
    case TAG_KB_DONE:
      keyboardVisible = false;
      displayNeedsUpdate = true;
      break;
      
    default:
      // Check if it's a character key
      if (tag >= TAG_KB_CHAR_START && tag < TAG_KB_CHAR_START + 37) {
        int charIndex = tag - TAG_KB_CHAR_START;
        const char* chars = shiftActive ? keyboardCharsShift : keyboardChars;
        char c = chars[charIndex];
        
        if (currentField == FIELD_SSID && strlen(tempSSID) < sizeof(tempSSID) - 1) {
          strncat(tempSSID, &c, 1);
          displayNeedsUpdate = true;
        } else if (currentField == FIELD_PASSWORD && strlen(tempPassword) < sizeof(tempPassword) - 1) {
          strncat(tempPassword, &c, 1);
          displayNeedsUpdate = true;
        }
        
        // Auto-disable shift after character
        if (shiftActive) {
          shiftActive = false;
          drawKeyboard();
        }
      }
      break;
  }
  
  // Original button handlers continue below
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
      currentScreen = SCREEN_SETUP;
      keyboardVisible = false;
      displayNeedsUpdate = true;
      break;
      
    case TAG_COMPASS_CAL:
      if (!compassCalibrating) {
        Serial.println("Starting compass calibration");
        compassCalibrating = true;
        calibrationStartTime = millis();
        startBackgroundCalibration();
        Serial.println("Rotate device through all orientations");
        Serial.println("Touch 'Stop Cal' when done (15+ seconds recommended)");
      } else {
        Serial.println("Stopping compass calibration");
        compassCalibrating = false;
        stopBackgroundCalibration();
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
      
      Serial.println("WiFi credentials updated - connecting...");
      initWebInterface();
      break;
      
    case TAG_SETUP_SKIP:
      Serial.println("Skip WiFi");
      wifiConfigured = false;
      currentScreen = SCREEN_MAIN;
      displayNeedsUpdate = true;
      break;
  }
}

// Helper function to detect keyboard key press
uint8_t getKeyboardTag(int16_t x, int16_t y) {
  int keyWidth = 30;
  int keyHeight = 22;
  int startY = 118;
  
  // Row 1: 1234567890 (10 keys)
  if (y >= startY && y <= startY + keyHeight) {
    for (int i = 0; i < 10; i++) {
      int kx = 5 + i * 31;
      if (x >= kx && x <= kx + keyWidth) {
        return TAG_KB_CHAR_START + i;
      }
    }
  }
  
  // Row 2: qwertyuiop (10 keys)
  if (y >= startY + 24 && y <= startY + 24 + keyHeight) {
    for (int i = 0; i < 10; i++) {
      int kx = 5 + i * 31;
      if (x >= kx && x <= kx + keyWidth) {
        return TAG_KB_CHAR_START + 10 + i;
      }
    }
  }
  
  // Row 3: asdfghjkl (9 keys)
  if (y >= startY + 48 && y <= startY + 48 + keyHeight) {
    for (int i = 0; i < 9; i++) {
      int kx = 20 + i * 31;
      if (x >= kx && x <= kx + keyWidth) {
        return TAG_KB_CHAR_START + 20 + i;
      }
    }
  }
  
  // Row 4: zxcvbnm (7 keys)
  if (y >= startY + 72 && y <= startY + 72 + keyHeight) {
    for (int i = 0; i < 7; i++) {
      int kx = 35 + i * 31;
      if (x >= kx && x <= kx + keyWidth) {
        return TAG_KB_CHAR_START + 29 + i;
      }
    }
  }
  
  // Bottom row buttons
  Button bottomRow[4] = {
    {5, 218, 45, 20, TAG_KB_SHIFT, "", GRAY},
    {55, 218, 120, 20, TAG_KB_SPACE, "", BLUE},
    {180, 218, 60, 20, TAG_KB_BACKSPACE, "", RED},
    {245, 218, 70, 20, TAG_KB_DONE, "", GREEN}
  };
  
  return getTouchedTag(x, y, bottomRow, 4);
}

void updateDisplay() {
  if (!displayNeedsUpdate) return;
  
  // Update tempSSID/tempPassword from global wifiSSID/wifiPassword
  if (strlen(wifiSSID) > 0 && strlen(tempSSID) == 0) {
    strncpy(tempSSID, wifiSSID, sizeof(tempSSID) - 1);
    tempSSID[sizeof(tempSSID) - 1] = '\0';
  }
  if (strlen(wifiPassword) > 0 && strlen(tempPassword) == 0) {
    strncpy(tempPassword, wifiPassword, sizeof(tempPassword) - 1);
    tempPassword[sizeof(tempPassword) - 1] = '\0';
  }
  
  switch (currentScreen) {
    case SCREEN_SETUP:
      drawSetupScreen();
      if (keyboardVisible) {
        drawKeyboard();
      }
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
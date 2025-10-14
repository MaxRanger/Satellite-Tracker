// ============================================================================
// display_module.cpp
// ============================================================================

#include "display_module.h"
#include "motor_control.h"

// External references to shared data (defined in shared_data.cpp)
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
    tft.print(tempSSID);
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
    // Show asterisks
    for (int i = 0; i < strlen(tempPassword) && i < 20; i++) {
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
    tft.print(satelliteName);
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
  // Check for touch
  if (!touch.touched()) {
    wasTouched = false;
    lastTag = TAG_NONE;
    return;
  }
  
  // Get touch point
  TS_Point p = touch.getPoint();
  
  // Map coordinates (adjust based on rotation and calibration)
  //int16_t x = map(p.x, 0, 240, 0, SCREEN_WIDTH);
  //int16_t y = map(p.y, 0, 320, 0, SCREEN_HEIGHT);
  
  int16_t  x = map(p.y, 0, 320, 0, 320);     // Y maps to screen X (0-319)
  int16_t  y = map(p.x, 0, 240, 239, 0);    // X maps to screen Y, inverted, offset by 65
  
  // Constrain to screen bounds
  x = constrain(x, 0, 319);
  y = constrain(y, 0, 239);

  // Detect new touch (rising edge)
  if (!wasTouched) {
    wasTouched = true;
    
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
    
    if (tag == TAG_NONE || tag == lastTag) {
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
        strncpy(wifiSSID, tempSSID, 31);
        strncpy(wifiPassword, tempPassword, 63);
        wifiSSID[31] = '\0';
        wifiPassword[63] = '\0';
        wifiConfigured = true;
        currentScreen = SCREEN_MAIN;
        displayNeedsUpdate = true;
        break;
        
      case TAG_SETUP_SKIP:
        Serial.println("Skip WiFi");
        wifiConfigured = false;
        currentScreen = SCREEN_MAIN;
        displayNeedsUpdate = true;
        break;
    }
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
    case SCREEN_MANUAL_CONTROL:
      drawManualControlScreen();
      break;
    default:
      drawMainScreen();
      break;
  }
  
  displayNeedsUpdate = false;
}
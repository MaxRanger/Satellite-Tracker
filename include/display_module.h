/*
 * display_module.h - Adafruit 2.8" TFT display (PID 2423) interface
 * ILI9341 TFT driver with FT6206 capacitive touch
 * UPDATED WITH SETTINGS SCREEN
 */

#ifndef DISPLAY_MODULE_H
#define DISPLAY_MODULE_H

#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>
#include "config.h"
#include "shared_data.h"

// Button tags
#define TAG_NONE        0
#define TAG_HOME        1
#define TAG_TRACK       2
#define TAG_STOP        3
#define TAG_MANUAL      4
#define TAG_SETTINGS    5
#define TAG_BACK        10
#define TAG_AZ_LEFT     11
#define TAG_AZ_RIGHT    12
#define TAG_EL_UP       13
#define TAG_EL_DOWN     14
#define TAG_SETUP_CONNECT  15
#define TAG_SETUP_SKIP     16
#define TAG_WIFI_CONFIG    17
#define TAG_COMPASS_CAL    18
#define TAG_COMPASS_TEST   19
#define TAG_KEYBOARD       20
#define TAG_KB_CHAR_START  100  // 100-199 for keyboard characters
#define TAG_KB_BACKSPACE   200
#define TAG_KB_SPACE       201
#define TAG_KB_DONE        202
#define TAG_KB_SHIFT       203
#define TAG_FIELD_SSID     204
#define TAG_FIELD_PASSWORD 205

// Color definitions (RGB565)
#define BLACK           0x0000
#define WHITE           0xFFFF
#define RED             0xF800
#define GREEN           0x07E0
#define BLUE            0x001F
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define ORANGE          0xFC00
#define GRAY            0x7BEF

// Initialize display
void initDisplay();

// Update display
void updateDisplay();

// Handle touch input
void handleDisplayTouch();

// Screen drawing functions
void drawSetupScreen();
void drawMainScreen();
void drawSettingsScreen();
void drawManualControlScreen();

// Button structure
struct Button {
  int16_t x, y, w, h;
  uint8_t tag;
  const char* label;
  uint16_t color;
};

// Compass calibration state (accessed by main loop)
extern bool compassCalibrating;
extern unsigned long calibrationStartTime;

#endif // DISPLAY_MODULE_H
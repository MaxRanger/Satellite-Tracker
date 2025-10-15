/*
 * led_module.h - WS2812 LED ring interface
 * 50 RGB LEDs driven by PIO for status indication
 */

#ifndef LED_MODULE_H
#define LED_MODULE_H

#include <Arduino.h>
#include "config.h"
#include "hardware/pio.h"

// LED ring modes
typedef enum {
  LED_MODE_OFF = 0,
  LED_MODE_STEADY_GREEN,      // Normal operation
  LED_MODE_FLASH_RED,         // Emergency stop
  LED_MODE_FLASH_YELLOW,      // GPS acquisition
  LED_MODE_FLASH_BLUE,        // Compass calibration
  LED_MODE_RAINBOW,           // Test/demo mode
  LED_MODE_CUSTOM             // User-defined pattern
} LEDMode;

// RGB color structure
struct RGBColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

// ============================================================================
// PUBLIC API
// ============================================================================

// Initialize LED module (sets up PIO)
void initLEDs();

// Set LED mode (automatic pattern control)
void setLEDMode(LEDMode mode);

// Get current LED mode
LEDMode getLEDMode();

// Update LEDs (call periodically from main loop - handles animations)
void updateLEDs();

// Manual control functions (when mode is LED_MODE_CUSTOM)
void setAllLEDs(RGBColor color);
void setLED(uint8_t index, RGBColor color);
void setLEDBrightness(uint8_t brightness); // 0-255
void showLEDs(); // Push buffer to LEDs

// Helper functions for common colors
RGBColor RGB(uint8_t r, uint8_t g, uint8_t b);
RGBColor colorRed();
RGBColor colorGreen();
RGBColor colorBlue();
RGBColor colorYellow();
RGBColor colorOff();

#endif // LED_MODULE_H
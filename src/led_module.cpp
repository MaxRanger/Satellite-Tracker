// ============================================================================
// led_module.cpp - WS2812 LED ring implementation with PIO
// ============================================================================

#include "led_module.h"
#include "hardware/clocks.h"

// LED configuration
#define NUM_LEDS 50
#define LED_BRIGHTNESS_DEFAULT 128  // 0-255, 50% brightness

// PIO configuration
static PIO led_pio = pio1;  // Use PIO1 (PIO0 used by encoders)
static uint led_sm = 0;      // State machine 0

// LED buffer (GRB format for WS2812)
static uint32_t ledBuffer[NUM_LEDS];
static uint8_t globalBrightness = LED_BRIGHTNESS_DEFAULT;
static LEDMode currentMode = LED_MODE_STEADY_GREEN;

// Animation state
static unsigned long lastUpdate = 0;
static bool flashState = false;
static uint16_t animationFrame = 0;

// ============================================================================
// WS2812 PIO PROGRAM
// ============================================================================

// PIO program for WS2812 timing
// Based on Raspberry Pi Pico examples
static const uint16_t ws2812_program_instructions[] = {
    //     .wrap_target
    0x6221, //  0: out    x, 1            side 0 [2]
    0x1024, //  1: jmp    !x, 4           side 1 [0]
    0x1400, //  2: jmp    0               side 1 [4]
    0xa042, //  3: nop                    side 0 [4]
    //     .wrap
};

static const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = 4,
    .origin = -1,
};

// PIO helper function to get default config
static inline pio_sm_config ws2812_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + 3);
    sm_config_set_sideset(&c, 1, false, false);
    return c;
}

// Initialize WS2812 PIO
static inline void ws2812_program_init(PIO pio, uint sm, uint offset, uint pin, float freq) {
    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    
    pio_sm_config c = ws2812_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, false, true, 24);  // Shift out 24 bits (GRB)
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX); // Use TX FIFO only
    
    // Calculate clock divider for 800kHz WS2812 timing
    float div = (float)clock_get_hz(clk_sys) / (freq * 8.0f);
    sm_config_set_clkdiv(&c, div);
    
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

// ============================================================================
// LOW-LEVEL LED FUNCTIONS
// ============================================================================

// Apply gamma correction and brightness
static uint32_t applyBrightness(uint8_t r, uint8_t g, uint8_t b) {
  // Apply brightness
  r = (r * globalBrightness) / 255;
  g = (g * globalBrightness) / 255;
  b = (b * globalBrightness) / 255;
  
  // WS2812 uses GRB format
  return (g << 16) | (r << 8) | b;
}

// Send data to LEDs via PIO
static void pushToLEDs() {
  for (int i = 0; i < NUM_LEDS; i++) {
    pio_sm_put_blocking(led_pio, led_sm, ledBuffer[i]);
  }
}

// ============================================================================
// COLOR HELPER FUNCTIONS
// ============================================================================

RGBColor RGB(uint8_t r, uint8_t g, uint8_t b) {
  return {r, g, b};
}

RGBColor colorRed() { return {255, 0, 0}; }
RGBColor colorGreen() { return {0, 255, 0}; }
RGBColor colorBlue() { return {0, 0, 255}; }
RGBColor colorYellow() { return {255, 255, 0}; }
RGBColor colorOff() { return {0, 0, 0}; }

// ============================================================================
// ANIMATION FUNCTIONS
// ============================================================================

void animateSteadyGreen() {
  RGBColor green = colorGreen();
  for (int i = 0; i < NUM_LEDS; i++) {
    ledBuffer[i] = applyBrightness(green.r, green.g, green.b);
  }
}

void animateFlashRed() {
  RGBColor color = flashState ? colorRed() : colorOff();
  for (int i = 0; i < NUM_LEDS; i++) {
    ledBuffer[i] = applyBrightness(color.r, color.g, color.b);
  }
}

void animateFlashYellow() {
  RGBColor color = flashState ? colorYellow() : colorOff();
  for (int i = 0; i < NUM_LEDS; i++) {
    ledBuffer[i] = applyBrightness(color.r, color.g, color.b);
  }
}

void animateFlashBlue() {
  RGBColor color = flashState ? colorBlue() : colorOff();
  for (int i = 0; i < NUM_LEDS; i++) {
    ledBuffer[i] = applyBrightness(color.r, color.g, color.b);
  }
}

void animateRainbow() {
  for (int i = 0; i < NUM_LEDS; i++) {
    // Create rainbow effect
    uint16_t hue = (animationFrame + (i * 65536 / NUM_LEDS)) & 0xFFFF;
    
    // Simple HSV to RGB conversion (hue only, full saturation and value)
    uint8_t sector = hue >> 13;  // 0-7
    uint8_t offset = (hue >> 5) & 0xFF;
    
    uint8_t r, g, b;
    switch(sector) {
      case 0: r = 255; g = offset; b = 0; break;
      case 1: r = 255-offset; g = 255; b = 0; break;
      case 2: r = 0; g = 255; b = offset; break;
      case 3: r = 0; g = 255-offset; b = 255; break;
      case 4: r = offset; g = 0; b = 255; break;
      case 5: r = 255; g = 0; b = 255-offset; break;
      case 6: r = 255; g = offset; b = 0; break;
      case 7: r = 255-offset; g = 255; b = 0; break;
    }
    
    ledBuffer[i] = applyBrightness(r, g, b);
  }
  animationFrame += 256;  // Rotate hue
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

void initLEDs() {
  Serial.println("Initializing WS2812 LED ring...");
  
  // Load PIO program
  uint offset = pio_add_program(led_pio, &ws2812_program);
  
  // Initialize PIO for WS2812 (800kHz)
  ws2812_program_init(led_pio, led_sm, offset, LED_DATA_PIN, 800000);
  
  // Clear LED buffer
  for (int i = 0; i < NUM_LEDS; i++) {
    ledBuffer[i] = 0;
  }
  
  // Set initial mode
  currentMode = LED_MODE_STEADY_GREEN;
  
  // Push initial state to LEDs
  pushToLEDs();
  
  Serial.println("WS2812 LED ring initialized");
  Serial.printf("  LEDs: %d\n", NUM_LEDS);
  Serial.printf("  Data pin: GPIO %d\n", LED_DATA_PIN);
  Serial.printf("  PIO: %d, SM: %d\n", led_pio == pio0 ? 0 : 1, led_sm);
}

void setLEDMode(LEDMode mode) {
  if (currentMode != mode) {
    currentMode = mode;
    lastUpdate = 0;  // Force immediate update
    flashState = false;
    animationFrame = 0;
    Serial.print("LED mode: ");
    switch(mode) {
      case LED_MODE_OFF: Serial.println("OFF"); break;
      case LED_MODE_STEADY_GREEN: Serial.println("STEADY GREEN"); break;
      case LED_MODE_FLASH_RED: Serial.println("FLASH RED"); break;
      case LED_MODE_FLASH_YELLOW: Serial.println("FLASH YELLOW"); break;
      case LED_MODE_FLASH_BLUE: Serial.println("FLASH BLUE"); break;
      case LED_MODE_RAINBOW: Serial.println("RAINBOW"); break;
      case LED_MODE_CUSTOM: Serial.println("CUSTOM"); break;
    }
  }
}

LEDMode getLEDMode() {
  return currentMode;
}

void updateLEDs() {
  unsigned long now = millis();
  bool needUpdate = false;
  
  switch(currentMode) {
    case LED_MODE_OFF:
      if (now - lastUpdate >= 1000) {  // Update once per second
        for (int i = 0; i < NUM_LEDS; i++) {
          ledBuffer[i] = 0;
        }
        needUpdate = true;
        lastUpdate = now;
      }
      break;
      
    case LED_MODE_STEADY_GREEN:
      if (now - lastUpdate >= 1000) {  // Update once per second
        animateSteadyGreen();
        needUpdate = true;
        lastUpdate = now;
      }
      break;
      
    case LED_MODE_FLASH_RED:
      if (now - lastUpdate >= 500) {  // Flash at 1 Hz
        flashState = !flashState;
        animateFlashRed();
        needUpdate = true;
        lastUpdate = now;
      }
      break;
      
    case LED_MODE_FLASH_YELLOW:
      if (now - lastUpdate >= 500) {  // Flash at 1 Hz
        flashState = !flashState;
        animateFlashYellow();
        needUpdate = true;
        lastUpdate = now;
      }
      break;
      
    case LED_MODE_FLASH_BLUE:
      if (now - lastUpdate >= 500) {  // Flash at 1 Hz
        flashState = !flashState;
        animateFlashBlue();
        needUpdate = true;
        lastUpdate = now;
      }
      break;
      
    case LED_MODE_RAINBOW:
      if (now - lastUpdate >= 50) {  // Update at 20 Hz
        animateRainbow();
        needUpdate = true;
        lastUpdate = now;
      }
      break;
      
    case LED_MODE_CUSTOM:
      // User controls, no automatic updates
      break;
  }
  
  if (needUpdate) {
    pushToLEDs();
  }
}

void setAllLEDs(RGBColor color) {
  for (int i = 0; i < NUM_LEDS; i++) {
    ledBuffer[i] = applyBrightness(color.r, color.g, color.b);
  }
}

void setLED(uint8_t index, RGBColor color) {
  if (index < NUM_LEDS) {
    ledBuffer[index] = applyBrightness(color.r, color.g, color.b);
  }
}

void setLEDBrightness(uint8_t brightness) {
  globalBrightness = brightness;
}

void showLEDs() {
  pushToLEDs();
}
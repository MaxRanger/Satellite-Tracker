// ============================================================================
// led_module.cpp - WS2812 LED ring implementation with PIO
// ============================================================================

#include "led_module.h"
#include "hardware/clocks.h"

// LED configuration
#define NUM_LEDS 24
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
static float clockDiv = 18.0f;  // gives ~1.2μs per bit

// ============================================================================
// WS2812 PIO PROGRAM
// ============================================================================

// Generates proper WS2812 timing: bit 1 = long HIGH, bit 0 = short HIGH
// This is the standard WS2812 PIO program from Raspberry Pi examples
const uint16_t ws2812_program_instructions[] = {
    //     .wrap_target
    0x6221, //  0: out    x, 1           side 0 [2] ; Side-set still takes place when instruction stalls
    0x1123, //  1: jmp    !x, 3          side 1 [1] ; Branch on the bit we shifted out. Positive pulse
    0x1400, //  2: jmp    0              side 1 [4] ; Continue driving high, for a long pulse
    0xa442, //  3: nop                   side 0 [4] ; Or drive low, for a short pulse
    //     .wrap
};

const struct pio_program ws2812_program = {
    .instructions = ws2812_program_instructions,
    .length = 4,
    .origin = -1,
};


// PIO helper function to get default config
static inline pio_sm_config ws2812_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset, offset + 3);
    sm_config_set_sideset(&c, 1, false, false);
    // CHANGE THIS LINE - add 'true' for inverted output:
    // sm_config_set_sideset(&c, 1, false, true);  // 1 bit, not optional, INVERTED
    //                                     ^^^^ this parameter inverts the output

    return c;
}

// Initialize WS2812 PIO
static inline void ws2812_program_init(PIO pio, uint sm, uint offset, uint pin, float freq) {
    // Load program
    offset = pio_add_program(pio, &ws2812_program);
    
    // Configure state machine
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 3);
    
    // Configure sideset - 1 bit, not optional, no pindirs
    sm_config_set_sideset(&c, 1, false, false);
    sm_config_set_sideset_pins(&c, pin);
    
    // Set clock divider
    sm_config_set_clkdiv(&c, clockDiv);
    
    // Configure shift - shift RIGHT (LSB first), autopull at 24 bits
    sm_config_set_out_shift(&c, true, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    
    // Initialize GPIO for PIO use
    pio_gpio_init(pio, pin);
    
    // Set pin direction to output
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);
    
    // Initialize and start state machine
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
    
    Serial.println("PIO initialized:");
    Serial.print("  PIO: pio1, SM: ");
    Serial.println(sm);
    Serial.print("  Program offset: ");
    Serial.println(offset);
    Serial.print("  Clock div: ");
    Serial.println(clockDiv, 3);
    Serial.print("  Pin: GPIO");
    Serial.println(pin);
}

// ============================================================================
// LOW-LEVEL LED FUNCTIONS
// ============================================================================
uint8_t reverse_byte(uint8_t b);
// Apply gamma correction and brightness
static uint32_t applyBrightness(uint32_t r, uint32_t g, uint32_t b) {
  // Apply brightness
  r = (r * globalBrightness) / 255;
  g = (g * globalBrightness) / 255;
  b = (b * globalBrightness) / 255;

  g = reverse_byte(g);
  r = reverse_byte(r);
  b = reverse_byte(b);

  // WS2812 uses GRB format, but fifo is shifted out LSB first, so we arrange as BGR
  return (b << 16) | (r << 8) | g;
}

// Reverse bits in a byte
uint8_t reverse_byte(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

// Send data to LEDs via PIO
static void pushToLEDs() {
  for (int i = 0; i < NUM_LEDS; i++) {
      pio_sm_put_blocking(led_pio, led_sm, ledBuffer[i]);
  }
  delayMicroseconds(60);  // RES time >50μs}
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
RGBColor colorYellow() { return {255, 192, 0}; }
RGBColor colorPurple() { return {220, 0, 255}; }
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

void initLEDs() {  Serial.println("Initializing WS2812 LED ring...");
  
  // Check if PIO is available
  Serial.printf("  PIO%d available: ", led_pio == pio0 ? 0 : 1);
  Serial.println(pio_can_add_program(led_pio, &ws2812_program) ? "YES" : "NO");

  // Load PIO program
  uint offset = pio_add_program(led_pio, &ws2812_program);
  Serial.print("PIO program loaded at offset: ");
  Serial.println(offset);
  
  // Initialize PIO for WS2812 (800kHz)
  ws2812_program_init(led_pio, led_sm, offset, LED_DATA_PIN, 800000);
  Serial.print("PIO initialized on pin: ");
  Serial.println(LED_DATA_PIN);
  
  // Clear LED buffer
  for (int i = 0; i < NUM_LEDS; i++) {
    ledBuffer[i] = 0x0;
  }
  pushToLEDs();
  delayMicroseconds(10);

  Serial.println("WS2812 LED ring initialized");
  Serial.printf("  LEDs: %d\n", NUM_LEDS);
  Serial.printf("  Data pin: GPIO %d\n", LED_DATA_PIN);
  Serial.printf("  PIO: %d, SM: %d\n", led_pio == pio0 ? 0 : 1, led_sm);

  currentMode = LED_MODE_STEADY_GREEN;  // Initial mode
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

uint32_t* getLEDBuffer() {
  return ledBuffer;
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

uint8_t getLEDBrightness() {
  return globalBrightness;
}

void showLEDs() {
  pushToLEDs();
}

void testLEDs() {
  Serial.println("\n=== LED Ring Test ===");
  
  int numLeds = 1;// NUM_LEDS;

  // Test 1: All red
  Serial.println("Test 1: All LEDs red");
  for (int i = 0; i < numLeds; i++) {
    ledBuffer[i] = applyBrightness(globalBrightness, 0, 0);
  }
  pushToLEDs();
  delay(1000);
  
  // Test 2: All green
  Serial.println("Test 2: All LEDs green");
  for (int i = 0; i < numLeds; i++) {
    ledBuffer[i] = applyBrightness(0, globalBrightness, 0);
  }
  pushToLEDs();
  delay(1000);
  
  // Test 3: All blue
  Serial.println("Test 3: All LEDs blue");
  for (int i = 0; i < numLeds; i++) {
    ledBuffer[i] = applyBrightness(0, 0, globalBrightness);
  }
  pushToLEDs();
  delay(1000);
  
  // Test 4: Chase pattern
  Serial.println("Test 4: Chase pattern");
  for (int j = 0; j < numLeds; j++) {
    for (int i = 0; i < numLeds; i++) {
      ledBuffer[i] = (i == j) ? applyBrightness(globalBrightness, globalBrightness, globalBrightness) : 0;
    }
    pushToLEDs();
    delay(50);
  }
  
  // Test 5: All off
  Serial.println("Test 5: All LEDs off");
  for (int i = 0; i < numLeds; i++) {
    ledBuffer[i] = 0;
  }
  pushToLEDs();
  
  Serial.println("LED test complete");
}
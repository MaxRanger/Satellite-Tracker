/*
 * button_module.h - Hardware button interface
 * 4 momentary-contact switches on LCD module
 */

#ifndef BUTTON_MODULE_H
#define BUTTON_MODULE_H

#include <Arduino.h>
#include "config.h"

// Button identifiers
typedef enum {
  BUTTON_NONE = 0,
  BUTTON_1 = 1,
  BUTTON_2 = 2,
  BUTTON_3 = 3,
  BUTTON_4 = 4
} ButtonID;

// Button event types
typedef enum {
  BUTTON_EVENT_NONE = 0,
  BUTTON_EVENT_PRESS,
  BUTTON_EVENT_RELEASE,
  BUTTON_EVENT_LONG_PRESS  // Held for > 1 second
} ButtonEvent;

// Button callback function type
typedef void (*ButtonCallback)(ButtonID button, ButtonEvent event);

// ============================================================================
// PUBLIC API
// ============================================================================

// Initialize button module
void initButtons();

// Set callback function for button events
// If callback is NULL, events are not reported
void setButtonCallback(ButtonCallback callback);

// Poll for button events (call from main loop if not using callback)
// Returns button that changed, or BUTTON_NONE
ButtonID pollButtons();

// Get current state of a button (true = pressed)
bool isButtonPressed(ButtonID button);

// Get time a button has been held (in milliseconds)
unsigned long getButtonHoldTime(ButtonID button);

#endif // BUTTON_MODULE_H
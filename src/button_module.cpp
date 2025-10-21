// ============================================================================
// button_module.cpp - Hardware button implementation
// ============================================================================

#include "button_module.h"

// Button pin assignments (using unused GPIOs)
const uint8_t buttonPins[5] = {
  0,              // BUTTON_NONE (placeholder)
  BUTTON_1_PIN,   // Button 1
  BUTTON_2_PIN,   // Button 2
  BUTTON_3_PIN,   // Button 3
  BUTTON_4_PIN    // Button 4
};

// Button state tracking
struct ButtonState {
  bool currentState;        // Current debounced state
  bool lastState;           // Previous state
  unsigned long lastChange; // Time of last state change
  unsigned long pressTime;  // Time button was pressed
  bool longPressFired;      // Has long press event been fired?
};

static ButtonState buttonStates[5] = {{false, false, 0, 0, false}};
static ButtonCallback buttonCallback = nullptr;

// Debounce time in milliseconds
#define DEBOUNCE_TIME_MS 50
#define LONG_PRESS_TIME_MS 1000

// Raw button states from ISR (volatile for thread safety)
volatile bool rawButtonStates[5] = {false, false, false, false, false};
volatile unsigned long lastInterruptTime[5] = {0, 0, 0, 0, 0};

// ISR enable flag - prevents interrupt storm during initialization
volatile bool isrEnabled = false;

// ============================================================================
// INTERRUPT HANDLERS (one per button)
// ============================================================================

void __not_in_flash_func(button1_ISR)() {
  if (!isrEnabled) return;
  
  unsigned long now = millis();
  if (now - lastInterruptTime[1] > DEBOUNCE_TIME_MS) {
    rawButtonStates[1] = !digitalRead(BUTTON_1_PIN); // Active low
    lastInterruptTime[1] = now;
  }
}

void __not_in_flash_func(button2_ISR)() {
  if (!isrEnabled) return;
  
  unsigned long now = millis();
  if (now - lastInterruptTime[2] > DEBOUNCE_TIME_MS) {
    rawButtonStates[2] = !digitalRead(BUTTON_2_PIN);
    lastInterruptTime[2] = now;
  }
}

void __not_in_flash_func(button3_ISR)() {
  if (!isrEnabled) return;
  
  unsigned long now = millis();
  if (now - lastInterruptTime[3] > DEBOUNCE_TIME_MS) {
    rawButtonStates[3] = !digitalRead(BUTTON_3_PIN);
    lastInterruptTime[3] = now;
  }
}

void __not_in_flash_func(button4_ISR)() {
  if (!isrEnabled) return;
  
  unsigned long now = millis();
  if (now - lastInterruptTime[4] > DEBOUNCE_TIME_MS) {
    rawButtonStates[4] = !digitalRead(BUTTON_4_PIN);
    lastInterruptTime[4] = now;
  }
}

// ============================================================================
// BUTTON PROCESSING
// ============================================================================

void processButton(ButtonID button) {
  if (button == BUTTON_NONE || button > BUTTON_4) return;
  
  ButtonState* state = &buttonStates[button];
  bool raw = rawButtonStates[button];
  unsigned long now = millis();
  
  // Update current state from debounced raw state
  if (raw != state->currentState) {
    state->lastState = state->currentState;
    state->currentState = raw;
    state->lastChange = now;
    
    if (state->currentState) {
      // Button pressed
      state->pressTime = now;
      state->longPressFired = false;
      
      if (buttonCallback) {
        buttonCallback(button, BUTTON_EVENT_PRESS);
      }
    } else {
      // Button released
      if (buttonCallback) {
        buttonCallback(button, BUTTON_EVENT_RELEASE);
      }
    }
  }
  
  // Check for long press
  if (state->currentState && !state->longPressFired) {
    if ((now - state->pressTime) >= LONG_PRESS_TIME_MS) {
      state->longPressFired = true;
      if (buttonCallback) {
        buttonCallback(button, BUTTON_EVENT_LONG_PRESS);
      }
    }
  }
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

void initButtons() {
  Serial.println("Initializing hardware buttons...");
  
  // Configure button pins as inputs with pullups
  pinMode(BUTTON_1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_2_PIN, INPUT_PULLUP);
  pinMode(BUTTON_3_PIN, INPUT_PULLUP);
  pinMode(BUTTON_4_PIN, INPUT_PULLUP);
  
  // Allow pullups to settle
  delay(50);
  
  // Initialize state by reading current button states
  for (int i = 1; i <= 4; i++) {
    buttonStates[i].currentState = false;
    buttonStates[i].lastState = false;
    buttonStates[i].lastChange = 0;
    buttonStates[i].pressTime = 0;
    buttonStates[i].longPressFired = false;
    
    // Read initial state (active low, so invert)
    bool initialState = !digitalRead(buttonPins[i]);
    rawButtonStates[i] = initialState;
    buttonStates[i].currentState = initialState;
    
    lastInterruptTime[i] = millis();
  }
  
  Serial.println("Button pins configured, settling...");
  delay(100);
  
  // Now attach interrupts with ISRs disabled initially
  isrEnabled = false;
  
  attachInterrupt(digitalPinToInterrupt(BUTTON_1_PIN), button1_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BUTTON_2_PIN), button2_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BUTTON_3_PIN), button3_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(BUTTON_4_PIN), button4_ISR, CHANGE);
  
  Serial.println("Interrupts attached, enabling...");
  delay(50);
  
  // Enable ISRs after everything is stable
  isrEnabled = true;
  
  Serial.println("Hardware buttons initialized");
  Serial.printf("Button 1: GPIO %d (state: %d)\n", BUTTON_1_PIN, rawButtonStates[1]);
  Serial.printf("Button 2: GPIO %d (state: %d)\n", BUTTON_2_PIN, rawButtonStates[2]);
  Serial.printf("Button 3: GPIO %d (state: %d)\n", BUTTON_3_PIN, rawButtonStates[3]);
  Serial.printf("Button 4: GPIO %d (state: %d)\n", BUTTON_4_PIN, rawButtonStates[4]);
}

void setButtonCallback(ButtonCallback callback) {
  buttonCallback = callback;
}

ButtonID pollButtons() {
  // Process all buttons and return first one that changed
  for (int i = 1; i <= 4; i++) {
    ButtonID button = (ButtonID)i;
    bool prevState = buttonStates[i].currentState;
    processButton(button);
    if (buttonStates[i].currentState != prevState) {
      return button;
    }
  }
  
  // Check for long presses even without state change
  for (int i = 1; i <= 4; i++) {
    processButton((ButtonID)i);
  }
  
  return BUTTON_NONE;
}

bool isButtonPressed(ButtonID button) {
  if (button == BUTTON_NONE || button > BUTTON_4) return false;
  return buttonStates[button].currentState;
}

unsigned long getButtonHoldTime(ButtonID button) {
  if (button == BUTTON_NONE || button > BUTTON_4) return 0;
  if (!buttonStates[button].currentState) return 0;
  return millis() - buttonStates[button].pressTime;
}
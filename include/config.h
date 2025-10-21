/*
 * config.h - Configuration and Pin Definitions
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// Motor Control Pins
#define MOTOR_E_PWM_FWD    6
#define MOTOR_E_PWM_REV    7
#define MOTOR_A_PWM_FWD    8
#define MOTOR_A_PWM_REV    9

// Motor Driver Enable Pins (optional, depends on driver type)
#define MOTOR_E_ENABLE     12
#define MOTOR_A_ENABLE     13

// Encoder Pins (must be consecutive for PIO)
#define ENCODER_E_BASE     2   // E_A=2, E_B=3
#define ENCODER_A_BASE     10  // A_A=10, A_B=11

// Index Sensor Pins
#define INDEX_E            24
#define INDEX_A            25

// Emergency Stop Pin (active low with internal pullup)
#define EMERGENCY_STOP_PIN 23  // GP23 (joystick button)

// GPS Serial (UART0)
#define GPS_RX             1   // GP1 Pin 2 (UART0 RX)
#define GPS_TX             0   // GP0 Pin 1 (UART0 TX)

// Adafruit 2.8" TFT Display (PID 2423) - ILI9341 with FT6206 Touch
#define TFT_MOSI           19  // GP19 Pin 25 (SPI TX)
#define TFT_MISO           16  // GP16 Pin 21 (SPI RX)
#define TFT_SCK            18  // GP18 Pin 24 (SPI SCK)
#define TFT_CS             17  // GP17 Pin 22 (Chip Select)
#define TFT_DC             14  // GP14 (Data/Command)

// FT6206 Capacitive Touch (I2C)
#define TOUCH_SDA          4   // GP4 Pin 6 (I2C SDA)
#define TOUCH_SCL          5   // GP5 Pin 7 (I2C SCL)

// I2C for QMC5883L Magnetometer (shares bus with touch - different I2C addresses)
#define I2C_SDA            4   // GP4 Pin 6 (Shared with TOUCH_SDA)
#define I2C_SCL            5   // GP5 Pin 7 (Shared with TOUCH_SCL)

// Hardware Buttons (4 momentary switches on LCD module)
#define BUTTON_1_PIN       20  // GP20 Pin 26
#define BUTTON_2_PIN       21  // GP21 Pin 27
#define BUTTON_3_PIN       15  // GP15 Pin 20
#define BUTTON_4_PIN       29  // GP29 Pin 35

// WS2812 LED Ring (50 LEDs)
#define LED_DATA_PIN       28  // GP28 Pin 34

// Analog Joystick (X/Y only, button used for E-Stop)
#define JOYSTICK_X_PIN     26  // GP26 (ADC0) Pin 31
#define JOYSTICK_Y_PIN     27  // GP27 (ADC1) Pin 32

// SD Card (SPI, shares bus with display)
#define SD_CS_PIN          22  // GP22 Pin 29

// NOTE: Magnetometer and touch screen share I2C bus
// This is OK because they have different I2C addresses:
// - FT6206 touch: 0x38
// - QMC5883L magnetometer: 0x0D
// Both can coexist on the same I2C bus

// ============================================================================
// MOTOR DRIVER CONFIGURATION
// ============================================================================

// Select your motor driver type (uncomment ONE)
#define MOTOR_DRIVER_TB6612FNG

#ifdef MOTOR_DRIVER_L298N
  #define MOTOR_USE_ENABLE_PINS true
  #define MOTOR_ENABLE_ACTIVE_HIGH true
  #define MOTOR_MIN_PWM 50
  #define MOTOR_BRAKE_MODE false
#endif

#ifdef MOTOR_DRIVER_TB6612FNG
  #define MOTOR_USE_ENABLE_PINS true
  #define MOTOR_ENABLE_ACTIVE_HIGH true
  #define MOTOR_MIN_PWM 0
  #define MOTOR_BRAKE_MODE true
#endif

#ifdef MOTOR_DRIVER_DRV8833
  #define MOTOR_USE_ENABLE_PINS false
  #define MOTOR_MIN_PWM 0
  #define MOTOR_BRAKE_MODE true
#endif

#ifdef MOTOR_DRIVER_GENERIC
  #define MOTOR_USE_ENABLE_PINS false
  #define MOTOR_MIN_PWM 0
  #define MOTOR_BRAKE_MODE false
#endif

// ============================================================================
// SYSTEM CONFIGURATION
// ============================================================================

// WiFi credentials are now configured via display setup screen
// No hardcoded credentials needed!

// Mechanical Configuration
#define GEAR_RATIO 75.0
#define ENCODER_PPR 1
#define DEGREES_PER_PULSE (360.0 / (GEAR_RATIO * ENCODER_PPR * 4))

// PID Parameters
#define KP 3.0
#define KI 0.15
#define KD 0.8
#define MAX_ERROR_INTEGRAL 50.0
#define POSITION_TOLERANCE 0.3

// Timing
#define CONTROL_LOOP_HZ 100.0
#define CONTROL_LOOP_DT (1.0 / CONTROL_LOOP_HZ)
#define TRACKING_UPDATE_MS ((unsigned long)(CONTROL_LOOP_DT * 1000))
#define DISPLAY_UPDATE_MS 500

// Safety Limits (with 5 degree margin for detection)
#define MAX_ELEVATION 90.0
#define MIN_ELEVATION 0.0

// Display Settings
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define STATUS_BAR_HEIGHT 30
#define BUTTON_HEIGHT 50
#define BUTTON_MARGIN 10

// PWM Configuration
#define PWM_FREQUENCY 20000
#define PWM_RESOLUTION 8

// GPS Configuration
#define GPS_TIMEOUT_MS 10000  // 10 seconds without valid fix

#endif // CONFIG_H
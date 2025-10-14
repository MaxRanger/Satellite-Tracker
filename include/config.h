/*
 * config.h - Configuration and Pin Definitions
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// Motor Control Pins
#define MOTOR_E_PWM_FWD    2
#define MOTOR_E_PWM_REV    3
#define MOTOR_A_PWM_FWD    4
#define MOTOR_A_PWM_REV    5

// Motor Driver Enable Pins (optional, depends on driver type)
#define MOTOR_E_ENABLE     12
#define MOTOR_A_ENABLE     13

// Encoder Pins (must be consecutive for PIO)
#define ENCODER_E_BASE     6   // E_A=6, E_B=7
#define ENCODER_A_BASE     8   // A_A=8, A_B=9

// Index Sensor Pins
#define INDEX_E            10
#define INDEX_A            11

// GPS Serial (UART1)
#define GPS_RX             17
#define GPS_TX             16

// Adafruit 2.8" TFT Display (PID 2423) - ILI9341 with FT6206 Touch
#define TFT_MOSI           19  // GP19 Pin 25 (SPI TX)
#define TFT_MISO           16  // GP16 Pin 21 (SPI RX)
#define TFT_SCK            18  // GP18 Pin 24 (SPI SCK)
#define TFT_CS             17  // GP17 Pin 22 (Chip Select)
#define TFT_DC             14  // GP14 (Data/Command) - MOVED from GP20 to avoid conflict

// FT6206 Capacitive Touch (I2C)
#define TOUCH_SDA          4   // GP4 Pin 6 (I2C SDA)
#define TOUCH_SCL          5   // GP5 Pin 7 (I2C SCL)

// I2C for QMC5883L Magnetometer (shares bus with touch - different I2C addresses)
#define I2C_SDA            4   // GP4 Pin 6 (Shared with TOUCH_SDA)
#define I2C_SCL            5   // GP5 Pin 7 (Shared with TOUCH_SCL)

// NOTE: Magnetometer and touch screen share I2C bus
// This is OK because they have different I2C addresses:
// - FT6206 touch: 0x38
// - QMC5883L magnetometer: 0x0D
// Both can coexist on the same I2C bus

// ============================================================================
// MOTOR DRIVER CONFIGURATION
// ============================================================================

// Select your motor driver type (uncomment ONE)
#define MOTOR_DRIVER_L298N
// #define MOTOR_DRIVER_TB6612FNG
// #define MOTOR_DRIVER_DRV8833
// #define MOTOR_DRIVER_GENERIC

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

// Safety Limits
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

#endif // CONFIG_H
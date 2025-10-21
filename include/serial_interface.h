/*
 * serial_interface.h - Serial command interface
 * Comprehensive CLI for system configuration, calibration, and monitoring
 */

#ifndef SERIAL_INTERFACE_H
#define SERIAL_INTERFACE_H

#include <Arduino.h>
#include "config.h"
#include "shared_data.h"
#include "gps_module.h"
#include "compass_module.h"
#include "joystick_module.h"
#include "motor_control.h"
#include "storage_module.h"
#include "web_interface.h"

// Command buffer size
#define SERIAL_BUFFER_SIZE 128

// Command parsing
struct SerialCommand {
  char command[32];
  char args[96];
};

// ============================================================================
// PUBLIC API
// ============================================================================

// Initialize serial interface
void initSerialInterface();

// Process incoming serial data (call from main loop)
void updateSerialInterface();

// Print system banner/help
void printBanner();
void printHelp();

// Status functions
void printSystemStatus();
void printGPSStatus();
void printCompassStatus();
void printJoystickStatus();
void printMotorStatus();
void printWiFiStatus();
void printStorageStatus();

// Configuration functions
void setWiFiCredentials(const char* ssid, const char* password);
void saveConfiguration();
void loadConfiguration();
void eraseConfiguration();

// Calibration functions
void beginCompassCalibration();
void endCompassCalibration();
void beginJoystickCalibration();
void endJoystickCalibration();

// Control functions
void beginHomeAxes();
void endTracking();
void setManualPosition(float az, float el);
void beginEmergencyStop();
void beginResetEmergencyStop();

// TLE management
void setTLE(const char* name, const char* line1, const char* line2);
void printTLE();

// Diagnostic functions
void printRawCompassData(int samples);
void printRawJoystickData(int samples);
void printEncoderCounts();
void streamGPSData(unsigned long duration);

#endif // SERIAL_INTERFACE_H
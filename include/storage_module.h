/*
 * storage_module.h - Non-volatile storage interface
 * Supports W25Q SPI flash and SD cards
 */

#ifndef STORAGE_MODULE_H
#define STORAGE_MODULE_H

#include <Arduino.h>
#include "config.h"

// Storage types
typedef enum {
  STORAGE_TYPE_NONE = 0,
  STORAGE_TYPE_W25Q_FLASH,
  STORAGE_TYPE_SD_CARD
} StorageType;

// Configuration structure for persistent storage
struct StorageConfig {
  // WiFi credentials
  char wifiSSID[32];
  char wifiPassword[64];
  bool wifiConfigured;
  
  // Compass calibration
  int compassMinX, compassMaxX;
  int compassMinY, compassMaxY;
  int compassMinZ, compassMaxZ;
  int compassDeadband;
  bool compassCalibrated;
  
  // Joystick calibration
  uint16_t joyXMin, joyXCenter, joyXMax;
  uint16_t joyYMin, joyYCenter, joyYMax;
  uint16_t joyDeadband;
  bool joyCalibrated;
  
  // TLE data
  char satelliteName[25];
  char tleLine1[70];
  char tleLine2[70];
  bool tleValid;
  
  // Magic number and version for validation
  uint32_t magic;      // 0xCAFEBABE
  uint16_t version;    // Config structure version
  uint16_t checksum;   // Simple checksum for validation
};

// ============================================================================
// PUBLIC API
// ============================================================================

// Initialize storage (auto-detects type)
bool initStorage();

// Get detected storage type
StorageType getStorageType();

// Check if storage is available
bool isStorageAvailable();

// Load configuration from storage
bool loadConfig(StorageConfig* config);

// Save configuration to storage
bool saveConfig(const StorageConfig* config);

// Erase all stored configuration
bool eraseConfig();

// Format storage (caution: erases everything)
bool formatStorage();

// Get storage info
void printStorageInfo();

// Convenience functions for specific config items
bool saveWiFiCredentials(const char* ssid, const char* password);
bool loadWiFiCredentials(char* ssid, char* password);

bool saveCompassCalibration(int minX, int maxX, int minY, int maxY, int minZ, int maxZ);
bool loadCompassCalibration(int* minX, int* maxX, int* minY, int* maxY, int* minZ, int* maxZ);

bool saveJoystickCalibration(uint16_t xMin, uint16_t xCenter, uint16_t xMax,
                             uint16_t yMin, uint16_t yCenter, uint16_t yMax, uint16_t deadband);
bool loadJoystickCalibration(uint16_t* xMin, uint16_t* xCenter, uint16_t* xMax,
                             uint16_t* yMin, uint16_t* yCenter, uint16_t* yMax, uint16_t* deadband);

bool saveTLE(const char* name, const char* line1, const char* line2);
bool loadTLE(char* name, char* line1, char* line2);

// Print storage status to Serial console (for debugging)
void printStorageStatus();

#endif // STORAGE_MODULE_H
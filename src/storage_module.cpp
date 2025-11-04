// ============================================================================
// storage_module.cpp
// ============================================================================

#include "storage_module.h"
#include <SPI.h>
#include <LittleFS.h>
#include <SD.h>

// Storage state
static StorageType currentStorageType = STORAGE_TYPE_NONE;
static bool storageInitialized = false;

// Magic number for config validation
#define CONFIG_MAGIC 0xCAFEBABE
#define CONFIG_VERSION 1
#define CONFIG_FILENAME "/tracker_config.dat"

// ============================================================================
// INTERNAL FUNCTIONS
// ============================================================================

// Calculate simple checksum
static uint16_t calculateChecksum(const StorageConfig* config) {
  uint16_t checksum = 0;
  const uint8_t* data = (const uint8_t*)config;
  size_t len = sizeof(StorageConfig) - sizeof(uint16_t); // Exclude checksum field
  
  for (size_t i = 0; i < len; i++) {
    checksum += data[i];
  }
  
  return checksum;
}

// Validate config structure
static bool validateConfig(const StorageConfig* config) {
  if (config->magic != CONFIG_MAGIC) {
    Serial.println("Config validation failed: bad magic number");
    return false;
  }
  
  if (config->version != CONFIG_VERSION) {
    Serial.println("Config validation failed: version mismatch");
    return false;
  }
  
  uint16_t calcChecksum = calculateChecksum(config);
  if (config->checksum != calcChecksum) {
    Serial.println("Config validation failed: checksum mismatch");
    return false;
  }
  
  return true;
}

// Try to initialize W25Q flash with LittleFS
static bool initW25QFlash() {
  Serial.println("Attempting to mount W25Q flash...");
  
  // LittleFS on external SPI flash
  if (LittleFS.begin()) {
    Serial.println("LittleFS mounted successfully");
    
    // Print filesystem info
    FSInfo info;
    LittleFS.info(info);
    Serial.printf("  Total: %lu bytes\n", info.totalBytes);
    Serial.printf("  Used: %lu bytes\n", info.usedBytes);
    
    return true;
  }
  
  Serial.println("LittleFS mount failed, attempting format...");
  
  // Try formatting
  if (LittleFS.format()) {
    Serial.println("Format successful, mounting...");
    if (LittleFS.begin()) {
      Serial.println("LittleFS mounted after format");
      return true;
    }
  }
  
  Serial.println("W25Q flash initialization failed");
  return false;
}

// Try to initialize SD card
static bool initSDCard() {
  Serial.println("Attempting to mount SD card...");
  
  if (SD.begin(SD_CS_PIN)) {
    Serial.println("SD card mounted successfully");
    
    // Print card info
    uint64_t cardSize = SD.size64() / (1024 * 1024);
    Serial.printf("  Size: %llu MB\n", cardSize);
    Serial.printf("  Type: ");
    
    switch (SD.type()) {
      case SD_CARD_TYPE_SD1:
        Serial.println("SD1");
        break;
      case SD_CARD_TYPE_SD2:
        Serial.println("SD2");
        break;
      case SD_CARD_TYPE_SDHC:
        Serial.println("SDHC");
        break;
      default:
        Serial.println("Unknown");
    }
    
    return true;
  }
  
  Serial.println("SD card initialization failed");
  return false;
}

// ============================================================================
// PUBLIC API IMPLEMENTATION
// ============================================================================

bool initStorage() {
  Serial.println("Initializing storage...");
  
  // Try W25Q flash first (preferred for reliability)
  if (initW25QFlash()) {
    currentStorageType = STORAGE_TYPE_W25Q_FLASH;
    storageInitialized = true;
    Serial.println("Using W25Q SPI flash storage");
    return true;
  }
  
  // Fall back to SD card
  if (initSDCard()) {
    currentStorageType = STORAGE_TYPE_SD_CARD;
    storageInitialized = true;
    Serial.println("Using SD card storage");
    return true;
  }
  
  // No storage available
  currentStorageType = STORAGE_TYPE_NONE;
  storageInitialized = false;
  Serial.println("WARNING: No storage available - configuration will not persist");
  return false;
}

StorageType getStorageType() {
  return currentStorageType;
}

bool isStorageAvailable() {
  return storageInitialized;
}

bool loadConfig(StorageConfig* config) {
  if (!storageInitialized) {
    Serial.println("Storage not initialized");
    return false;
  }
  
  File file;
  
  // Open file based on storage type
  if (currentStorageType == STORAGE_TYPE_W25Q_FLASH) {
    file = LittleFS.open(CONFIG_FILENAME, "r");
  } else if (currentStorageType == STORAGE_TYPE_SD_CARD) {
    file = SD.open(CONFIG_FILENAME, FILE_READ);
  }
  
  if (!file) {
    Serial.println("Config file not found");
    return false;
  }
  
  // Read config structure
  size_t bytesRead = file.read((uint8_t*)config, sizeof(StorageConfig));
  file.close();
  
  if (bytesRead != sizeof(StorageConfig)) {
    Serial.println("Config file read error");
    return false;
  }
  
  // Validate config
  if (!validateConfig(config)) {
    return false;
  }
  
  Serial.println("Configuration loaded successfully");
  return true;
}

bool saveConfig(const StorageConfig* config) {
  if (!storageInitialized) {
    Serial.println("Storage not initialized");
    return false;
  }
  
  // Create a copy to add magic/version/checksum
  StorageConfig configCopy = *config;
  configCopy.magic = CONFIG_MAGIC;
  configCopy.version = CONFIG_VERSION;
  configCopy.checksum = calculateChecksum(&configCopy);
  
  File file;
  
  // Open file based on storage type
  if (currentStorageType == STORAGE_TYPE_W25Q_FLASH) {
    file = LittleFS.open(CONFIG_FILENAME, "w");
  } else if (currentStorageType == STORAGE_TYPE_SD_CARD) {
    file = SD.open(CONFIG_FILENAME, FILE_WRITE);
  }
  
  if (!file) {
    Serial.println("Failed to open config file for writing");
    return false;
  }
  
  // Write config structure
  size_t bytesWritten = file.write((const uint8_t*)&configCopy, sizeof(StorageConfig));
  file.close();
  
  if (bytesWritten != sizeof(StorageConfig)) {
    Serial.println("Config file write error");
    return false;
  }
  
  Serial.println("Configuration saved successfully");
  return true;
}

bool eraseConfig() {
  if (!storageInitialized) {
    return false;
  }
  
  bool result = false;
  
  if (currentStorageType == STORAGE_TYPE_W25Q_FLASH) {
    result = LittleFS.remove(CONFIG_FILENAME);
  } else if (currentStorageType == STORAGE_TYPE_SD_CARD) {
    result = SD.remove(CONFIG_FILENAME);
  }
  
  if (result) {
    Serial.println("Configuration erased");
  } else {
    Serial.println("Failed to erase configuration");
  }
  
  return result;
}

bool formatStorage() {
  if (!storageInitialized) {
    return false;
  }
  
  Serial.println("WARNING: Formatting storage - all data will be lost!");
  
  bool result = false;
  
  if (currentStorageType == STORAGE_TYPE_W25Q_FLASH) {
    LittleFS.end();
    result = LittleFS.format();
    if (result) {
      LittleFS.begin();
    }
  } else if (currentStorageType == STORAGE_TYPE_SD_CARD) {
    // SD card format not supported, just delete config file
    result = SD.remove(CONFIG_FILENAME);
  }
  
  if (result) {
    Serial.println("Storage formatted");
  } else {
    Serial.println("Format failed");
  }
  
  return result;
}

void printStorageInfo() {
  Serial.println("\n=== Storage Information ===");
  
  switch (currentStorageType) {
    case STORAGE_TYPE_W25Q_FLASH:
      Serial.println("Type: W25Q SPI Flash");
      if (storageInitialized) {
        FSInfo info;
        LittleFS.info(info);
        Serial.printf("Total: %lu bytes (%.2f KB)\n", info.totalBytes, info.totalBytes / 1024.0);
        Serial.printf("Used: %lu bytes (%.2f KB)\n", info.usedBytes, info.usedBytes / 1024.0);
        Serial.printf("Free: %lu bytes (%.2f KB)\n", 
                      info.totalBytes - info.usedBytes, 
                      (info.totalBytes - info.usedBytes) / 1024.0);
      }
      break;
      
    case STORAGE_TYPE_SD_CARD:
      Serial.println("Type: SD Card");
      if (storageInitialized) {
        uint64_t cardSize = SD.size64();
        Serial.printf("Size: %.2f MB\n", cardSize / (1024.0 * 1024.0));
      }
      break;
      
    case STORAGE_TYPE_NONE:
      Serial.println("Type: None (no storage available)");
      break;
  }
  
  Serial.println();
}

// ============================================================================
// CONVENIENCE FUNCTIONS
// ============================================================================

bool saveWiFiCredentials(const char* ssid, const char* password) {
  StorageConfig config = {0};
  
  // Try to load existing config
  loadConfig(&config);
  
  // Update WiFi credentials
  strncpy(config.wifiSSID, ssid, sizeof(config.wifiSSID) - 1);
  config.wifiSSID[sizeof(config.wifiSSID) - 1] = '\0';
  
  strncpy(config.wifiPassword, password, sizeof(config.wifiPassword) - 1);
  config.wifiPassword[sizeof(config.wifiPassword) - 1] = '\0';
  
  config.wifiConfigured = true;
  
  return saveConfig(&config);
}

bool loadWiFiCredentials(char* ssid, char* password) {
  StorageConfig config = {0};
  
  if (!loadConfig(&config)) {
    return false;
  }
  
  if (!config.wifiConfigured) {
    return false;
  }
  
  strcpy(ssid, config.wifiSSID);
  strcpy(password, config.wifiPassword);
  
  return true;
}

bool saveCompassCalibration(int minX, int maxX, int minY, int maxY, int minZ, int maxZ) {
  StorageConfig config = {0};
  loadConfig(&config);
  
  config.compassMinX = minX;
  config.compassMaxX = maxX;
  config.compassMinY = minY;
  config.compassMaxY = maxY;
  config.compassMinZ = minZ;
  config.compassMaxZ = maxZ;
  config.compassCalibrated = true;
  
  return saveConfig(&config);
}

bool loadCompassCalibration(int* minX, int* maxX, int* minY, int* maxY, int* minZ, int* maxZ) {
  StorageConfig config = {0};
  
  if (!loadConfig(&config)) {
    return false;
  }
  
  if (!config.compassCalibrated) {
    return false;
  }
  
  *minX = config.compassMinX;
  *maxX = config.compassMaxX;
  *minY = config.compassMinY;
  *maxY = config.compassMaxY;
  *minZ = config.compassMinZ;
  *maxZ = config.compassMaxZ;
  
  return true;
}

bool saveJoystickCalibration(uint16_t xMin, uint16_t xCenter, uint16_t xMax,
                             uint16_t yMin, uint16_t yCenter, uint16_t yMax, uint16_t deadband) {
  StorageConfig config = {0};
  loadConfig(&config);
  
  config.joyXMin = xMin;
  config.joyXCenter = xCenter;
  config.joyXMax = xMax;
  config.joyYMin = yMin;
  config.joyYCenter = yCenter;
  config.joyYMax = yMax;
  config.joyDeadband = deadband;
  config.joyCalibrated = true;
  
  return saveConfig(&config);
}

bool loadJoystickCalibration(uint16_t* xMin, uint16_t* xCenter, uint16_t* xMax,
                             uint16_t* yMin, uint16_t* yCenter, uint16_t* yMax, uint16_t* deadband) {
  StorageConfig config = {0};
  
  if (!loadConfig(&config)) {
    return false;
  }
  
  if (!config.joyCalibrated) {
    return false;
  }
  
  *xMin = config.joyXMin;
  *xCenter = config.joyXCenter;
  *xMax = config.joyXMax;
  *yMin = config.joyYMin;
  *yCenter = config.joyYCenter;
  *yMax = config.joyYMax;
  *deadband = config.joyDeadband;
  
  return true;
}

bool saveTLE(const char* name, const char* line1, const char* line2) {
  StorageConfig config = {0};
  loadConfig(&config);
  
  strncpy(config.satelliteName, name, sizeof(config.satelliteName) - 1);
  config.satelliteName[sizeof(config.satelliteName) - 1] = '\0';
  
  strncpy(config.tleLine1, line1, sizeof(config.tleLine1) - 1);
  config.tleLine1[sizeof(config.tleLine1) - 1] = '\0';
  
  strncpy(config.tleLine2, line2, sizeof(config.tleLine2) - 1);
  config.tleLine2[sizeof(config.tleLine2) - 1] = '\0';
  
  config.tleValid = true;
  
  return saveConfig(&config);
}

bool loadTLE(char* name, char* line1, char* line2) {
  StorageConfig config = {0};
  
  if (!loadConfig(&config)) {
    return false;
  }
  
  if (!config.tleValid) {
    return false;
  }
  
  strcpy(name, config.satelliteName);
  strcpy(line1, config.tleLine1);
  strcpy(line2, config.tleLine2);
  
  return true;
}

void printStorageStatus() {
  Serial.println(F("\n=== STORAGE STATUS ==="));
  Serial.println();
  
  if (!isStorageAvailable()) {
    Serial.println(F("No storage available"));
    Serial.println(F("Configuration will not persist across reboots"));
    Serial.println();
    return;
  }
  
  StorageType type = getStorageType();
  Serial.print(F("Type:          "));
  
  switch (type) {
    case STORAGE_TYPE_W25Q_FLASH:
      Serial.println(F("W25Q SPI Flash"));
      break;
    case STORAGE_TYPE_SD_CARD:
      Serial.println(F("SD Card"));
      break;
    default:
      Serial.println(F("Unknown"));
      break;
  }
  
  printStorageInfo();
}
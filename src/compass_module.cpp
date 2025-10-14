// ============================================================================
// compass_module.cpp
// ============================================================================

#include "compass_module.h"

QMC5883LCompass compass;

// Calibration data - updated by calibrateCompass()
float magOffset[3] = {0, 0, 0};
float magScale[3] = {1, 1, 1};

QMC5883LCompass& getCompass() {
  return compass;
}

void initCompass() {
  Serial.println("Initializing compass...");
  
  // I2C already initialized by display module (shared bus)
  // Just init the compass sensor
  
  compass.init();
  
  // Default calibration (update with actual values)
  // These should be determined by running calibrateCompass()
  compass.setCalibration(-1642, 1694, -2084, 1567, -2073, 1556);
  
  Serial.println("Compass initialized (shared I2C bus with touch)");
  Serial.println("Run calibrateCompass() for accurate readings");
}

void setCompassCalibration(int minX, int maxX, int minY, int maxY, int minZ, int maxZ) {
  compass.setCalibration(minX, maxX, minY, maxY, minZ, maxZ);
  
  // Calculate and store offsets and scale factors
  magOffset[0] = (maxX + minX) / 2.0;
  magOffset[1] = (maxY + minY) / 2.0;
  magOffset[2] = (maxZ + minZ) / 2.0;
  
  float rangeX = maxX - minX;
  float rangeY = maxY - minY;
  float rangeZ = maxZ - minZ;
  float avgRange = (rangeX + rangeY + rangeZ) / 3.0;
  
  // Normalize to average range to handle non-uniform magnetic field
  magScale[0] = avgRange / rangeX;
  magScale[1] = avgRange / rangeY;
  magScale[2] = avgRange / rangeZ;
  
  Serial.println("Compass calibration updated");
  Serial.printf("Offsets: X=%.1f Y=%.1f Z=%.1f\n", 
                magOffset[0], magOffset[1], magOffset[2]);
  Serial.printf("Scales: X=%.3f Y=%.3f Z=%.3f\n", 
                magScale[0], magScale[1], magScale[2]);
}

float readCompassHeading() {
  compass.read();
  
  // Get raw values
  float x = compass.getX();
  float y = compass.getY();
  float z = compass.getZ();
  
  // Apply hard iron correction (offset)
  float xCorrected = x - magOffset[0];
  float yCorrected = y - magOffset[1];
  float zCorrected = z - magOffset[2];
  
  // Apply soft iron correction (scale)
  xCorrected *= magScale[0];
  yCorrected *= magScale[1];
  zCorrected *= magScale[2];
  
  // Calculate heading (assumes level mounting)
  // For non-level mounting, would need tilt compensation
  float heading = atan2(yCorrected, xCorrected) * 180.0 / PI;
  
  // Normalize to 0-360
  if (heading < 0) heading += 360.0;
  
  return heading;
}

void calibrateCompass() {
  Serial.println("\n=== COMPASS CALIBRATION ===");
  Serial.println("Rotate the device through all orientations");
  Serial.println("Move slowly and smoothly for 30-60 seconds");
  Serial.println("Send any character via Serial Monitor to finish");
  Serial.println("Starting in 3 seconds...\n");
  
  delay(3000);
  
  int minX = 32767, maxX = -32768;
  int minY = 32767, maxY = -32768;
  int minZ = 32767, maxZ = -32768;
  
  unsigned long startTime = millis();
  unsigned long lastPrint = 0;
  const unsigned long CALIBRATION_TIMEOUT = 120000; // 2 minute max
  const unsigned long MIN_CALIBRATION_TIME = 15000; // 15 second minimum
  
  Serial.println("Calibrating... (showing ranges every 0.5s)");
  
  while (!Serial.available() && (millis() - startTime) < CALIBRATION_TIMEOUT) {
    compass.read();
    
    int x = compass.getX();
    int y = compass.getY();
    int z = compass.getZ();
    
    // Update min/max values
    minX = min(minX, x);
    maxX = max(maxX, x);
    minY = min(minY, y);
    maxY = max(maxY, y);
    minZ = min(minZ, z);
    maxZ = max(maxZ, z);
    
    // Print progress every 500ms
    if (millis() - lastPrint >= 500) {
      int rangeX = maxX - minX;
      int rangeY = maxY - minY;
      int rangeZ = maxZ - minZ;
      
      Serial.printf("X:[%5d,%5d]=%4d  Y:[%5d,%5d]=%4d  Z:[%5d,%5d]=%4d\n",
                    minX, maxX, rangeX,
                    minY, maxY, rangeY,
                    minZ, maxZ, rangeZ);
      
      lastPrint = millis();
    }
    
    delay(50);  // 20 Hz sample rate
  }
  
  // Clear serial buffer
  while (Serial.available()) Serial.read();
  
  unsigned long calibrationDuration = millis() - startTime;
  
  // Check if calibration was long enough
  if (calibrationDuration < MIN_CALIBRATION_TIME) {
    Serial.println("\nWARNING: Calibration too short!");
    Serial.println("For best results, calibrate for at least 15 seconds");
  }
  
  // Validate calibration ranges
  int rangeX = maxX - minX;
  int rangeY = maxY - minY;
  int rangeZ = maxZ - minZ;
  
  if (rangeX < 100 || rangeY < 100 || rangeZ < 100) {
    Serial.println("\nWARNING: Insufficient rotation detected!");
    Serial.println("Calibration may be inaccurate.");
    Serial.println("Rotate through ALL orientations for better results.");
  }
  
  // Apply calibration
  setCompassCalibration(minX, maxX, minY, maxY, minZ, maxZ);
  
  Serial.println("\n=== Calibration Complete ===");
  Serial.printf("Duration: %lu seconds\n", calibrationDuration / 1000);
  Serial.println("\nCalibration Values:");
  Serial.printf("X: [%d, %d] range=%d\n", minX, maxX, rangeX);
  Serial.printf("Y: [%d, %d] range=%d\n", minY, maxY, rangeY);
  Serial.printf("Z: [%d, %d] range=%d\n", minZ, maxZ, rangeZ);
  Serial.println("\nAdd these to initCompass() for permanent calibration:");
  Serial.printf("compass.setCalibration(%d, %d, %d, %d, %d, %d);\n",
                minX, maxX, minY, maxY, minZ, maxZ);
  Serial.println();
}
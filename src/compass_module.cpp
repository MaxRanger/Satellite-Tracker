// ============================================================================
// compass_module.cpp
// ============================================================================

#include "compass_module.h"

QMC5883LCompass compass;
float magCalibration[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
float magOffset[3] = {0, 0, 0};

QMC5883LCompass& getCompass() {
  return compass;
}

void initCompass() {
  Serial.println("Initializing compass...");
  
  // I2C already initialized by display module (shared bus)
  // Just init the compass sensor
  
  compass.init();
  
  // Default calibration (update with actual values)
  compass.setCalibration(-1642, 1694, -2084, 1567, -2073, 1556);
  
  Serial.println("Compass initialized (shared I2C bus with touch)");
}

void setCompassCalibration(int minX, int maxX, int minY, int maxY, int minZ, int maxZ) {
  compass.setCalibration(minX, maxX, minY, maxY, minZ, maxZ);
  Serial.println("Compass calibration updated");
}

float readCompassHeading() {
  compass.read();
  
  float x = compass.getX();
  float y = compass.getY();
  float z = compass.getZ();
  
  // Apply calibration matrix and offset
  float xCal = magCalibration[0][0] * (x - magOffset[0]) +
               magCalibration[0][1] * (y - magOffset[1]) +
               magCalibration[0][2] * (z - magOffset[2]);
  float yCal = magCalibration[1][0] * (x - magOffset[0]) +
               magCalibration[1][1] * (y - magOffset[1]) +
               magCalibration[1][2] * (z - magOffset[2]);
  
  float heading = atan2(yCal, xCal) * 180.0 / PI;
  if (heading < 0) heading += 360;
  
  return heading;
}

void calibrateCompass() {
  Serial.println("Compass calibration mode - rotate through all orientations");
  Serial.println("Send any character to finish calibration");
  
  int minX = 32767, maxX = -32768;
  int minY = 32767, maxY = -32768;
  int minZ = 32767, maxZ = -32768;
  
  while (!Serial.available()) {
    compass.read();
    
    int x = compass.getX();
    int y = compass.getY();
    int z = compass.getZ();
    
    if (x < minX) minX = x;
    if (x > maxX) maxX = x;
    if (y < minY) minY = y;
    if (y > maxY) maxY = y;
    if (z < minZ) minZ = z;
    if (z > maxZ) maxZ = z;
    
    Serial.print("X: "); Serial.print(x);
    Serial.print(" Y: "); Serial.print(y);
    Serial.print(" Z: "); Serial.println(z);
    
    delay(100);
  }
  
  // Clear serial buffer
  while (Serial.available()) Serial.read();
  
  // Apply calibration
  compass.setCalibration(minX, maxX, minY, maxY, minZ, maxZ);
  
  Serial.println("Calibration complete:");
  Serial.print("X: "); Serial.print(minX); Serial.print(" to "); Serial.println(maxX);
  Serial.print("Y: "); Serial.print(minY); Serial.print(" to "); Serial.println(maxY);
  Serial.print("Z: "); Serial.print(minZ); Serial.print(" to "); Serial.println(maxZ);
}
/*
 * Satellite Tracker Motor Control Debug Program
 * Version: 71 - MAJOR REFACTOR
 * 
 * Target: Raspberry Pi Pico 2W (RP2350)
 * Framework: Arduino (Earle Philhower)
 * 
 * SUMMARY:
 * This program provides complete motor control with PID velocity/position control,
 * encoder feedback, index sensors, and calibration routines for a 2-axis
 * satellite tracker with very high gear ratios.
 * 
 * HARDWARE:
 * - Elevation: 298:1 gear, 8,587 counts/rev, range 0-90°
 * - Azimuth: 1,164:1 gear, 32,558 counts/rev, continuous rotation
 * - Encoders: Adafruit #4641 (7 PPR × 4 quadrature = 28 counts/motor-rev)
 * - Motors: TB6612FNG driver, 6V nominal
 * - Index sensors: A3144 hall-effect at known positions
 * 
 * v71 CHANGES:
 * - Cleaned up command structure (consolidated, better mnemonics)
 * - Fixed version display
 * - Removed debug cruft
 * - Added comprehensive comments
 * - Streamlined calibration commands
 * - Fixed deadband compensation bug
 */

#include <Arduino.h>
#include <Wire.h>

// ============================================================================
// VERSION
// ============================================================================
#define VERSION "71"

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// Motors - TB6612FNG H-bridge driver
#define MOTOR_E_PWM_FWD    6   // Elevation forward
#define MOTOR_E_PWM_REV    7   // Elevation reverse
#define MOTOR_A_PWM_FWD    8   // Azimuth forward
#define MOTOR_A_PWM_REV    9   // Azimuth reverse

// Encoders - quadrature, push-pull output
#define ENCODER_E_A        2
#define ENCODER_E_B        3
#define ENCODER_A_A        10
#define ENCODER_A_B        11

// Index sensors - hall-effect with pullups
#define INDEX_E            12  // Elevation at 90°
#define INDEX_A            13  // Azimuth (one per revolution)

// User input
#define EMERGENCY_STOP_PIN 23  // Joystick button
#define JOYSTICK_X_PIN     26  // ADC0
#define JOYSTICK_Y_PIN     27  // ADC1

// I2C
#define I2C_SDA            4
#define I2C_SCL            5
#define MCP23017_ADDRESS   0x20

// MCP23017 registers
#define MCP23017_IODIRA    0x00
#define MCP23017_GPPUA     0x0C
#define MCP23017_GPIOA     0x12

// Buttons on MCP23017 Bank A
#define MCP_BUTTON_1       0
#define MCP_BUTTON_2       1
#define MCP_BUTTON_3       2
#define MCP_BUTTON_4       3
#define MCP_JOYSTICK_BTN   4

// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================

// PWM
#define PWM_FREQUENCY 20000
#define PWM_RESOLUTION 8
#define PWM_MAX_VALUE 255

// Calibrated encoder counts per output revolution
#define ELEVATION_COUNTS_PER_REV 8587.0
#define AZIMUTH_COUNTS_PER_REV   32558.0

// Motor deadband (minimum PWM to overcome friction at 6V)
#define DEADBAND_PWM 140

// ============================================================================
// PID CONTROL CONFIGURATION
// ============================================================================

#define CONTROL_LOOP_HZ 100
#define CONTROL_LOOP_INTERVAL_MS (1000 / CONTROL_LOOP_HZ)
#define VELOCITY_UPDATE_MS 100
#define MAX_INTEGRAL 100.0
#define POSITION_TOLERANCE 0.5  // degrees

// Control modes
enum ControlMode {
  MODE_IDLE,
  MODE_VELOCITY,
  MODE_POSITION
};

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Emergency stop
volatile bool emergencyStop = false;

// Motor state
bool motorsEnabled = false;
int elevationPWM = 0;
int azimuthPWM = 0;

// Encoder tracking (volatile for ISR)
volatile long encoderE_count = 0;
volatile long encoderA_count = 0;
volatile uint8_t encoderE_lastState = 0;
volatile uint8_t encoderA_lastState = 0;

// Index sensors
volatile bool indexE_detected = false;
volatile bool indexA_detected = false;
volatile long indexE_position = 0;
volatile long indexA_position = 0;

// Velocity calculation
unsigned long lastVelocityUpdate = 0;
long lastEncoderE_count = 0;
long lastEncoderA_count = 0;
float velocityE = 0.0;  // deg/s
float velocityA = 0.0;  // deg/s

// Calibration
float calibratedCountsPerRevE = ELEVATION_COUNTS_PER_REV;
float calibratedCountsPerRevA = AZIMUTH_COUNTS_PER_REV;

// PID control
bool pidEnabled = false;
ControlMode controlModeE = MODE_IDLE;
ControlMode controlModeA = MODE_IDLE;

float targetVelocityE = 0.0;
float targetVelocityA = 0.0;
float targetPositionE = 0.0;
float targetPositionA = 0.0;

float Kp_E = 8.0, Ki_E = 1.0, Kd_E = 0.1;
float Kp_A = 8.0, Ki_A = 1.0, Kd_A = 0.1;

float errorIntegralE = 0.0, lastErrorE = 0.0;
float errorIntegralA = 0.0, lastErrorA = 0.0;

// Performance monitoring
unsigned long controlLoopCount = 0;
unsigned long maxLoopTime = 0;

// ============================================================================
// FUNCTION PROTOTYPES
// ============================================================================

// Setup
void setupPins();
void setupEncoders();
void printStartupInfo();

// Motor control
void setMotorPWM(char axis, int pwmValue);
void stopAllMotors();

// Encoder/velocity
void updateVelocity();
float countsToDegreesE(long counts);
float countsToDegreesA(long counts);

// PID control
void pidControlLoop();
float pidCalculate(float error, float &integral, float &lastErr, float Kp, float Ki, float Kd, float dt);
void setVelocity(char axis, float velocity);
void setPosition(char axis, float position);
void stopPID();

// Calibration
void calibrateElevation();
void calibrateAzimuth();

// Commands
void processSerialCommands();
void showHelp();
void showStatus();
void showPIDStatus();
void liveMonitor();

// Utilities
void scanI2C();
bool mcp23017Write(uint8_t reg, uint8_t value);
uint8_t mcp23017Read(uint8_t reg);
void testButtons();

// ISRs
void encoderE_ISR();
void encoderA_ISR();
void indexE_ISR();
void indexA_ISR();
void emergencyStop_ISR();

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n========================================");
  Serial.println("  SATELLITE TRACKER MOTOR DEBUG");
  Serial.print("  Version: ");
  Serial.println(VERSION);
  Serial.println("  Phase 4: PID Velocity Control");
  Serial.println("========================================\n");
  
  setupPins();
  setupEncoders();
  
  // I2C for MCP23017
  Wire.setSDA(I2C_SDA);
  Wire.setSCL(I2C_SCL);
  Wire.begin();
  
  // Emergency stop interrupt
  attachInterrupt(digitalPinToInterrupt(EMERGENCY_STOP_PIN), emergencyStop_ISR, FALLING);
  
  printStartupInfo();
  
  Serial.println("\nInitialization complete!");
  Serial.println("Type 'help' for commands\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  static unsigned long lastControlUpdate = 0;
  
  // Emergency stop check
  if (emergencyStop) {
    stopAllMotors();
    stopPID();
    Serial.println("\n!!! EMERGENCY STOP !!!");
    Serial.println("Type 'reset' to clear\n");
    while (emergencyStop) {
      if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "reset") {
          emergencyStop = false;
          Serial.println("Emergency stop cleared\n");
        }
      }
      delay(100);
    }
  }
  
  // Velocity update
  if (millis() - lastVelocityUpdate >= VELOCITY_UPDATE_MS) {
    updateVelocity();
    lastVelocityUpdate = millis();
  }
  
  // PID control loop
  if (pidEnabled && (millis() - lastControlUpdate >= CONTROL_LOOP_INTERVAL_MS)) {
    unsigned long start = micros();
    pidControlLoop();
    unsigned long elapsed = micros() - start;
    if (elapsed > maxLoopTime) maxLoopTime = elapsed;
    controlLoopCount++;
    lastControlUpdate = millis();
  }
  
  // Process commands
  if (Serial.available()) {
    processSerialCommands();
  }
  
  delay(1);
}

// ============================================================================
// PIN SETUP
// ============================================================================

void setupPins() {
  // Motor outputs
  pinMode(MOTOR_E_PWM_FWD, OUTPUT);
  pinMode(MOTOR_E_PWM_REV, OUTPUT);
  pinMode(MOTOR_A_PWM_FWD, OUTPUT);
  pinMode(MOTOR_A_PWM_REV, OUTPUT);
  
  digitalWrite(MOTOR_E_PWM_FWD, LOW);
  digitalWrite(MOTOR_E_PWM_REV, LOW);
  digitalWrite(MOTOR_A_PWM_FWD, LOW);
  digitalWrite(MOTOR_A_PWM_REV, LOW);
  
  // Encoders
  pinMode(ENCODER_E_A, INPUT);
  pinMode(ENCODER_E_B, INPUT);
  pinMode(ENCODER_A_A, INPUT);
  pinMode(ENCODER_A_B, INPUT);
  
  // Index sensors with pullups
  pinMode(INDEX_E, INPUT_PULLUP);
  pinMode(INDEX_A, INPUT_PULLUP);
  
  // Emergency stop
  pinMode(EMERGENCY_STOP_PIN, INPUT_PULLUP);
  
  // PWM configuration
  analogWriteFreq(PWM_FREQUENCY);
  analogWriteResolution(PWM_RESOLUTION);
  
  Serial.println("Pins configured");
}

void setupEncoders() {
  // Read initial states
  encoderE_lastState = (digitalRead(ENCODER_E_B) << 1) | digitalRead(ENCODER_E_A);
  encoderA_lastState = (digitalRead(ENCODER_A_B) << 1) | digitalRead(ENCODER_A_A);
  
  // Attach interrupts
  attachInterrupt(digitalPinToInterrupt(ENCODER_E_A), encoderE_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_E_B), encoderE_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_A), encoderA_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_A_B), encoderA_ISR, CHANGE);
  
  attachInterrupt(digitalPinToInterrupt(INDEX_E), indexE_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(INDEX_A), indexA_ISR, FALLING);
  
  Serial.println("Encoders configured (x4 quadrature)");
}

// ============================================================================
// MOTOR CONTROL
// ============================================================================

void setMotorPWM(char axis, int pwm) {
  pwm = constrain(pwm, -255, 255);
  
  if (axis == 'E' || axis == 'e') {
    elevationPWM = pwm;
    if (pwm > 0) {
      analogWrite(MOTOR_E_PWM_FWD, pwm);
      analogWrite(MOTOR_E_PWM_REV, 0);
    } else if (pwm < 0) {
      analogWrite(MOTOR_E_PWM_FWD, 0);
      analogWrite(MOTOR_E_PWM_REV, -pwm);
    } else {
      analogWrite(MOTOR_E_PWM_FWD, 0);
      analogWrite(MOTOR_E_PWM_REV, 0);
    }
  } else if (axis == 'A' || axis == 'a') {
    azimuthPWM = pwm;
    // Azimuth motor reversed
    if (pwm > 0) {
      analogWrite(MOTOR_A_PWM_FWD, 0);
      analogWrite(MOTOR_A_PWM_REV, pwm);
    } else if (pwm < 0) {
      analogWrite(MOTOR_A_PWM_FWD, -pwm);
      analogWrite(MOTOR_A_PWM_REV, 0);
    } else {
      analogWrite(MOTOR_A_PWM_FWD, 0);
      analogWrite(MOTOR_A_PWM_REV, 0);
    }
  }
}

void stopAllMotors() {
  setMotorPWM('E', 0);
  setMotorPWM('A', 0);
  motorsEnabled = false;
}

// ============================================================================
// ENCODER ISRs - Quadrature decoding
// ============================================================================

void encoderE_ISR() {
  uint8_t state = (digitalRead(ENCODER_E_B) << 1) | digitalRead(ENCODER_E_A);
  uint8_t combined = (encoderE_lastState << 2) | state;
  
  // Gray code sequence
  if (combined == 0b0001 || combined == 0b0111 || combined == 0b1110 || combined == 0b1000) {
    encoderE_count++;
  } else if (combined == 0b0010 || combined == 0b1011 || combined == 0b1101 || combined == 0b0100) {
    encoderE_count--;
  }
  
  encoderE_lastState = state;
}

void encoderA_ISR() {
  uint8_t state = (digitalRead(ENCODER_A_B) << 1) | digitalRead(ENCODER_A_A);
  uint8_t combined = (encoderA_lastState << 2) | state;
  
  if (combined == 0b0001 || combined == 0b0111 || combined == 0b1110 || combined == 0b1000) {
    encoderA_count++;
  } else if (combined == 0b0010 || combined == 0b1011 || combined == 0b1101 || combined == 0b0100) {
    encoderA_count--;
  }
  
  encoderA_lastState = state;
}

void indexE_ISR() {
  indexE_detected = true;
  indexE_position = encoderE_count;
}

void indexA_ISR() {
  indexA_detected = true;
  indexA_position = encoderA_count;
}

void emergencyStop_ISR() {
  emergencyStop = true;
}

// ============================================================================
// VELOCITY CALCULATION
// ============================================================================

void updateVelocity() {
  static unsigned long lastTime = 0;
  unsigned long currentTime = millis();
  float dt = (currentTime - lastTime) / 1000.0;
  
  if (dt > 0) {
    noInterrupts();
    long currentE = encoderE_count;
    long currentA = encoderA_count;
    interrupts();
    
    long deltaE = currentE - lastEncoderE_count;
    long deltaA = currentA - lastEncoderA_count;
    
    velocityE = countsToDegreesE(deltaE) / dt;
    velocityA = countsToDegreesA(deltaA) / dt;
    
    lastEncoderE_count = currentE;
    lastEncoderA_count = currentA;
    lastTime = currentTime;
  } else {
    lastTime = currentTime;
  }
}

float countsToDegreesE(long counts) {
  return (float)counts * (360.0 / calibratedCountsPerRevE);
}

float countsToDegreesA(long counts) {
  return (float)counts * (360.0 / calibratedCountsPerRevA);
}

// ============================================================================
// PID CONTROL
// ============================================================================

void pidControlLoop() {
  float dt = CONTROL_LOOP_INTERVAL_MS / 1000.0;
  
  // Elevation axis
  if (controlModeE == MODE_VELOCITY && targetVelocityE != 0.0) {
    float error = targetVelocityE - velocityE;
    float output = pidCalculate(error, errorIntegralE, lastErrorE, Kp_E, Ki_E, Kd_E, dt);
    
    int pwm = constrain((int)output, -255, 255);
    
    // Deadband compensation: add offset to overcome friction
    if (pwm > 0) pwm += DEADBAND_PWM;
    else if (pwm < 0) pwm -= DEADBAND_PWM;
    pwm = constrain(pwm, -255, 255);
    
    setMotorPWM('E', pwm);
    
  } else if (controlModeE == MODE_POSITION) {
    float currentPos = countsToDegreesE(encoderE_count);
    float posError = targetPositionE - currentPos;
    
    if (abs(posError) < POSITION_TOLERANCE) {
      targetVelocityE = 0.0;
      controlModeE = MODE_VELOCITY;
      setMotorPWM('E', 0);
      Serial.print("Elevation reached ");
      Serial.print(currentPos, 1);
      Serial.println("°");
    } else {
      targetVelocityE = constrain(posError * 2.0, -20.0, 20.0);
      
      float error = targetVelocityE - velocityE;
      float output = pidCalculate(error, errorIntegralE, lastErrorE, Kp_E, Ki_E, Kd_E, dt);
      int pwm = constrain((int)output, -255, 255);
      
      if (pwm > 0) pwm += DEADBAND_PWM;
      else if (pwm < 0) pwm -= DEADBAND_PWM;
      pwm = constrain(pwm, -255, 255);
      
      setMotorPWM('E', pwm);
    }
  }
  
  // Azimuth axis
  if (controlModeA == MODE_VELOCITY && targetVelocityA != 0.0) {
    float error = targetVelocityA - velocityA;
    float output = pidCalculate(error, errorIntegralA, lastErrorA, Kp_A, Ki_A, Kd_A, dt);
    
    int pwm = constrain((int)output, -255, 255);
    
    // Deadband compensation
    if (pwm > 0) pwm += DEADBAND_PWM;
    else if (pwm < 0) pwm -= DEADBAND_PWM;
    pwm = constrain(pwm, -255, 255);
    
    setMotorPWM('A', pwm);
    
  } else if (controlModeA == MODE_POSITION) {
    float currentPos = countsToDegreesA(encoderA_count);
    float posError = targetPositionA - currentPos;
    
    if (abs(posError) < POSITION_TOLERANCE) {
      targetVelocityA = 0.0;
      controlModeA = MODE_VELOCITY;
      setMotorPWM('A', 0);
      Serial.print("Azimuth reached ");
      Serial.print(currentPos, 1);
      Serial.println("°");
    } else {
      targetVelocityA = constrain(posError * 2.0, -20.0, 20.0);
      
      float error = targetVelocityA - velocityA;
      float output = pidCalculate(error, errorIntegralA, lastErrorA, Kp_A, Ki_A, Kd_A, dt);
      int pwm = constrain((int)output, -255, 255);
      
      if (pwm > 0) pwm += DEADBAND_PWM;
      else if (pwm < 0) pwm -= DEADBAND_PWM;
      pwm = constrain(pwm, -255, 255);
      
      setMotorPWM('A', pwm);
    }
  }
}

float pidCalculate(float error, float &integral, float &lastErr, float Kp, float Ki, float Kd, float dt) {
  float P = Kp * error;
  
  integral += error * dt;
  integral = constrain(integral, -MAX_INTEGRAL, MAX_INTEGRAL);
  float I = Ki * integral;
  
  float D = (dt > 0) ? Kd * (error - lastErr) / dt : 0;
  lastErr = error;
  
  return P + I + D;
}

void setVelocity(char axis, float velocity) {
  if (axis == 'E' || axis == 'e') {
    targetVelocityE = velocity;
    controlModeE = MODE_VELOCITY;
    errorIntegralE = 0.0;
    lastErrorE = 0.0;
  } else if (axis == 'A' || axis == 'a') {
    targetVelocityA = velocity;
    controlModeA = MODE_VELOCITY;
    errorIntegralA = 0.0;
    lastErrorA = 0.0;
  }
}

void setPosition(char axis, float position) {
  if (axis == 'E' || axis == 'e') {
    targetPositionE = constrain(position, 0.0, 90.0);
    controlModeE = MODE_POSITION;
    errorIntegralE = 0.0;
    lastErrorE = 0.0;
  } else if (axis == 'A' || axis == 'a') {
    targetPositionA = position;
    controlModeA = MODE_POSITION;
    errorIntegralA = 0.0;
    lastErrorA = 0.0;
  }
}

void stopPID() {
  pidEnabled = false;
  controlModeE = MODE_IDLE;
  controlModeA = MODE_IDLE;
  targetVelocityE = 0.0;
  targetVelocityA = 0.0;
  errorIntegralE = 0.0;
  errorIntegralA = 0.0;
  stopAllMotors();
}

// ============================================================================
// CALIBRATION
// ============================================================================

void calibrateElevation() {
  Serial.println("\n=== ELEVATION CALIBRATION (0° to 90°) ===");
  Serial.print("Number of runs (3-10): ");
  while (!Serial.available()) delay(10);
  int runs = Serial.parseInt();
  while (Serial.available()) Serial.read();
  runs = constrain(runs, 1, 10);
  Serial.println(runs);
  
  Serial.print("PWM (30-100): ");
  while (!Serial.available()) delay(10);
  int pwm = Serial.parseInt();
  while (Serial.available()) Serial.read();
  pwm = constrain(pwm, 20, 150);
  Serial.println(pwm);
  
  long measurements[10];
  int count = 0;
  
  Serial.println("\nRun | Counts | Degrees");
  Serial.println("----+--------+---------");
  
  for (int i = 0; i < runs; i++) {
    Serial.print("\nRun ");
    Serial.print(i + 1);
    Serial.println(": Position at 0°, press ENTER");
    
    while (Serial.available()) Serial.read();
    while (!Serial.available()) delay(10);
    while (Serial.available()) Serial.read();
    
    noInterrupts();
    encoderE_count = 0;
    indexE_detected = false;
    interrupts();
    
    motorsEnabled = true;
    setMotorPWM('E', pwm);
    
    unsigned long start = millis();
    while (!indexE_detected && (millis() - start) < 20000) delay(10);
    
    setMotorPWM('E', 0);
    
    if (indexE_detected) {
      long cnts = encoderE_count;
      measurements[count++] = cnts;
      
      Serial.print(" ");
      Serial.print(i + 1);
      Serial.print("  | ");
      Serial.print(cnts);
      Serial.print(" | ");
      Serial.print(countsToDegreesE(cnts), 1);
      Serial.println("°");
    } else {
      Serial.println(" TIMEOUT");
    }
    
    delay(1000);
  }
  
  motorsEnabled = false;
  
  if (count == 0) {
    Serial.println("\nNo valid measurements");
    return;
  }
  
  long sum = 0;
  for (int i = 0; i < count; i++) sum += measurements[i];
  float avg = (float)sum / count;
  float countsPerRev = avg * 4.0;
  
  Serial.println("\n----+--------+---------");
  Serial.print("Average (0-90°): ");
  Serial.println(avg, 1);
  Serial.print("Full revolution: ");
  Serial.println(countsPerRev, 0);
  Serial.print("Theoretical: ");
  Serial.println(ELEVATION_COUNTS_PER_REV, 0);
  
  Serial.print("\nSave? (y/n): ");
  while (!Serial.available()) delay(10);
  char c = Serial.read();
  while (Serial.available()) Serial.read();
  
  if (c == 'y' || c == 'Y') {
    calibratedCountsPerRevE = countsPerRev;
    Serial.println("Saved!");
  } else {
    Serial.println("Not saved");
  }
}

void calibrateAzimuth() {
  Serial.println("\n=== AZIMUTH CALIBRATION (Index Sensor) ===");
  Serial.print("PWM (80-200): ");
  while (!Serial.available()) delay(10);
  int pwm = Serial.parseInt();
  while (Serial.available()) Serial.read();
  pwm = constrain(pwm, 50, 255);
  Serial.println(pwm);
  
  Serial.print("Revolutions (3-10): ");
  while (!Serial.available()) delay(10);
  int revs = Serial.parseInt();
  while (Serial.available()) Serial.read();
  revs = constrain(revs, 1, 20);
  Serial.println(revs);
  
  Serial.println("\nMeasuring...");
  
  pidEnabled = false;
  motorsEnabled = true;
  
  noInterrupts();
  indexA_detected = false;
  interrupts();
  
  setMotorPWM('A', pwm);
  delay(1000);
  
  unsigned long times[20];
  long counts[20];
  
  for (int i = 0; i < revs; i++) {
    while (!indexA_detected) delay(1);
    times[i] = millis();
    noInterrupts();
    counts[i] = encoderA_count;
    indexA_detected = false;
    interrupts();
    
    Serial.print("Index ");
    Serial.println(i);
  }
  
  setMotorPWM('A', 0);
  motorsEnabled = false;
  
  Serial.println("\nRev | Delta Counts");
  Serial.println("----+-------------");
  
  long sum = 0;
  for (int i = 1; i < revs; i++) {
    long delta = counts[i] - counts[i-1];
    sum += delta;
    Serial.print(" ");
    Serial.print(i);
    Serial.print("  | ");
    Serial.println(delta);
  }
  
  float avg = (float)sum / (revs - 1);
  
  Serial.println("----+-------------");
  Serial.print("Average: ");
  Serial.println(avg, 0);
  Serial.print("Theoretical: ");
  Serial.println(AZIMUTH_COUNTS_PER_REV, 0);
  
  Serial.print("\nSave? (y/n): ");
  while (!Serial.available()) delay(10);
  char c = Serial.read();
  while (Serial.available()) Serial.read();
  
  if (c == 'y' || c == 'Y') {
    calibratedCountsPerRevA = avg;
    Serial.println("Saved!");
  } else {
    Serial.println("Not saved");
  }
}

// ============================================================================
// COMMAND PROCESSOR
// ============================================================================

void processSerialCommands() {
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();
  
  if (cmd == "" || cmd == "\r") return;
  
  // Help
  if (cmd == "help" || cmd == "h" || cmd == "?") {
    showHelp();
  }
  // Status
  else if (cmd == "status" || cmd == "st") {
    showStatus();
  }
  // PID status
  else if (cmd == "pid") {
    showPIDStatus();
  }
  // Live monitor
  else if (cmd == "monitor" || cmd == "mon") {
    liveMonitor();
  }
  // PID enable/disable
  else if (cmd == "pid on") {
    pidEnabled = true;
    motorsEnabled = true;
    Serial.println("PID enabled");
  }
  else if (cmd == "pid off") {
    stopPID();
    Serial.println("PID disabled");
  }
  // Stop
  else if (cmd == "stop" || cmd == "x") {
    stopAllMotors();
    stopPID();
    Serial.println("Stopped");
  }
  // Zero encoders
  else if (cmd == "zero" || cmd == "z") {
    noInterrupts();
    encoderE_count = 0;
    encoderA_count = 0;
    lastEncoderE_count = 0;
    lastEncoderA_count = 0;
    indexE_detected = false;
    indexA_detected = false;
    interrupts();
    Serial.println("Encoders zeroed");
  }
  // Calibration
  else if (cmd == "cal elev" || cmd == "cal e") {
    calibrateElevation();
  }
  else if (cmd == "cal azim" || cmd == "cal a") {
    calibrateAzimuth();
  }
  // Test buttons
  else if (cmd == "test buttons" || cmd == "test btn") {
    testButtons();
  }
  // I2C scan
  else if (cmd == "scan i2c" || cmd == "scan") {
    scanI2C();
  }
  // PID gains: "gain 5.0 1.0 0.1"
  else if (cmd.startsWith("gain ")) {
    int idx1 = cmd.indexOf(' ');
    int idx2 = cmd.indexOf(' ', idx1 + 1);
    int idx3 = cmd.indexOf(' ', idx2 + 1);
    
    if (idx1 > 0 && idx2 > 0 && idx3 > 0) {
      Kp_E = Kp_A = cmd.substring(idx1 + 1, idx2).toFloat();
      Ki_E = Ki_A = cmd.substring(idx2 + 1, idx3).toFloat();
      Kd_E = Kd_A = cmd.substring(idx3 + 1).toFloat();
      
      Serial.print("Gains: Kp=");
      Serial.print(Kp_E, 2);
      Serial.print(" Ki=");
      Serial.print(Ki_E, 2);
      Serial.print(" Kd=");
      Serial.println(Kd_E, 2);
    } else {
      Serial.println("Usage: gain <Kp> <Ki> <Kd>");
    }
  }
  // Velocity: "vel e 10.5" or "vel a -20"
  else if (cmd.startsWith("vel ")) {
    char axis = cmd.charAt(4);
    float vel = cmd.substring(6).toFloat();
    setVelocity(axis, vel);
    Serial.print("Set ");
    Serial.print((char)toupper(axis));
    Serial.print(" velocity: ");
    Serial.print(vel, 1);
    Serial.println(" deg/s");
  }
  // Position: "pos e 45" or "pos a 180"
  else if (cmd.startsWith("pos ")) {
    char axis = cmd.charAt(4);
    float pos = cmd.substring(6).toFloat();
    setPosition(axis, pos);
    Serial.print("Moving ");
    Serial.print((char)toupper(axis));
    Serial.print(" to ");
    Serial.print(pos, 1);
    Serial.println("°");
  }
  // Manual PWM: "pwm e 100" or "pwm a -150"
  else if (cmd.startsWith("pwm ")) {
    char axis = cmd.charAt(4);
    int pwm = cmd.substring(6).toInt();
    stopPID();
    motorsEnabled = true;
    setMotorPWM(axis, pwm);
    Serial.print("Manual PWM ");
    Serial.print((char)toupper(axis));
    Serial.print(": ");
    Serial.println(pwm);
  }
  // Read joystick
  else if (cmd == "joy") {
    Serial.print("Joystick X: ");
    Serial.print(analogRead(JOYSTICK_X_PIN));
    Serial.print("  Y: ");
    Serial.println(analogRead(JOYSTICK_Y_PIN));
  }
  // Show current velocity
  else if (cmd == "vel") {
    Serial.print("Velocity E: ");
    Serial.print(velocityE, 2);
    Serial.print(" deg/s  A: ");
    Serial.print(velocityA, 2);
    Serial.println(" deg/s");
  }
  // Show position
  else if (cmd == "pos") {
    Serial.print("Position E: ");
    Serial.print(countsToDegreesE(encoderE_count), 2);
    Serial.print("°  A: ");
    Serial.print(countsToDegreesA(encoderA_count), 2);
    Serial.println("°");
  }
  // Reset emergency stop
  else if (cmd == "reset") {
    emergencyStop = false;
    Serial.println("Emergency stop cleared");
  }
  else {
    Serial.print("Unknown: '");
    Serial.print(cmd);
    Serial.println("' Type 'help'");
  }
}

// ============================================================================
// COMMAND FUNCTIONS
// ============================================================================

void showHelp() {
  Serial.println("\n=== COMMANDS ===");
  Serial.println("\nMotor Control:");
  Serial.println("  pwm <e|a> <value>  - Manual PWM (-255 to 255)");
  Serial.println("  stop, x            - Stop all motors");
  Serial.println("  zero, z            - Zero encoders");
  
  Serial.println("\nPID Control:");
  Serial.println("  pid on             - Enable PID");
  Serial.println("  pid off            - Disable PID");
  Serial.println("  vel <e|a> <speed>  - Set velocity (deg/s)");
  Serial.println("  pos <e|a> <angle>  - Move to position");
  Serial.println("  gain <Kp> <Ki> <Kd> - Set PID gains");
  
  Serial.println("\nStatus:");
  Serial.println("  status, st         - Show system status");
  Serial.println("  pid                - Show PID status");
  Serial.println("  monitor, mon       - Live PID monitor");
  Serial.println("  vel                - Show current velocity");
  Serial.println("  pos                - Show current position");
  
  Serial.println("\nCalibration:");
  Serial.println("  cal elev, cal e    - Calibrate elevation");
  Serial.println("  cal azim, cal a    - Calibrate azimuth");
  
  Serial.println("\nDiagnostics:");
  Serial.println("  scan i2c, scan     - Scan I2C bus");
  Serial.println("  test buttons       - Test MCP23017 buttons");
  Serial.println("  joy                - Read joystick");
  Serial.println("  help, h, ?         - This help");
  Serial.println();
}

void showStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  
  Serial.print("Version: ");
  Serial.println(VERSION);
  
  Serial.print("Motors: ");
  Serial.println(motorsEnabled ? "ENABLED" : "DISABLED");
  
  Serial.print("PID: ");
  Serial.println(pidEnabled ? "ENABLED" : "DISABLED");
  
  Serial.println("\nElevation:");
  Serial.print("  Position: ");
  Serial.print(countsToDegreesE(encoderE_count), 2);
  Serial.println("°");
  Serial.print("  Velocity: ");
  Serial.print(velocityE, 2);
  Serial.println(" deg/s");
  Serial.print("  PWM: ");
  Serial.println(elevationPWM);
  Serial.print("  Counts/rev: ");
  Serial.println(calibratedCountsPerRevE, 0);
  
  Serial.println("\nAzimuth:");
  Serial.print("  Position: ");
  Serial.print(countsToDegreesA(encoderA_count), 2);
  Serial.println("°");
  Serial.print("  Velocity: ");
  Serial.print(velocityA, 2);
  Serial.println(" deg/s");
  Serial.print("  PWM: ");
  Serial.println(azimuthPWM);
  Serial.print("  Counts/rev: ");
  Serial.println(calibratedCountsPerRevA, 0);
  Serial.println();
}

void showPIDStatus() {
  Serial.println("\n=== PID STATUS ===");
  
  Serial.print("Enabled: ");
  Serial.println(pidEnabled ? "YES" : "NO");
  Serial.print("Loop rate: ");
  Serial.print(CONTROL_LOOP_HZ);
  Serial.println(" Hz");
  Serial.print("Loops: ");
  Serial.println(controlLoopCount);
  Serial.print("Max time: ");
  Serial.print(maxLoopTime);
  Serial.println(" µs");
  
  Serial.println("\nElevation:");
  Serial.print("  Mode: ");
  if (controlModeE == MODE_IDLE) Serial.println("IDLE");
  else if (controlModeE == MODE_VELOCITY) Serial.println("VELOCITY");
  else Serial.println("POSITION");
  Serial.print("  Target vel: ");
  Serial.print(targetVelocityE, 2);
  Serial.println(" deg/s");
  if (controlModeE == MODE_POSITION) {
    Serial.print("  Target pos: ");
    Serial.print(targetPositionE, 2);
    Serial.println("°");
  }
  Serial.print("  Gains: Kp=");
  Serial.print(Kp_E, 2);
  Serial.print(" Ki=");
  Serial.print(Ki_E, 2);
  Serial.print(" Kd=");
  Serial.println(Kd_E, 2);
  
  Serial.println("\nAzimuth:");
  Serial.print("  Mode: ");
  if (controlModeA == MODE_IDLE) Serial.println("IDLE");
  else if (controlModeA == MODE_VELOCITY) Serial.println("VELOCITY");
  else Serial.println("POSITION");
  Serial.print("  Target vel: ");
  Serial.print(targetVelocityA, 2);
  Serial.println(" deg/s");
  if (controlModeA == MODE_POSITION) {
    Serial.print("  Target pos: ");
    Serial.print(targetPositionA, 2);
    Serial.println("°");
  }
  Serial.print("  Gains: Kp=");
  Serial.print(Kp_A, 2);
  Serial.print(" Ki=");
  Serial.print(Ki_A, 2);
  Serial.print(" Kd=");
  Serial.println(Kd_A, 2);
  Serial.println();
}

void liveMonitor() {
  Serial.println("\n=== LIVE MONITOR ===");
  Serial.println("Press any key to stop\n");
  Serial.println("Axis | Target | Current | Error | PWM");
  Serial.println("-----+--------+---------+-------+-----");
  
  while (!Serial.available()) {
    if (controlModeE != MODE_IDLE || targetVelocityE != 0) {
      Serial.print("  E  | ");
      Serial.print(targetVelocityE, 1);
      Serial.print(" | ");
      Serial.print(velocityE, 1);
      Serial.print(" | ");
      Serial.print(targetVelocityE - velocityE, 1);
      Serial.print(" | ");
      Serial.println(elevationPWM);
    }
    
    if (controlModeA != MODE_IDLE || targetVelocityA != 0) {
      Serial.print("  A  | ");
      Serial.print(targetVelocityA, 1);
      Serial.print(" | ");
      Serial.print(velocityA, 1);
      Serial.print(" | ");
      Serial.print(targetVelocityA - velocityA, 1);
      Serial.print(" | ");
      Serial.println(azimuthPWM);
    }
    
    delay(200);
  }
  
  while (Serial.available()) Serial.read();
  Serial.println("\nMonitor stopped\n");
}

// ============================================================================
// UTILITIES
// ============================================================================

void scanI2C() {
  Serial.println("\n=== I2C SCAN ===");
  
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("0x");
      if (addr < 16) Serial.print("0");
      Serial.print(addr, HEX);
      
      if (addr == 0x0D) Serial.print(" - QMC5883L Compass");
      else if (addr == 0x20) Serial.print(" - MCP23017 I/O Expander");
      else if (addr == 0x38) Serial.print(" - FT6206 Touch");
      
      Serial.println();
      found++;
    }
  }
  
  if (found == 0) {
    Serial.println("No devices found");
  } else {
    Serial.print("\nFound ");
    Serial.print(found);
    Serial.println(" device(s)");
  }
  Serial.println();
}

bool mcp23017Write(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MCP23017_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return (Wire.endTransmission() == 0);
}

uint8_t mcp23017Read(uint8_t reg) {
  Wire.beginTransmission(MCP23017_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return 0xFF;
  
  Wire.requestFrom(MCP23017_ADDRESS, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

void testButtons() {
  Serial.println("\n=== BUTTON TEST ===");
  
  Wire.beginTransmission(MCP23017_ADDRESS);
  if (Wire.endTransmission() != 0) {
    Serial.println("ERROR: MCP23017 not found at 0x20");
    return;
  }
  
  // Configure as inputs with pullups
  mcp23017Write(MCP23017_IODIRA, 0xFF);
  mcp23017Write(MCP23017_GPPUA, 0x1F);  // Pullups on pins 0-4
  
  Serial.println("Press buttons (any key to exit)\n");
  
  uint8_t last = 0xFF;
  while (!Serial.available()) {
    uint8_t state = mcp23017Read(MCP23017_GPIOA);
    
    if (state != last) {
      Serial.print("BTN1:");
      Serial.print((state & 0x01) ? "1" : "0");
      Serial.print(" BTN2:");
      Serial.print((state & 0x02) ? "1" : "0");
      Serial.print(" BTN3:");
      Serial.print((state & 0x04) ? "1" : "0");
      Serial.print(" BTN4:");
      Serial.print((state & 0x08) ? "1" : "0");
      Serial.print(" JOY:");
      Serial.print((state & 0x10) ? "1" : "0");
      
      if (state != 0x1F) {
        Serial.print("  [");
        if (!(state & 0x01)) Serial.print("1 ");
        if (!(state & 0x02)) Serial.print("2 ");
        if (!(state & 0x04)) Serial.print("3 ");
        if (!(state & 0x08)) Serial.print("4 ");
        if (!(state & 0x10)) Serial.print("J ");
        Serial.print("PRESSED]");
      }
      
      Serial.println();
      last = state;
    }
    delay(50);
  }
  
  while (Serial.available()) Serial.read();
  Serial.println("\nTest stopped\n");
}

void printStartupInfo() {
  Serial.println("CALIBRATION:");
  Serial.print("  Elevation: ");
  Serial.print(calibratedCountsPerRevE, 0);
  Serial.println(" counts/rev");
  Serial.print("  Azimuth: ");
  Serial.print(calibratedCountsPerRevA, 0);
  Serial.println(" counts/rev");
  
  Serial.println("\nPID GAINS:");
  Serial.print("  Kp=");
  Serial.print(Kp_E, 1);
  Serial.print(" Ki=");
  Serial.print(Ki_E, 1);
  Serial.print(" Kd=");
  Serial.println(Kd_E, 1);
  
  Serial.println("\nHARDWARE:");
  Serial.println("  Motors: TB6612FNG @ 6V");
  Serial.println("  PWM: 20kHz, 8-bit");
  Serial.print("  Deadband: ");
  Serial.print(DEADBAND_PWM);
  Serial.println(" PWM");
  Serial.println("  Encoders: x4 quadrature");
  Serial.print("  Control loop: ");
  Serial.print(CONTROL_LOOP_HZ);
  Serial.println(" Hz");
}
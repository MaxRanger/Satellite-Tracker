// ============================================================================
// motor_control.cpp
// ============================================================================

#include "motor_control.h"

PIO pioEncoder = pio0;
uint smElevation;
uint smAzimuth;

void setupPIOEncoders() {
  // Load PIO program
  uint offset = pio_add_program(pioEncoder, &quadrature_encoder_program);
  
  // Configure elevation encoder
  smElevation = 0;
  quadrature_encoder_program_init(pioEncoder, smElevation, offset, ENCODER_E_BASE, 0);
  
  // Configure azimuth encoder
  smAzimuth = 1;
  quadrature_encoder_program_init(pioEncoder, smAzimuth, offset, ENCODER_A_BASE, 0);
}

int32_t readPIOEncoder(uint sm) {
  quadrature_encoder_request_count(pioEncoder, sm);
  while (pio_sm_is_rx_fifo_empty(pioEncoder, sm));
  return quadrature_encoder_fetch_count(pioEncoder, sm);
}

void __not_in_flash_func(indexE_ISR)() {
  pio_sm_exec(pioEncoder, smElevation, pio_encode_set(pio_x, 0));
  motorPos.elevationIndexFound = true;
}

void __not_in_flash_func(indexA_ISR)() {
  pio_sm_exec(pioEncoder, smAzimuth, pio_encode_set(pio_x, 0));
  motorPos.azimuthIndexFound = true;
}

void setMotorEnable(int enablePin, bool enable) {
#if MOTOR_USE_ENABLE_PINS
  #if MOTOR_ENABLE_ACTIVE_HIGH
    digitalWrite(enablePin, enable ? HIGH : LOW);
  #else
    digitalWrite(enablePin, enable ? LOW : HIGH);
  #endif
#endif
}

void setMotorSpeed(int fwdPin, int revPin, int enablePin, int speed) {
  speed = constrain(speed, -255, 255);
  
  if (abs(speed) > 0 && abs(speed) < MOTOR_MIN_PWM) {
    speed = (speed > 0) ? MOTOR_MIN_PWM : -MOTOR_MIN_PWM;
  }
  
  #if MOTOR_USE_ENABLE_PINS
    setMotorEnable(enablePin, true);
  #endif
  
  if (speed > 0) {
    analogWrite(fwdPin, speed);
    analogWrite(revPin, 0);
  } else if (speed < 0) {
    analogWrite(fwdPin, 0);
    analogWrite(revPin, -speed);
  } else {
    #if MOTOR_BRAKE_MODE
      analogWrite(fwdPin, 255);
      analogWrite(revPin, 255);
    #else
      analogWrite(fwdPin, 0);
      analogWrite(revPin, 0);
    #endif
  }
}

void stopAllMotors() {
  setMotorSpeed(MOTOR_E_PWM_FWD, MOTOR_E_PWM_REV, MOTOR_E_ENABLE, 0);
  setMotorSpeed(MOTOR_A_PWM_FWD, MOTOR_A_PWM_REV, MOTOR_A_ENABLE, 0);
  
  #if MOTOR_USE_ENABLE_PINS
    setMotorEnable(MOTOR_E_ENABLE, false);
    setMotorEnable(MOTOR_A_ENABLE, false);
  #endif
}

float pidControl(float error, float &errorIntegral, float &lastError, float dt) {
  errorIntegral += error * dt;
  errorIntegral = constrain(errorIntegral, -MAX_ERROR_INTEGRAL, MAX_ERROR_INTEGRAL);
  
  float errorDerivative = (error - lastError) / dt;
  lastError = error;
  
  float output = KP * error + KI * errorIntegral + KD * errorDerivative;
  return constrain(output, -255, 255);
}

void updateMotorControl() {
  motorPos.elevation = readPIOEncoder(smElevation);
  motorPos.azimuth = readPIOEncoder(smAzimuth);
  
  float currentElevation = motorPos.elevation * DEGREES_PER_PULSE;
  float currentAzimuth = motorPos.azimuth * DEGREES_PER_PULSE;
  
  while (currentAzimuth < 0) currentAzimuth += 360.0;
  while (currentAzimuth >= 360) currentAzimuth -= 360.0;
  
  float targetEl = constrain(targetPos.elevation, MIN_ELEVATION, MAX_ELEVATION);
  float targetAz = targetPos.azimuth;
  
  float errorE = targetEl - currentElevation;
  float errorA = targetAz - currentAzimuth;
  
  if (errorA > 180) errorA -= 360;
  if (errorA < -180) errorA += 360;
  
  float controlE = 0, controlA = 0;
  
  if (abs(errorE) > POSITION_TOLERANCE) {
    controlE = pidControl(errorE, errorIntegralE, lastErrorE, CONTROL_LOOP_DT);
  } else {
    errorIntegralE = 0;
    lastErrorE = 0;
  }
  
  if (abs(errorA) > POSITION_TOLERANCE) {
    controlA = pidControl(errorA, errorIntegralA, lastErrorA, CONTROL_LOOP_DT);
  } else {
    errorIntegralA = 0;
    lastErrorA = 0;
  }
  
  setMotorSpeed(MOTOR_E_PWM_FWD, MOTOR_E_PWM_REV, MOTOR_E_ENABLE, (int)controlE);
  setMotorSpeed(MOTOR_A_PWM_FWD, MOTOR_A_PWM_REV, MOTOR_A_ENABLE, (int)controlA);
}

void homeAxes() {
  Serial.println("Homing axes...");
  trackerState.tracking = false;
  
  motorPos.elevationIndexFound = false;
  setMotorSpeed(MOTOR_E_PWM_FWD, MOTOR_E_PWM_REV, MOTOR_E_ENABLE, -80);
  unsigned long startTime = millis();
  while (!motorPos.elevationIndexFound && (millis() - startTime) < 30000) {
    delay(10);
  }
  setMotorSpeed(MOTOR_E_PWM_FWD, MOTOR_E_PWM_REV, MOTOR_E_ENABLE, 0);
  
  if (motorPos.elevationIndexFound) {
    Serial.println("Elevation homed");
  } else {
    Serial.println("ERROR: Elevation home timeout");
  }
  
  motorPos.azimuthIndexFound = false;
  setMotorSpeed(MOTOR_A_PWM_FWD, MOTOR_A_PWM_REV, MOTOR_A_ENABLE, -80);
  startTime = millis();
  while (!motorPos.azimuthIndexFound && (millis() - startTime) < 30000) {
    delay(10);
  }
  setMotorSpeed(MOTOR_A_PWM_FWD, MOTOR_A_PWM_REV, MOTOR_A_ENABLE, 0);
  
  if (motorPos.azimuthIndexFound) {
    Serial.println("Azimuth homed");
  } else {
    Serial.println("ERROR: Azimuth home timeout");
  }
  
  delay(500);
}

void initMotorControl() {
  Serial.println("Initializing motor control...");
  
  setupPIOEncoders();
  Serial.println("PIO encoders initialized");
  
  pinMode(MOTOR_E_PWM_FWD, OUTPUT);
  pinMode(MOTOR_E_PWM_REV, OUTPUT);
  pinMode(MOTOR_A_PWM_FWD, OUTPUT);
  pinMode(MOTOR_A_PWM_REV, OUTPUT);
  
  #if MOTOR_USE_ENABLE_PINS
    pinMode(MOTOR_E_ENABLE, OUTPUT);
    pinMode(MOTOR_A_ENABLE, OUTPUT);
    setMotorEnable(MOTOR_E_ENABLE, false);
    setMotorEnable(MOTOR_A_ENABLE, false);
  #endif
  
  analogWriteFreq(PWM_FREQUENCY);
  analogWriteResolution(PWM_RESOLUTION);
  
  stopAllMotors();
  
  pinMode(INDEX_E, INPUT_PULLUP);
  pinMode(INDEX_A, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INDEX_E), indexE_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(INDEX_A), indexA_ISR, FALLING);
  
  Serial.println("Motor control initialized");
}
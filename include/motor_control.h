/*
 * motor_control.h - Motor control and PIO encoder management
 */

#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>
#include "config.h"
#include "shared_data.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "Tracker_RPI2350_PIO.h"

// PIO objects
extern PIO pioEncoder;
extern uint smElevation;
extern uint smAzimuth;

// Emergency stop flag
extern volatile bool emergencyStop;

// Initialize motor control system
void initMotorControl();

// Motor control functions
void setMotorSpeed(int fwdPin, int revPin, int enablePin, int speed);
void setMotorEnable(int enablePin, bool enable);
void stopAllMotors();

// Emergency stop functions
void resetEmergencyStop();
bool isEmergencyStop();

// PID control
float pidControl(float error, float &errorIntegral, float &lastError, float dt);
void updateMotorControl();

// Homing
void homeAxes();

// PIO encoder functions
void setupPIOEncoders();
int32_t readPIOEncoder(uint sm);

// Interrupt handlers
void __not_in_flash_func(indexE_ISR)();
void __not_in_flash_func(indexA_ISR)();
void __not_in_flash_func(emergencyStop_ISR)();

#endif // MOTOR_CONTROL_H
#pragma once
#include <stdint.h>

// ------------------------------------------------------------
// Global angle output from sensors.cpp
// Updated every cycle by readSensors()
// Used by logic.cpp for posture classification
// ------------------------------------------------------------
extern float currentAngle;

// ------------------------------------------------------------
// Initialize MPU6050 and I2C
// ------------------------------------------------------------
void initSensors();

// ------------------------------------------------------------
// Read accelerometer, compute filtered pitch angle,
// and update currentAngle
// ------------------------------------------------------------
void readSensors();

// Getting tilt angle
#ifndef SENSORS_H
#define SENSORS_H

float getTiltAngle();

#endif

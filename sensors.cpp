#include "sensors.h"
#include <Wire.h>
#include <Arduino.h>
#include "logic.h"

// ------------------------------------------------------------
// Complementary filter variables
// ------------------------------------------------------------
static float pitch = 0.0f;
static unsigned long lastTime = 0;

// Smoothing factor (0.0–1.0). Higher = smoother but slower response.
static float filteredAngle = 0.0f;
static const float alpha = 0.1;

// ------------------------------------------------------------
// MPU6050 Registers
// ------------------------------------------------------------
#define MPU6050_ADDR 0x68
#define REG_PWR_MGMT_1 0x6B
#define REG_ACCEL_XOUT_H 0x3B
#define REG_GYRO_XOUT_H 0x43

// ------------------------------------------------------------
// Fast math helpers
// ------------------------------------------------------------
static inline float fastAbs(float x) {
  return (x < 0) ? -x : x;
}

static inline float fastSqrt(float x) {
  if (x <= 0) return 0;
  float half = 0.5f * x;
  long i = *(long*)&x;
  i = 0x1fbd1df5 + (i >> 1);
  x = *(float*)&i;
  return x * (1.5f - half * x * x);
}

static inline float fastAtan2(float y, float x) {
  float abs_y = fastAbs(y) + 1e-10f;
  float r, angle;

  if (x >= 0) {
    r = (x - abs_y) / (x + abs_y);
    angle = 0.78539816339f - 0.78539816339f * r;
  } else {
    r = (x + abs_y) / (abs_y - x);
    angle = 2.35619449019f - 0.78539816339f * r;
  }

  return (y < 0) ? -angle : angle;
}

// ------------------------------------------------------------
// I2C helpers
// ------------------------------------------------------------
static void writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

static bool readBytes(uint8_t reg, uint8_t count, uint8_t* dest) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;

  if (Wire.requestFrom((uint8_t)MPU6050_ADDR, count) != count)
    return false;

  for (uint8_t i = 0; i < count; i++)
    dest[i] = Wire.read();

  return true;
}

// ------------------------------------------------------------
// Init MPU6050
// ------------------------------------------------------------
void initSensors() {
  delay(50);

  // Wake up MPU6050
  writeReg(REG_PWR_MGMT_1, 0);
  delay(50);

  lastTime = millis();
}

// ------------------------------------------------------------
// Read accelerometer + gyro + compute filtered pitch
// ------------------------------------------------------------
void readSensors() {
  uint8_t raw[14];
  if (!readBytes(REG_ACCEL_XOUT_H, 14, raw)) return;

  // Raw accelerometer
  int16_t ax = (raw[0] << 8) | raw[1];
  int16_t ay = (raw[2] << 8) | raw[3];
  int16_t az = (raw[4] << 8) | raw[5];

  // Raw gyro
  int16_t gx = (raw[8] << 8) | raw[9];

  // Convert accel to g
  float axg = ax * 0.000061035f;
  float ayg = ay * 0.000061035f;
  float azg = az * 0.000061035f;

  // Accelerometer pitch
  float accelPitch = fastAtan2(-azg, fastSqrt(ayg * ayg + azg * azg)) * 57.2958f;

  // Gyro rate (deg/s)
  float gyroRate = gx / 131.0f;

  // Time step
  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0f;
  lastTime = now;

  // Complementary filter
  pitch = 0.98f * (pitch + gyroRate * dt) + 0.02f * accelPitch;

  // Output for logic + display
  currentAngle = pitch;
}

// ------------------------------------------------------------
// Simple accelerometer-only tilt angle (used by display)
// ------------------------------------------------------------
float getTiltAngle() {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return filteredAngle;

  if (Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)6) != 6)
    return filteredAngle;

  int16_t rawX = (Wire.read() << 8) | Wire.read();
  int16_t rawY = (Wire.read() << 8) | Wire.read();
  int16_t rawZ = (Wire.read() << 8) | Wire.read();

  float ax = rawX / 16384.0;
  float ay = rawY / 16384.0;
  float az = rawZ / 16384.0;

  float angle = atan2(ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
  if (angle < 0) angle = -angle;

  filteredAngle = (alpha * angle) + ((1 - alpha) * filteredAngle);
  return filteredAngle;
}
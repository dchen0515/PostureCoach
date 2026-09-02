#include "sensors.h"
#include "logic.h"
#include "display.h"
#include <U8g2lib.h>

unsigned long lastPrint = 0;

U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void setup() {
  delay(500);
  Serial.begin(9600);

  initDisplay();
  Logic_setup();
  initSensors();

  // Force correct startup mode
  currentMode = SET_WEEKDAY;

  // Allow display drawing
  displayReady = true;

  // Show splash
  showSplashScreen();
  delay(2000);

  // Draw Select Weekday
  updateDisplay(0);
}

void loop() {
  readSensors();
  float angle = getTiltAngle();
  updateLogic(angle);

  if (millis() - lastPrint >= 250) {
    lastPrint = millis();
    Serial.println(angle);
  }

  updateDisplay(angle);  // <-- THIS is the missing piece
}
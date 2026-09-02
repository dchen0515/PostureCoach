#pragma once
#include <stdint.h>
#include <Arduino.h>
#include <U8g2lib.h>
#include "logic.h"   // gives access to ScreenMode, PostureState, globals

extern U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2;

// Display API
void initDisplay();
void updateDisplay(float angle);

// Message overlay
void showMessage(const __FlashStringHelper* msg, unsigned long duration);
void showMessage(const char* msg, unsigned long duration);
void showMessageUTF8(const char* text, unsigned long duration);

// Calibration bar
void drawCalibrationStability(float stability);

void showMessageUTF8(const char* text, unsigned long duration);
void showMessageUTF8_P(const __FlashStringHelper* ptext, unsigned long duration);
void showMessage(const __FlashStringHelper* msg, unsigned long duration);

// New features: startup screen and display sleep mode
void showSplashScreen();
void requestDisplaySleep();

extern uint8_t currentWeekday;

// UI state (declared in display.cpp)
extern ScreenMode currentMode;
extern int menuIndex;
extern int settingsIndex;
extern int graphMode;

extern float displayAngle;
extern bool displayReady;
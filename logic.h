#pragma once
#include <Arduino.h>
#include <stdint.h>
#include "pins.h"

// ------------------------------------------------------------
// Screen modes
// ------------------------------------------------------------
enum ScreenMode : uint8_t {
    LIVE,
    SUMMARY,
    SETTINGS,
    MENU,
    WEEKLY_SUMMARY,
    SET_WEEKDAY,
    SESSION_TIMER,
    CALIBRATION,
    CALIBRATION_CONFIRM
};

// ------------------------------------------------------------
// Posture states
// ------------------------------------------------------------
enum PostureState : uint8_t {
    GOOD,
    SLIGHT,
    BAD
};

// ------------------------------------------------------------
// Compact Flags (1 byte total)
// ------------------------------------------------------------
struct Flags {
    uint8_t inactive : 1;
    uint8_t sessionActive : 1;
    uint8_t wasBad : 1;
    uint8_t alertShown : 1;
    uint8_t achievement5min : 1;
    uint8_t achievement15min : 1;
    uint8_t achievementDaily85 : 1;
    uint8_t achievementDaily95 : 1;
};

// ------------------------------------------------------------
// Global variables (extern)
// ------------------------------------------------------------
extern float currentAngle;

extern uint32_t goodTime;
extern uint32_t badTime;
extern uint32_t sessionStart;

extern int16_t dailyScore;
extern int16_t slouchThreshold;
extern int16_t alertDelay;
extern int16_t postureQuality;

extern uint16_t goodStreak;

extern int16_t weeklyScores[7];
extern uint8_t currentWeekday;

extern Flags flags;

extern ScreenMode currentMode;
extern int menuIndex;
extern int settingsIndex;

extern bool hasUsedToday;

extern int streak;

// ------------------------------------------------------------
// EEPROM version (updated for optimized layout)
// ------------------------------------------------------------
#define EEPROM_VERSION 5

// ------------------------------------------------------------
// Function prototypes
// ------------------------------------------------------------
void Logic_setup();
void updateLogic(float angle);

void showMessageUTF8(const char* t, unsigned long d);
void showMessageUTF8_P(const __FlashStringHelper* p, unsigned long d);

void requestDisplaySleep();
void updateDisplay(float angle);
void showSplashScreen();

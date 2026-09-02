#include "logic.h"
#include "sensors.h"
#include "display.h"
#include <Arduino.h>
#include "pins.h"
#include "ui_strings.h"
#include <EEPROM.h>

// ------------------------------------------------------------
// Forward declarations
// ------------------------------------------------------------
void saveState();
void loadState();
void handleBreakReminder(unsigned long now);
bool readMenuButton();
bool readScrollButton();

// ------------------------------------------------------------
// Constants
// ------------------------------------------------------------
constexpr unsigned long LONG_PRESS_MS = 1000;
constexpr unsigned long STREAK_MS = 20000;
constexpr unsigned long CONSISTENCY_MS = 3UL * 60UL * 1000UL;
constexpr unsigned long DAILY_RESET_MS = 86400000UL;
constexpr float MOVEMENT_THRESHOLD = 0.2f;
constexpr unsigned long SLOUCH_COOLDOWN_MS = 15000;
constexpr unsigned long BREAK_INACTIVE_THRESHOLD = 5UL * 60UL * 1000UL;
constexpr unsigned long BREAK_REMINDER_COOLDOWN = 30UL * 60UL * 1000UL;

// ------------------------------------------------------------
// UI State
// ------------------------------------------------------------
ScreenMode currentMode = LIVE;
int menuIndex = 0;
int settingsIndex = 0;

// ------------------------------------------------------------
// Startup / Display Sleep
// ------------------------------------------------------------
bool splashShown = false;
unsigned long splashStart = 0;
unsigned long lastDisplayActivity = 0;
bool displaySleeping = false;

bool resetMenuReady = false;
bool exitArmed = false;
bool dayChosen = false;

int streak = 0;

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static inline void msg(const __FlashStringHelper* m, uint16_t dur = 2000) {
  showMessageUTF8_P(m, dur);
}

static inline void setLights(bool g, bool y, bool r, bool buzz) {
  digitalWrite(GREEN_LED_PIN, g);
  digitalWrite(YELLOW_LED_PIN, y);
  digitalWrite(RED_LED_PIN, r);
  digitalWrite(BUZZER_PIN, buzz);
}

int16_t baselineAngle = 0;
int16_t lastBaselineAngle = 0;

void undoCalibration() {
    baselineAngle = lastBaselineAngle;
}

// ------------------------------------------------------------
// Reset helpers
// ------------------------------------------------------------
void resetTodayStats() {
  goodTime = 0;
  badTime = 0;
  goodStreak = 0;
  dailyScore = 0;
  postureQuality = 0;
  sessionStart = millis();
}

void resetWeekStats() {
  for (int i = 0; i < 7; i++)
    weeklyScores[i] = -1;  // no data
}

// ------------------------------------------------------------
// Global state
// ------------------------------------------------------------
float currentAngle = 0;

uint32_t sessionStart = 0;
uint32_t lastUpdate = 0;
uint32_t lastDailyReset = 0;
uint32_t lastMovement = 0;
uint32_t lastSlouchTime = 0;
uint32_t lastSlouchAlert = 0;
uint32_t lastBreakReminder = 0;

uint32_t goodTime = 0;
uint32_t badTime = 0;
uint32_t goodSessionStart = 0;


int16_t smoothedDeviation = 0;
int16_t calibrationStability = 0;

PostureState posture = GOOD;
int16_t postureQuality = 100;

int16_t slouchThreshold = 10;
int16_t badThreshold = 15;
int16_t alertDelay = 10000;

int16_t weeklyScores[7] = { -1, -1, -1, -1, -1, -1, -1 };
int8_t lastWeekAvg = 100;

uint16_t goodStreak = 0;
int16_t dailyScore = 0;

Flags flags = {};

bool slouching = false;
uint32_t slouchStartTime = 0;

uint8_t currentWeekday = 0;
uint8_t initialWeekday = 0;

bool hasUsedToday = false;

// ------------------------------------------------------------
// LED Animation State Machine
// ------------------------------------------------------------
struct LedState {
  uint8_t mode = 0;  // 0=static, 1=inactive blink, 2=red pulse, 3=achievement flash
  bool green = false;
  bool yellow = false;
  bool red = false;
  unsigned long lastUpdate = 0;
  bool phaseOn = false;
  uint8_t flashesDone = 0;
};

static LedState led;

// ------------------------------------------------------------
// Buzzer (non-blocking)
// ------------------------------------------------------------
static bool buzzerActive = false;
static unsigned long buzzerStart = 0;
static const unsigned long BUZZER_DURATION = 200;

static void startBeep() {
  digitalWrite(BUZZER_PIN, HIGH);
  buzzerActive = true;
  buzzerStart = millis() + BUZZER_DURATION;
}

static void updateBuzzer() {
  if (buzzerActive && millis() >= buzzerStart) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }
}

static inline void startAchievementFlash() {
  led.mode = 3;
  led.flashesDone = 0;
  led.phaseOn = false;
}

// ------------------------------------------------------------
// LED Update (non-blocking)
// ------------------------------------------------------------
void updateLEDs() {
  unsigned long now = millis();

  switch (led.mode) {
    case 0:  // static posture LEDs
      digitalWrite(GREEN_LED_PIN, led.green);
      digitalWrite(YELLOW_LED_PIN, led.yellow);
      digitalWrite(RED_LED_PIN, led.red);
      break;

    case 1:  // inactive: slow green blink
      if (now - led.lastUpdate >= 800) {
        led.lastUpdate = now;
        led.phaseOn = !led.phaseOn;
        digitalWrite(GREEN_LED_PIN, led.phaseOn);
        digitalWrite(YELLOW_LED_PIN, LOW);
        digitalWrite(RED_LED_PIN, LOW);
      }
      break;

    case 2:  // bad posture: red pulse
      if (now - led.lastUpdate >= 200) {
        led.lastUpdate = now;
        led.phaseOn = !led.phaseOn;
        digitalWrite(RED_LED_PIN, led.phaseOn);
      }
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(YELLOW_LED_PIN, LOW);
      break;

    case 3:  // achievement flash
      if (now - led.lastUpdate >= 120) {
        led.lastUpdate = now;
        led.phaseOn = !led.phaseOn;

        if (led.phaseOn) {
          digitalWrite(GREEN_LED_PIN, HIGH);
          digitalWrite(YELLOW_LED_PIN, HIGH);
        } else {
          digitalWrite(GREEN_LED_PIN, LOW);
          digitalWrite(YELLOW_LED_PIN, LOW);
          if (++led.flashesDone >= 4) {
            led.mode = 0;
            led.flashesDone = 0;
          }
        }
      }
      digitalWrite(RED_LED_PIN, LOW);
      break;
  }
}

// ------------------------------------------------------------
// EEPROM save/load (version 5)
// ------------------------------------------------------------
void saveState() {
  int addr = 0;

  EEPROM.update(addr++, EEPROM_VERSION);

  EEPROM.put(addr, goodTime);
  addr += 4;
  EEPROM.put(addr, badTime);
  addr += 4;
  EEPROM.put(addr, dailyScore);
  addr += 2;

  EEPROM.put(addr, weeklyScores);
  addr += sizeof(weeklyScores);

  EEPROM.update(addr++, currentWeekday);
}

void loadState() {
  int addr = 0;

  uint8_t version = EEPROM.read(addr++);
  if (version != EEPROM_VERSION) {
    goodTime = badTime = 0;
    dailyScore = 0;
    for (int i = 0; i < 7; i++) weeklyScores[i] = -1;
    currentWeekday = 0;
    lastWeekAvg = 100;
    return;
  }

  EEPROM.get(addr, goodTime);
  addr += 4;
  EEPROM.get(addr, badTime);
  addr += 4;
  EEPROM.get(addr, dailyScore);
  addr += 2;

  EEPROM.get(addr, weeklyScores);
  addr += sizeof(weeklyScores);

  currentWeekday = EEPROM.read(addr++);
}

// ------------------------------------------------------------
// Init
// ------------------------------------------------------------
void initLogic() {
  for (int i = 0; i < 7; i++) {
    weeklyScores[i] = -1;  // -1 = no data yet
  }
  pinMode(CALIBRATE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(MENU_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SCROLL_BUTTON_PIN, INPUT_PULLUP);

  loadState();

  initialWeekday = currentWeekday;

  baselineAngle = 0;
  sessionStart = millis();
  lastUpdate = sessionStart;
  lastDailyReset = sessionStart;
  lastMovement = sessionStart;
  lastSlouchTime = sessionStart;

  flags.inactive = false;
  flags.sessionActive = false;
  flags.wasBad = false;
  flags.alertShown = false;

  setLights(0, 0, 0, 0);

  led.mode = 0;
  led.green = false;
  led.yellow = false;
  led.red = false;
  led.lastUpdate = millis();
  led.phaseOn = false;
  led.flashesDone = 0;
}

void Logic_setup() {
  initLogic();
  slouching = false;
  slouchStartTime = 0;
  splashShown = false;
  splashStart = millis();
  lastDisplayActivity = millis();
  displaySleeping = false;

  currentMode = SET_WEEKDAY;
}

// ------------------------------------------------------------
// Button Input
// ------------------------------------------------------------
void handleButtonInput() {

  // ------------------------------------------------------------
  // 1. Handle CALIBRATION_CONFIRM (modal override)
  // ------------------------------------------------------------
  if (currentMode == CALIBRATION_CONFIRM) {

    // SCROLL = No
    if (readScrollButton()) {
      currentMode = LIVE;
      return;
    }

    // MENU short press = Yes
    if (!readMenuButton()) {  // MENU released
      static unsigned long confirmPressStart = 0;
      static bool confirmPressing = false;

      // Detect MENU press start
      if (readMenuButton()) {
        if (!confirmPressing) {
          confirmPressing = true;
          confirmPressStart = millis();
        }
      }
      // Detect MENU release → short press
      else if (confirmPressing) {
        if (millis() - confirmPressStart < LONG_PRESS_MS) {
          currentMode = CALIBRATION;
        }
        confirmPressing = false;
      }
    }

    return;  // Block all other logic
  }

  // ------------------------------------------------------------
  // 2. Do NOT allow long-press MENU to exit SET_WEEKDAY
  // ------------------------------------------------------------
  if (currentMode == SET_WEEKDAY) {
    return;
  }

  // ------------------------------------------------------------
  // 3. If we just entered MENU, ignore MENU button until released
  // ------------------------------------------------------------
  if (currentMode == MENU && !exitArmed) {
    if (!readMenuButton()) {
      exitArmed = true;  // Now we can accept presses
    }
    return;
  }

  // ------------------------------------------------------------
  // 4. Ignore simultaneous MENU + SCROLL
  // ------------------------------------------------------------
  if (readMenuButton() && readScrollButton()) return;

  // ------------------------------------------------------------
  // 5. MENU button press tracking (for long/short press)
  // ------------------------------------------------------------
  static unsigned long pressStart = 0;
  static bool longPressFired = false;
  bool pressed = readMenuButton();

  if (pressed) {
    if (!pressStart) {
      pressStart = millis();
      longPressFired = false;
    }

    // Long press toggles LIVE <-> MENU
    if (!longPressFired && millis() - pressStart >= LONG_PRESS_MS) {
      longPressFired = true;

      // If already in MENU, long press returns to LIVE
      if (currentMode == MENU) {
        currentMode = LIVE;
        exitArmed = false;
        return;
      }

      // If in LIVE, long press enters MENU
      if (currentMode == LIVE) {
        currentMode = MENU;
        exitArmed = false;
        resetMenuReady = true;  // Reset menuReady on entry
        return;
      }

      // In submenus, long press does nothing
      return;
    }

  } else {
    // ------------------------------------------------------------
    // 6. MENU short press behavior
    // ------------------------------------------------------------
    if (pressStart && !longPressFired && millis() - pressStart < LONG_PRESS_MS) {

      // Two-press exit for submenus (except SETTINGS)
      if (currentMode != MENU && currentMode != LIVE && currentMode != SETTINGS) {
        if (!exitArmed) {
          exitArmed = true;  // first press arms exit
        } else {
          currentMode = MENU;  // second press exits
          exitArmed = false;
        }
        pressStart = 0;
        return;
      }

      // Summary screens return to LIVE
      if (currentMode == SUMMARY || currentMode == WEEKLY_SUMMARY) {
        currentMode = LIVE;
      }
    }

    pressStart = 0;
    longPressFired = false;
  }
}

// ------------------------------------------------------------
// Menu Mode
// ------------------------------------------------------------
void handleMenuMode() {
  if (currentMode != MENU) return;

  static bool menuReady = false;
  static unsigned long pressStart = 0;
  static unsigned long lastPress = 0;

  // NEW: Compute menu count dynamically
  uint8_t menuCount = sizeof(menu_items) / sizeof(menu_items[0]);

  // Reset menuReady every time we enter MENU
  if (resetMenuReady) {
    menuReady = false;
    resetMenuReady = false;
    exitArmed = false;  // Block input until release
  }

  // Prevent accidental activation when entering MENU
  if (!menuReady) {
    if (!readMenuButton()) {
      menuReady = true;  // Now menu can accept presses
      exitArmed = true;  // And button input is allowed
    }
    return;
  }

  // ------------------------------------------------------------
  // SCROLL BUTTON → move selection
  // ------------------------------------------------------------
  if (readScrollButton()) {
    if (millis() - lastPress > 180) {
      menuIndex = (menuIndex + 1) % menuCount;  // ⭐ FIXED
      lastPress = millis();
    }
    return;
  }

  // ------------------------------------------------------------
  // MENU BUTTON PRESSED
  // ------------------------------------------------------------
  if (readMenuButton()) {
    if (!pressStart) pressStart = millis();
    return;
  }

  // ------------------------------------------------------------
  // MENU BUTTON RELEASED → short press
  // ------------------------------------------------------------
  if (pressStart) {
    bool shortPress = (millis() - pressStart < LONG_PRESS_MS);

    if (shortPress) {
      switch (menuIndex) {

        case 0:  // Live
          currentMode = LIVE;
          break;

        case 1:  // Daily Summary
          currentMode = SUMMARY;
          break;

        case 2:  // Settings
          currentMode = SETTINGS;
          break;

        case 3:  // Weekly Summary
          currentMode = WEEKLY_SUMMARY;
          break;

        case 4:  // Reset Today
          resetTodayStats();
          msg(F("Today reset"), 1500);
          hasUsedToday = false;
          break;

        case 5:  // Reset Week
          resetWeekStats();
          msg(F("Week reset"), 1500);
          hasUsedToday = false;
          break;

        case 6:  // Session Timer
          currentMode = SESSION_TIMER;
          break;

        case 7:               // Undo Calibration
          undoCalibration();  
          msg(F("Calibration undone"), 1500);
          currentMode = LIVE;
          break;
      }

      lastPress = millis();
    }

    pressStart = 0;
  }
}

// ------------------------------------------------------------
// Settings Mode
// ------------------------------------------------------------
void handleSettingsMode() {
  if (currentMode != SETTINGS) return;

  static unsigned long pressStart = 0;
  static unsigned long lastPress = 0;

  // Scroll button → move between settings
  if (readScrollButton()) {
    if (millis() - lastPress > 180) {
      settingsIndex = (settingsIndex + 1) % 2;
      lastPress = millis();
    }
    return;
  }

  // MENU button pressed
  if (readMenuButton()) {
    if (!pressStart) pressStart = millis();
    return;
  }

  // MENU button released
  if (pressStart) {
    unsigned long held = millis() - pressStart;

    // SHORT PRESS → change setting
    if (held < LONG_PRESS_MS) {

      if (settingsIndex == 0) {
        if (slouchThreshold == 10) slouchThreshold = 15;
        else if (slouchThreshold == 15) slouchThreshold = 20;
        else slouchThreshold = 10;
      } else {
        alertDelay = (alertDelay >= 20000 ? 5000 : alertDelay + 5000);
      }

      lastPress = millis();
      pressStart = 0;
      return;
    }

    // LONG PRESS → exit SETTINGS
    currentMode = MENU;
    exitArmed = false;
    pressStart = 0;
    return;
  }
}

// ------------------------------------------------------------
// Session Timing
// ------------------------------------------------------------
void handleSessionTiming() {
  unsigned long now = millis();
  unsigned long dt = now - lastUpdate;
  lastUpdate = now;

  // --- movement / inactivity ---
  static float lastAngle = 0.0f;
  if (fabs(currentAngle - lastAngle) > MOVEMENT_THRESHOLD) {
    lastMovement = now;
    flags.inactive = false;
  }
  lastAngle = currentAngle;

  if (now - lastMovement > BREAK_INACTIVE_THRESHOLD) {
    flags.inactive = true;
  }

  handleBreakReminder(now);

  // If we're inactive, only show LED state, but DO NOT block timing forever
  if (flags.inactive) {
    led.mode = 1;
    led.green = true;
    led.yellow = false;
    led.red = false;
    return;
  }

  // Do not count posture time until the user chooses the weekday
  if (!dayChosen) return;

  // --- posture timing ---
  if (posture == GOOD) {
    goodTime += dt;
  } else {
    badTime += dt;
  }

  unsigned long total = millis() - sessionStart;
  if (total > 0) {
    dailyScore = (100.0f * goodTime) / total;
    dailyScore = constrain(dailyScore, 0, 100);
  }

  // Mark as "used today" after 5s of real posture time
  if (goodTime + badTime > 5000) {
    hasUsedToday = true;
  }

  // Only write weeklyScores after some real session time AND real usage
  if (hasUsedToday && total > 5000) {
    weeklyScores[currentWeekday] = dailyScore;
  }
}

// ------------------------------------------------------------
// Calibration
// ------------------------------------------------------------
void handleCalibration() {

  // If user releases the button early → cancel calibration
  if (digitalRead(CALIBRATE_BUTTON_PIN) != LOW) {
    calibrationStability = 0;
    currentMode = LIVE;
    return;
  }

  // Require user to be within ±7° of upright before calibration can proceed
  float delta = fabs(currentAngle - baselineAngle);
  if (delta > 7.0f) {
    calibrationStability = 0;
    msg(F("Sit upright"), 500);
    return;
  }

  // Stability tracking
  static float lastAngle = currentAngle;
  float diff = fabs(currentAngle - lastAngle);
  lastAngle = currentAngle;

  if (diff < 0.5f)
    calibrationStability += 20;
  else
    calibrationStability -= 50;

  calibrationStability = constrain(calibrationStability, 0, 1000);

  // Calibration complete
  if (calibrationStability >= 1000) {

    // Save previous baseline so we can undo it later
    lastBaselineAngle = baselineAngle;

    // Write new baseline
    baselineAngle = (int16_t)currentAngle;

    calibrationStability = 0;

    msg(F("Done calibrating!"), 1500);

    currentMode = LIVE;
  }
}

// ------------------------------------------------------------
// Posture transition feedback
// ------------------------------------------------------------
static void postureTransitionFeedback(PostureState from, PostureState to) {
  if (from == GOOD && to == SLIGHT) {
    startBeep();
    msg(F("Slight"), 800);
  } else if (from == SLIGHT && to == BAD) {
    startBeep();
    msg(F("Bad!"), 800);
  } else if (from == BAD && to == SLIGHT) {
    startBeep();
    msg(F("Better"), 800);
  } else if (from == SLIGHT && to == GOOD) {
    startBeep();
    msg(F("Good!"), 800);
  }
}

// ------------------------------------------------------------
// Posture Processing
// ------------------------------------------------------------
void handlePostureProcessing() {

  float deviation = currentAngle - baselineAngle;

  smoothedDeviation = (int16_t)(0.9f * smoothedDeviation + 0.1f * deviation);

  postureQuality = 100 - (int16_t)(fabs(smoothedDeviation) * 3);
  postureQuality = constrain(postureQuality, 0, 100);

  badThreshold = slouchThreshold + 5;

  static PostureState lastPosture = GOOD;
  static unsigned long lastChange = 0;

  lastDisplayActivity = millis();
  displaySleeping = false;

  if (millis() - lastChange < 200) return;
  lastChange = millis();

  // posture classification using absolute deviation
  float delta = fabs(smoothedDeviation);

  PostureState newPosture;
  if (delta < 10.0f) {
    newPosture = GOOD;
  } else if (delta <= 15.0f) {
    newPosture = SLIGHT;
  } else {
    newPosture = BAD;
  }

  if (newPosture != lastPosture) {
    postureTransitionFeedback(lastPosture, newPosture);
    hasUsedToday = true;  // REAL USAGE DETECTED
  }

  lastPosture = newPosture;
  posture = newPosture;

  // Prevent false inactivity when posture is GOOD
  if (posture == GOOD) {
    lastMovement = millis();
    flags.inactive = false;
  }

  if (!flags.inactive) {
    if (posture == GOOD) {
      led.mode = 0;
      led.green = true;
      led.yellow = false;
      led.red = false;
    } else if (posture == SLIGHT) {
      led.mode = 0;
      led.green = false;
      led.yellow = true;
      led.red = false;
    } else {
      led.mode = 2;
      led.green = false;
      led.yellow = false;
      led.red = true;
    }
  }
}

// ------------------------------------------------------------
// Slouch alerts
// ------------------------------------------------------------
static void handleSlouchAlertCore(unsigned long now) {

  if (!flags.alertShown && now - lastSlouchAlert > SLOUCH_COOLDOWN_MS && badTime >= (unsigned long)alertDelay) {

    msg(F("Slouch!"), 1200);
    flags.alertShown = true;
    lastSlouchAlert = now;
  }

  static unsigned long lastReminderBeep = 0;
  const unsigned long REMINDER_INTERVAL = 30000;

  if (now - lastReminderBeep >= REMINDER_INTERVAL) {
    startBeep();
    lastReminderBeep = now;
  }

  flags.wasBad = true;
}

// ------------------------------------------------------------
// Streaks & Alerts
// ------------------------------------------------------------
void handleStreaksAndAlerts() {
  unsigned long now = millis();

  static bool recoveryShown = false;
  static bool consistencyShown = false;
  static bool streakShown = false;

  if (posture == GOOD) {

    flags.alertShown = false;

    if (flags.wasBad && badTime > 3000 && !recoveryShown) {
      startBeep();
      msg(F("Recover!"), 1000);
      recoveryShown = true;
    }

    flags.wasBad = false;

    if (!flags.sessionActive) {
      goodSessionStart = now;
      flags.sessionActive = true;
      consistencyShown = false;
      streakShown = false;
    }

    if (!consistencyShown && now - goodSessionStart >= CONSISTENCY_MS) {
      msg(F("Steady!"), 1000);
      consistencyShown = true;
    }

    if (!streakShown && flags.sessionActive && (now - goodSessionStart >= STREAK_MS)) {

      goodStreak++;
      msg(F("Great!"), 1000);
      streakShown = true;
      flags.sessionActive = false;
    }

    lastSlouchTime = now;
    return;
  }

  recoveryShown = false;
  consistencyShown = false;
  streakShown = false;

  if (posture == BAD) {
    handleSlouchAlertCore(now);
  }
}

// ------------------------------------------------------------
// Achievements
// ------------------------------------------------------------
void handleAchievements() {
  unsigned long sessionElapsed = millis() - sessionStart;
  if (sessionElapsed < 60000) return;
  if (goodTime < 60000) return;

  unsigned long now = millis();
  unsigned long sinceLastSlouch = now - lastSlouchTime;

  if (!flags.achievement5min && goodTime >= 5UL * 60UL * 1000UL && sinceLastSlouch >= 5UL * 60UL * 1000UL) {

    flags.achievement5min = 1;
    msg(F("5 min!"), 1200);
    startAchievementFlash();
  }

  if (!flags.achievement15min && goodTime >= 15UL * 60UL * 1000UL && sinceLastSlouch >= 10UL * 60UL * 1000UL) {

    flags.achievement15min = 1;
    msg(F("15 min!"), 1200);
    startAchievementFlash();
  }

  if (!flags.achievementDaily85 && dailyScore >= 85) {
    flags.achievementDaily85 = 1;
    msg(F("85%!"), 1200);
    startAchievementFlash();
  }

  if (!flags.achievementDaily95 && dailyScore >= 95) {
    flags.achievementDaily95 = 1;
    msg(F("95%!"), 1200);
    startAchievementFlash();
  }
}

// ------------------------------------------------------------
// Daily Reset
// ------------------------------------------------------------
void handleDailyReset() {
  unsigned long now = millis();
  if (now - lastDailyReset < DAILY_RESET_MS) return;

  weeklyScores[currentWeekday] =
    (goodTime + badTime > 0) ? dailyScore : -1;

  currentWeekday = (currentWeekday + 1) % 7;

  int sum = 0;
  for (int i = 0; i < 7; i++) sum += weeklyScores[i];
  lastWeekAvg = sum / 7;

  goodTime = badTime = 0;
  dailyScore = 0;
  goodStreak = 0;
  sessionStart = now;

  flags.achievement5min = 0;
  flags.achievement15min = 0;
  flags.achievementDaily85 = 0;
  flags.achievementDaily95 = 0;

  lastDailyReset = now;

  msg(F("New day!"), 1500);
}

// ------------------------------------------------------------
// Break Reminder
// ------------------------------------------------------------
void handleBreakReminder(unsigned long now) {
  static bool breakShown = false;

  if (!flags.inactive) {
    breakShown = false;
    return;
  }

  bool longInactive = (now - lastMovement >= BREAK_INACTIVE_THRESHOLD);
  bool cooldownOver = (now - lastBreakReminder >= BREAK_REMINDER_COOLDOWN);

  if (longInactive && cooldownOver && !breakShown) {
    msg(F("Break!"), 1500);
    lastBreakReminder = now;
    breakShown = true;
  }
}

// ------------------------------------------------------------
// Button debouncing
// ------------------------------------------------------------
bool readMenuButton() {
  static bool stable = HIGH;
  static unsigned long lastChange = 0;
  bool raw = digitalRead(MENU_BUTTON_PIN);

  if (raw != stable && millis() - lastChange > 25) {
    stable = raw;
    lastChange = millis();
  }
  return stable == LOW;
}

bool readScrollButton() {
  static bool stable = HIGH;
  static unsigned long lastChange = 0;
  bool raw = digitalRead(SCROLL_BUTTON_PIN);

  if (raw != stable && millis() - lastChange > 25) {
    stable = raw;
    lastChange = millis();
  }
  return stable == LOW;
}

// ------------------------------------------------------------
// Weekday selection
// ------------------------------------------------------------
void handleSetWeekday() {
  if (currentMode != SET_WEEKDAY) return;

  if (readScrollButton()) {
    static unsigned long lastScroll = 0;
    if (millis() - lastScroll > 180) {
      currentWeekday = (currentWeekday + 1) % 7;
      lastScroll = millis();
    }
    return;
  }

  static unsigned long pressStart = 0;

  if (readMenuButton()) {
    if (!pressStart) pressStart = millis();
  } else {
    if (pressStart && millis() - pressStart < LONG_PRESS_MS) {

      if (currentWeekday != initialWeekday) {
        resetTodayStats();
      }

      // User has officially chosen the day
      dayChosen = true;
      sessionStart = millis();  // clean session start
      goodTime = 0;
      badTime = 0;

      saveState();

      // Update streak based on yesterday's score
      if (weeklyScores[initialWeekday] >= 75) {
        streak++;
      } else {
        streak = 0;
      }
      currentMode = LIVE;
    }
    pressStart = 0;
  }
}

// ------------------------------------------------------------
// Main update loop
// ------------------------------------------------------------
void updateLogic(float angle) {
  currentAngle = angle;

  // // --- Splash screen ---
  // if (!splashShown) {
  //   if (millis() - splashStart < 1500) {
  //     showSplashScreen();
  //     updateLEDs();
  //     updateBuzzer();
  //     return;
  //   } else {
  //     splashShown = true;
  //   }
  // }

  // --- Calibration button handling ---
  static bool calReady = true;
  static unsigned long calPressStart = 0;

  if (digitalRead(CALIBRATE_BUTTON_PIN) == LOW) {
    if (calReady && !calPressStart)
      calPressStart = millis();

    if (calReady && millis() - calPressStart > LONG_PRESS_MS) {
      calReady = false;
      currentMode = CALIBRATION;
      return;
    }
  } else {
    calReady = true;
    calPressStart = 0;
  }

  // --- Calibration mode ---
  if (currentMode == CALIBRATION) {
    handleCalibration();
    updateBuzzer();
    updateLEDs();
    return;  // no direct updateDisplay here
  }

  // KEEP DISPLAY AWAKE DURING SET_WEEKDAY
  if (currentMode == SET_WEEKDAY) {
    lastDisplayActivity = millis();
    displaySleeping = false;
  }

  // FIXED SLEEP LOGIC
  // Only sleep in LIVE, MENU, SUMMARY, WEEKLY_SUMMARY, SETTINGS, SESSION_TIMER
  if (!displaySleeping && currentMode != SET_WEEKDAY && millis() - lastDisplayActivity > 10000) {

    saveState();
    displaySleeping = true;
    requestDisplaySleep();
  }

  if (displaySleeping) {
    updateBuzzer();
    updateLEDs();
    return;
  }

  // --- Normal operation ---
  handleButtonInput();  // detect presses FIRST
  handleMenuMode();
  handleSettingsMode();
  // Only run SET_WEEKDAY logic when actually in that mode
  if (currentMode == SET_WEEKDAY) {
    handleSetWeekday();
  }
  handlePostureProcessing();
  handleSessionTiming();
  handleStreaksAndAlerts();
  handleAchievements();
  handleDailyReset();
  handleBreakReminder(millis());
  updateBuzzer();
  updateLEDs();

  static unsigned long lastSave = 0;
  if (millis() - lastSave > 300000) {
    saveState();
    lastSave = millis();
  }

  updateDisplay(angle);
}
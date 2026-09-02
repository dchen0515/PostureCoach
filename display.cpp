#include "display.h"
#include "logic.h"
#include "ui_strings.h"
#include <U8g2lib.h>
#include <Wire.h>

// ------------------------------------------------------------
// External state
// ------------------------------------------------------------
extern Flags flags;
extern int16_t weeklyScores[7];
extern int16_t dailyScore, slouchThreshold, alertDelay, postureQuality;
extern uint32_t goodTime, badTime, sessionStart;
extern uint16_t goodStreak;
extern float currentAngle;
extern PostureState posture;
extern uint8_t currentWeekday;

extern ScreenMode currentMode;
extern int menuIndex;
extern int settingsIndex;

bool displayReady = false;

// ------------------------------------------------------------
// Message system
// ------------------------------------------------------------
static char msgText[48];
static unsigned long msgExpire = 0;

void showMessageUTF8(const char* t, unsigned long d) {
  strncpy(msgText, t, sizeof(msgText) - 1);
  msgText[sizeof(msgText) - 1] = 0;
  msgExpire = millis() + d;
}

void showMessageUTF8_P(const __FlashStringHelper* p, unsigned long d) {
  strncpy_P(msgText, (const char*)p, sizeof(msgText) - 1);
  msgText[sizeof(msgText) - 1] = 0;
  msgExpire = millis() + d;
}

static inline void wrapMsg() {
  u8g2.setCursor(4, 34);
  u8g2.print(msgText);
}

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static inline void drawDivider() {
  u8g2.drawHLine(0, 13, 128);
}

void drawCenteredTitle(const __FlashStringHelper* t, uint8_t y) {
  char buf[32];
  strncpy_P(buf, (char*)t, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;

  uint16_t w = u8g2.getStrWidth(buf);
  uint8_t x = (128 - w) / 2;

  u8g2.setCursor(x, y);
  u8g2.print(buf);
}

static inline void drawDottedHLine(int y, int startX = 18, int endX = 122) {
  for (int x = startX; x <= endX; x += 4) {
    u8g2.drawPixel(x, y);
  }
}

// ------------------------------------------------------------
// Splash
// ------------------------------------------------------------
static const char splashText[] PROGMEM = "POSTURE COACH";

static void drawSplash() {
  u8g2.setCursor(28, 32);
  u8g2.print((const __FlashStringHelper*)splashText);
}

void initDisplay() {
  Wire.begin();
  Wire.setClock(100000);
  u8g2.begin();
  u8g2.setContrast(200);
  u8g2.setFont(u8g2_font_5x8_tf);

  u8g2.firstPage();
  do { drawSplash(); } while (u8g2.nextPage());
}

void showSplashScreen() {
  u8g2.firstPage();
  do { drawSplash(); } while (u8g2.nextPage());
}

// ------------------------------------------------------------
// Screens
// ------------------------------------------------------------
static void liveScreen() {
  drawCenteredTitle(F("Live"), 10);
  drawDivider();

  u8g2.setCursor(4, 26);
  u8g2.print(F("Angle: "));
  u8g2.print(currentAngle, 1);
  u8g2.print(" deg");

  u8g2.setCursor(4, 36);
  u8g2.print(F("Good: "));
  u8g2.print(goodTime / 1000);
  u8g2.print("s");

  u8g2.setCursor(4, 46);
  u8g2.print(F("Bad:  "));
  u8g2.print(badTime / 1000);
  u8g2.print("s");

  u8g2.setCursor(70, 36);
  u8g2.print(F("Score: "));
  u8g2.print(dailyScore);
  u8g2.print("%");

  u8g2.setCursor(70, 46);
  u8g2.print(F("Qual:  "));
  u8g2.print(postureQuality);
  u8g2.print("%");

  // Optional slouch alert icon
  if (posture == BAD) {
    u8g2.drawTriangle(118, 4, 124, 4, 121, 10);
  }
}

static void summaryScreen() {
  drawCenteredTitle(F("Summary"), 10);
  drawDivider();

  u8g2.setCursor(4, 28);
  u8g2.print(F("Good: "));
  u8g2.print(goodTime / 1000);
  u8g2.print("s");

  u8g2.setCursor(4, 38);
  u8g2.print(F("Bad:  "));
  u8g2.print(badTime / 1000);
  u8g2.print("s");

  u8g2.setCursor(4, 48);
  u8g2.print(F("Score: "));
  u8g2.print(dailyScore);
  u8g2.print("%");

  u8g2.setCursor(4, 58);
  u8g2.print(F("Streak: "));
  u8g2.print(streak);
  u8g2.print(F(" "));

  switch (streak) {
    case 1:
      u8g2.print(F("day"));
      break;
    default:
      u8g2.print(F("days"));
      break;
  }
}

static void menuScreen() {
  drawCenteredTitle(F("Menu"), 10);
  drawDivider();

  const uint8_t leftX = 4;
  const uint8_t rightX = 58;
  const uint8_t startY = 24;
  const uint8_t rowH = 10;

  // Automatically compute number of menu items
  uint8_t menuCount = sizeof(menu_items) / sizeof(menu_items[0]);

  // Loop through all menu items (now 8)
  for (uint8_t i = 0; i < menuCount; i++) {
    bool leftCol = (i < 4);
    uint8_t colX = leftCol ? leftX : rightX;
    uint8_t row = leftCol ? i : (i - 4);
    uint8_t y = startY + row * rowH;

    u8g2.setCursor(colX, y);
    u8g2.print(i == menuIndex ? "> " : "  ");

    char buf[18];
    strncpy_P(buf, (char*)pgm_read_ptr(&menu_items[i]), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    u8g2.print(buf);
  }
}

static void settingsScreen() {
  drawCenteredTitle(F("Settings"), 10);
  drawDivider();

  const int vals[2] = { slouchThreshold, alertDelay / 1000 };

  for (uint8_t i = 0; i < 2; i++) {
    u8g2.setCursor(4, 26 + i * 12);
    u8g2.print(i == settingsIndex ? "> " : "  ");

    char buf[18];
    strncpy_P(buf, (char*)pgm_read_ptr(&settings_items[i]), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    u8g2.print(buf);
    u8g2.print(": ");
    u8g2.print(vals[i]);
  }

  u8g2.setCursor(10, 60);
  u8g2.print("Hold MENU to exit");
}

static void calibrationConfirmScreen() {
  drawCenteredTitle(F("Calibrate?"), 10);
  drawDivider();

  u8g2.setCursor(20, 34);
  u8g2.print(F("Menu = Yes"));

  u8g2.setCursor(20, 48);
  u8g2.print(F("Scroll = No"));
}

static void weeklyScreen() {
  // Move title UP
  drawCenteredTitle(F("Weekly"), 6);

  // Move divider UP
  u8g2.drawHLine(0, 10, 128);

  // Graph layout for 128x64
  const int graphBottom = 54;  // where bars sit on the Y axis
  const int graphHeight = 38;  // bar height range

  // ------------------------------------------------------------
  // Percentage guide lines + labels
  // ------------------------------------------------------------
  u8g2.setFont(u8g2_font_5x8_tf);

  // 100% line (top of graph)
  drawDottedHLine(graphBottom - graphHeight, 18, 122);
  u8g2.setCursor(0, graphBottom - graphHeight + 3);
  u8g2.print(F("100"));

  // 75%
  drawDottedHLine(graphBottom - (graphHeight * 3 / 4), 18, 122);
  u8g2.setCursor(0, graphBottom - (graphHeight * 3 / 4) + 3);
  u8g2.print(F("75"));

  // 50%
  drawDottedHLine(graphBottom - (graphHeight / 2), 18, 122);
  u8g2.setCursor(0, graphBottom - (graphHeight / 2) + 3);
  u8g2.print(F("50"));

  // 25%
  drawDottedHLine(graphBottom - (graphHeight / 4), 18, 122);
  u8g2.setCursor(0, graphBottom - (graphHeight / 4) + 3);
  u8g2.print(F("25"));
  // ------------------------------------------------------------

  for (uint8_t i = 0; i < 7; i++) {
    int score = weeklyScores[i];
    int x = 15 + i * 16;

    if (score >= 0) {
      // Compute bar height
      int h = (constrain(score, 0, 100) * graphHeight) / 100;
      int y = graphBottom - h;

      // Draw bar (10 px wide, centered inside highlight frame)
      if (h > 0) {
        u8g2.drawBox(x + 1, y, 10, h);
      }

      // Highlight current day
      if (i == currentWeekday) {
        u8g2.drawFrame(x - 1, y - 1, 14, h + 2);
      }

    } else {
      // EMPTY DAY — draw nothing
      if (i == currentWeekday) {
        // Small highlight box for empty current day
        u8g2.drawFrame(x - 1, graphBottom - 1, 14, 2);
      }
    }

    // Draw weekday label (moved DOWN for spacing)
    char buf[4];
    strncpy_P(buf, (char*)pgm_read_ptr(&weekday_labels[i]), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    uint16_t w = u8g2.getStrWidth(buf);
    int labelX = x + 6 - (w / 2);

    u8g2.setCursor(labelX, 63);  // perfect for 128x64
    u8g2.print(buf);
  }
}

static void timerScreen() {
  drawCenteredTitle(F("Timer"), 12);
  drawDivider();

  u8g2.setCursor(4, 34);
  u8g2.print((millis() - sessionStart) / 1000);
  u8g2.print(" sec");
}

static void setWeekdayScreen() {
  drawCenteredTitle(F("Select Day"), 10);
  drawDivider();

  static const char* const days[7] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };

  u8g2.setCursor(50, 36);
  u8g2.print(days[currentWeekday]);

  u8g2.setCursor(10, 50);
  u8g2.print("Scroll = Change");

  u8g2.setCursor(10, 60);
  u8g2.print("Menu = Confirm");
}

// ------------------------------------------------------------
// Frame dispatcher
// ------------------------------------------------------------
static void drawFrame() {
  if (msgText[0] && millis() < msgExpire) {
    wrapMsg();
    return;
  }
  if (millis() >= msgExpire) msgText[0] = 0;

  switch (currentMode) {
    case SUMMARY: summaryScreen(); break;
    case SETTINGS: settingsScreen(); break;
    case MENU: menuScreen(); break;
    case WEEKLY_SUMMARY: weeklyScreen(); break;
    case SET_WEEKDAY: setWeekdayScreen(); break;
    case SESSION_TIMER: timerScreen(); break;
    case CALIBRATION_CONFIRM:
      calibrationConfirmScreen();
      break;

    case CALIBRATION:
      drawCenteredTitle(F("Calibration"), 10);
      drawDivider();

      u8g2.setCursor(10, 34);
      u8g2.print("Please hold still");

      {
        static uint8_t dotCount = 0;
        static unsigned long lastDotUpdate = 0;

        if (millis() - lastDotUpdate > 300) {
          dotCount = (dotCount + 1) % 4;
          lastDotUpdate = millis();
        }

        u8g2.setCursor(10, 48);
        u8g2.print("Calibrating");
        for (uint8_t i = 0; i < dotCount; i++)
          u8g2.print(".");
      }
      break;

    case LIVE:
    default:
      if (flags.inactive) {
        drawCenteredTitle(F("Inactive"), 28);
      } else {
        liveScreen();
      }
      break;
  }
}

// ------------------------------------------------------------
// Display update loop
// ------------------------------------------------------------
void updateDisplay(float angle) {
  if (!displayReady) return;

  static unsigned long lastDraw = 0;
  unsigned long now = millis();

  if (now - lastDraw < 50) return;
  lastDraw = now;

  u8g2.firstPage();
  do { drawFrame(); } while (u8g2.nextPage());
}

// ------------------------------------------------------------
// Sleep screen
// ------------------------------------------------------------
void requestDisplaySleep() {
  u8g2.firstPage();
  do {
  } while (u8g2.nextPage());
}
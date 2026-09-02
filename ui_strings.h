#pragma once
#include <Arduino.h>

// ------------------------------------------------------------
// Startup
// ------------------------------------------------------------
const char ui_startup_title[] PROGMEM = "Posture Coach";

// ------------------------------------------------------------
// Live Screen Labels
// ------------------------------------------------------------
const char ui_angle[] PROGMEM = "Angle: ";
const char ui_good_label[] PROGMEM = "Good: ";
const char ui_bad_label[] PROGMEM = "Bad:  ";
const char ui_score_label[] PROGMEM = "Score: ";
const char ui_streak_label[] PROGMEM = "Streak: ";
const char ui_inactive[] PROGMEM = "Inactive";

// ------------------------------------------------------------
// Menu Labels (7 items)
// ------------------------------------------------------------
const char ui_menu_live[] PROGMEM = "Live";
const char ui_menu_summary[] PROGMEM = "Summary";
const char ui_menu_settings[] PROGMEM = "Settings";
const char ui_menu_weekly[] PROGMEM = "Weekly";
const char ui_menu_reset_today[] PROGMEM = "Reset Today";
const char ui_menu_reset_week[] PROGMEM = "Reset Week";
const char ui_menu_timer[] PROGMEM = "Timer";
const char ui_menu_undo_calib[] PROGMEM = "Undo Calib";

const char* const menu_items[] PROGMEM = {
  ui_menu_live,
  ui_menu_summary,
  ui_menu_settings,
  ui_menu_weekly,
  ui_menu_reset_today,
  ui_menu_reset_week,
  ui_menu_timer,
  ui_menu_undo_calib
};

// ------------------------------------------------------------
// Settings Labels (2 items)
// ------------------------------------------------------------
const char ui_set_slouch[] PROGMEM = "Slouch Thresh";
const char ui_set_delay[] PROGMEM = "Alert Delay";

const char* const settings_items[] PROGMEM = {
  ui_set_slouch,
  ui_set_delay
};

// ------------------------------------------------------------
// Weekday Labels
// ------------------------------------------------------------
const char wd_M[] PROGMEM = "M";
const char wd_Tu[] PROGMEM = "Tu";
const char wd_W[] PROGMEM = "W";
const char wd_Th[] PROGMEM = "Th";
const char wd_F[] PROGMEM = "F";
const char wd_Sa[] PROGMEM = "Sa";
const char wd_Su[] PROGMEM = "Su";

const char* const weekday_labels[] PROGMEM = {
  wd_M, wd_Tu, wd_W, wd_Th, wd_F, wd_Sa, wd_Su
};
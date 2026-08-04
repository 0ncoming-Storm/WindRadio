#pragma once
#include "SystemData.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Arduino.h>
#include <Wire.h>

// screen.h — OLED display UI classes for the WindRadio base station.
//
// Button bitmask layout (uint8_t): bit 0 = A, bit 1 = B, bit 2 = C.
// See screen.cpp::readButtons() for how hardware pins map to these bits.

// MyButtons — debounced reading of the three front-panel buttons.
class MyButtons {
public:
  MyButtons();
  uint8_t readButtonsOnClick(void);

private:
  // Hardware button pins (Arduino pin numbers, not bitmask bits).
  static constexpr uint8_t BUTTON_A = 5;
  static constexpr uint8_t BUTTON_B = 6;
  static constexpr uint8_t BUTTON_C = 9;

  // Raw hardware poll; returns a bitmask of currently-pressed buttons.
  uint8_t readButtons(void);
};

enum ButtonState : uint8_t {
  BTN_NONE = 0b000,
  BTN_A = 0b001,
  BTN_B = 0b010,
  BTN_C = 0b100,
  BTN_MENU_COMBO = 0b011 // A + B
};

// MyDisplay — thin wrapper around the SH1107 OLED for drawing UI screens.
class MyDisplay {
public:
  MyDisplay();
  void init();
  void showCurrentInformation(int windSpeed, float tempriture, int hours,
                              int minuits);
  void showSettingsMenu(String options[], int numOptions, int selectedOption);
  void showStatusViewSelectonScreen();

  void showTheIsValues(const char *deviceNameChar, int maxWind,
                       uint8_t startHour, uint8_t startMin, uint8_t endHour,
                       uint8_t endMin, DeviceMode off_auto_manual,
                       bool controledDeviceOnOrOff);

  void showIntKmhSetting(String label, int value, int highlightTarget);
  void showTimeIntervalSetting(String label, int startHour, int startMin,
                               int endHour, int endMin, int highlightTarget);

private:
  Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
};

enum InfoScreen { INFO_DEFAULT, INFO_SELECT_DEVICE, INFO_SHOW_DEVICE };

// InfoController — manages the read-only information screens (status, device
// view).
class InfoController {
public:
  InfoController(MyDisplay &disp);
  void handleInput(uint8_t buttons);
  void render();
  void resetToDefault();

private:
  MyDisplay &display;
  InfoScreen screen = INFO_DEFAULT;
  DeviceID selectedDevice = DEVICE_GATE;
};

enum MenuScreen {
  MENU_SELECT_DEVICE,
  MENU_DEVICE_OPTIONS,
  MENU_MODE_SELECT,
  MENU_EDIT_WIND,
  MENU_EDIT_SCHEDULE
};

// MenuController — interactive settings menu with navigation + inline editing.
class MenuController {
public:
  MenuController(MyDisplay &disp);
  void reset();
  bool wantsExit();
  void handleInput(uint8_t buttons);
  void render();

private:
  MyDisplay &display;
  MenuScreen screen = MENU_SELECT_DEVICE;
  DeviceID selectedDevice = DEVICE_GATE;
  DeviceSettings editBuffer;

  int listSelection = 0; // which row in the current menu is highlighted
  int editTarget = 1;    // which field is being edited (1-based)
  bool isEditingField = false;
  bool exitRequested = false;

  // Blinking cursor for inline editing
  bool blinkVisible = true;
  unsigned long lastBlinkToggle = 0;
  const unsigned long blinkIntervalMs = 400;

  // Menu option text arrays
  String deviceListOptions[4] = {"Gate", "Pond", "Fountains", "Back"};
  String deviceMenuOptions[4] = {"Schedule", "Wind Limit", "Mode", "Back"};
  String modeSelectOptions[4] = {"Off", "Auto", "Manual", "Back"};

  // Input handlers for list navigation and field editing
  bool navigateList(uint8_t buttons, int numOptions);
  void handleWindEditInput(uint8_t buttons);
  void handleScheduleEditInput(uint8_t buttons);
  void updateBlink();
  int currentBlinkTarget();
};

enum AppMode { MODE_INFO, MODE_MENU };

// App — top-level coordinator: owns display/buttons, dispatches to info/menu.
class App {
public:
  App();
  void init();
  void loop();

private:
  MyDisplay display;
  MyButtons button;

  InfoController infoController;
  MenuController menuController;
  AppMode currentMode = MODE_INFO;

  // Auto-return to info screen after inactivity
  unsigned long lastInputTime = 0;
  const unsigned long infoTimeoutMs = 8000;

  // Combo-button detection (A+B to enter menu)
  uint8_t pendingButtons = 0;
  unsigned long pendingSince = 0;
  const unsigned long comboWindowMs = 60;

  void handleModeEntry(uint8_t buttons);
};

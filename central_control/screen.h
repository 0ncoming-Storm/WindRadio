#pragma once
#include "SystemData.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Arduino.h>
#include <Wire.h>

// screen.h — OLED display UI classes for the WindRadio base station.
//
// Button bitmask layout (uint8_t): bit 0 = A, bit 1 = B, bit 2 = C.

class MyButtons {
public:
  MyButtons();
  uint8_t readButtonsOnClick(void);

private:
  static constexpr uint8_t BUTTON_A = 5;
  static constexpr uint8_t BUTTON_B = 6;
  static constexpr uint8_t BUTTON_C = 9;

  uint8_t readButtons(void);
};

enum ButtonState : uint8_t {
  BTN_NONE = 0b000,
  BTN_A = 0b001,
  BTN_B = 0b010,
  BTN_C = 0b100,
  BTN_MENU_COMBO = 0b011 // A + B
};

class MyDisplay {
public:
  MyDisplay();
  void init();
  void showCurrentInformation(int windSpeed, float temperature, int hours,
                              int minutes);
  void showSettingsMenu(String options[], int numOptions, int selectedOption);
  void showStatusViewSelectionScreen();

  void showTheIsValues(const char *deviceNameChar, int maxWind,
                       uint8_t startHour, uint8_t startMin, uint8_t endHour,
                       uint8_t endMin, DeviceMode off_auto_manual,
                       RelayState relayState);

  void showIntKmhSetting(String label, int value, int highlightTarget);
  void showTimeIntervalSetting(String label, int startHour, int startMin,
                               int endHour, int endMin, int highlightTarget);
  void showErrorBanner(const char *message); // bottom-row warning banner
  void showErrorScreen(const SystemErrors &errors); // full-screen fault view

private:
  Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
};

enum InfoScreen { INFO_DEFAULT, INFO_SELECT_DEVICE, INFO_SHOW_DEVICE };

class InfoController {
public:
  InfoController(MyDisplay &disp);
  void handleInput(uint8_t buttons);
  void resetToDefault();
  void drawDefaultScreen();
  // Called every UI loop tick: refreshes the default/error screen so the
  // error view appears/disappears live as faults come and go.
  void tick(unsigned long now);
  bool isDefaultScreen() const { return screen == INFO_DEFAULT; }

private:
  MyDisplay &display;
  InfoScreen screen = INFO_DEFAULT;
  DeviceID selectedDevice = DEVICE_GATE;

  // Per-error dismiss timestamps (indexed by ErrorCode); errors reappear
  // after the dismiss timeout if the fault persists.
  unsigned long dismissedAt[6] = {0, 0, 0, 0, 0, 0};
  static constexpr unsigned long DISMISS_TIMEOUT_MS = 60000;

  bool isDismissed(ErrorCode code, unsigned long now) const;
  void drawDeviceStatus();
};

enum MenuScreen {
  MENU_SELECT_DEVICE,
  MENU_DEVICE_OPTIONS,
  MENU_MODE_SELECT,
  MENU_EDIT_WIND,
  MENU_EDIT_SCHEDULE
};

class MenuController {
public:
  MenuController(MyDisplay &disp);
  void reset();
  bool wantsExit();
  void handleInput(uint8_t buttons);
  void updateBlink();

private:
  MyDisplay &display;
  MenuScreen screen = MENU_SELECT_DEVICE;
  DeviceID selectedDevice = DEVICE_GATE;
  DeviceSettings editBuffer;

  int listSelection = 0;
  int editTarget = 1;
  bool isEditingField = false;
  bool exitRequested = false;

  // Set when the user saves a schedule with start == end; the schedule
  // screen shows an error banner for a few seconds after returning.
  bool scheduleInvalid = false;
  unsigned long invalidSinceMs = 0;
  static constexpr unsigned long INVALID_BANNER_MS = 3000;

  bool blinkVisible = true;
  unsigned long lastBlinkToggle = 0;
  const unsigned long blinkIntervalMs = 400;

  String deviceListOptions[4] = {"Gate", "Pond", "Fountains", "Back"};
  String deviceMenuOptions[4] = {"Schedule", "Wind Limit", "Mode", "Back"};
  String modeSelectOptions[4] = {"Off", "Auto", "Manual", "Back"};

  bool navigateList(uint8_t buttons, int numOptions);
  void handleWindEditInput(uint8_t buttons);
  void handleScheduleEditInput(uint8_t buttons);
  int currentBlinkTarget();
  void drawCurrentScreen();
};

enum AppMode { MODE_INFO, MODE_MENU };

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

  unsigned long lastInputTime = 0;
  const unsigned long infoTimeoutMs = 8000;
  const unsigned long menuTimeoutMs = 30000;

  uint8_t pendingButtons = 0;
  unsigned long pendingSince = 0;
  const unsigned long comboWindowMs = 60;

  void handleModeEntry(uint8_t buttons);
};

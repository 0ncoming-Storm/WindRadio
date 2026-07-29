#pragma once
#include "SystemData.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Arduino.h>
#include <Wire.h>

class MyDisplay {
private:
  Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);
  uint8_t readButtons(void);

public:
  static constexpr uint8_t BUTTON_A = 5;
  static constexpr uint8_t BUTTON_B = 6;
  static constexpr uint8_t BUTTON_C = 9;

  MyDisplay();
  void init();
  void showCurrentInformation(int windSpeed, float tempriture, int hours,
                              int minuits);
  void showSettingsMenu(String options[], int numOptions, int selectedOption);
  void showStatusViewSelectonScreen();
  void showTheIsValues(String deviceName, int maxWind, String startTime,
                       String endTime, bool controledDeviceOnOrOff,
                       bool controlingOrManual);
  void showIntKmhSetting(String label, int value, int highlightTarget);
  void showTimeIntervalSetting(String label, int startHour, int startMin,
                               int endHour, int endMin, int highlightTarget);
  uint8_t readButtonsDebounced(void);
};

enum InfoScreen { INFO_DEFAULT, INFO_SELECT_DEVICE, INFO_SHOW_DEVICE };

class InfoController {
private:
  MyDisplay &display;
  InfoScreen screen = INFO_DEFAULT;
  DeviceID selectedDevice = DEVICE_GATE;

public:
  InfoController(MyDisplay &disp);
  void handleInput(uint8_t buttons);
  void render();
  void resetToDefault();
};

enum MenuScreen {
  MENU_SELECT_DEVICE,
  MENU_DEVICE_OPTIONS,
  MENU_MODE_SELECT,
  MENU_EDIT_WIND,
  MENU_EDIT_SCHEDULE
};

class MenuController {
private:
  MyDisplay &display;
  MenuScreen screen = MENU_SELECT_DEVICE;
  DeviceID selectedDevice = DEVICE_GATE;
  DeviceSettings editBuffer;

  int listSelection = 0;
  int editTarget = 1;
  bool isEditingField = false;
  bool exitRequested = false;

  bool blinkVisible = true;
  unsigned long lastBlinkToggle = 0;
  const unsigned long blinkIntervalMs = 400;

  String deviceListOptions[4] = {"Gate", "Pond", "Fountains", "Back"};
  String deviceMenuOptions[4] = {"Schedule", "Wind Limit", "Mode", "Back"};
  String modeSelectOptions[4] = {"Off", "Auto", "Manual", "Back"};

  bool navigateList(uint8_t buttons, int numOptions);
  void handleWindEditInput(uint8_t buttons);
  void handleScheduleEditInput(uint8_t buttons);
  void updateBlink();
  int currentBlinkTarget();

public:
  MenuController(MyDisplay &disp);
  void reset();
  bool wantsExit();
  void handleInput(uint8_t buttons);
  void render();
};

enum AppMode { MODE_INFO, MODE_MENU };

class App {
private:
  MyDisplay display;
  InfoController infoController;
  MenuController menuController;
  AppMode currentMode = MODE_INFO;

  unsigned long lastInputTime = 0;
  const unsigned long infoTimeoutMs = 8000;

  uint8_t pendingButtons = 0;
  unsigned long pendingSince = 0;
  const unsigned long comboWindowMs = 60;

  void handleModeEntry(uint8_t buttons);

public:
  App();
  void init();
  void loop();
};

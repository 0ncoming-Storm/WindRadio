#line 1 "/home/storm/Projects/WindRadio/client/screen.cpp"
#include "screen.h"

// Helper function used for UI headers
static String to_upper(String str) {
  str.toUpperCase();
  return str;
}

// ==========================================
// MyButtons Implementation
// ==========================================

MyButtons::MyButtons() {
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);
}
uint8_t MyButtons::readButtons(void) {
  uint8_t buttons = 0;
  if (digitalRead(BUTTON_A) == 0)
    buttons |= 0x1;
  if (digitalRead(BUTTON_B) == 0)
    buttons |= 0x2;
  if (digitalRead(BUTTON_C) == 0)
    buttons |= 0x4;
  return buttons;
}

uint8_t MyButtons::readButtonsOnClick(void) {
  static uint8_t lastReading = 0;
  static uint8_t stableState = 0;
  static uint8_t previousStable = 0;
  static unsigned long lastChangeTime = 0;
  const unsigned long debounceMs = 25;

  uint8_t currentReading = readButtons();
  if (currentReading != lastReading) {
    lastChangeTime = millis();
    lastReading = currentReading;
  }
  if ((millis() - lastChangeTime) > debounceMs) {
    stableState = currentReading;
  }
  uint8_t newlyPressed = stableState & ~previousStable;
  previousStable = stableState;
  return newlyPressed;
}
// ==========================================
// MyDisplay Implementation
// ==========================================

MyDisplay::MyDisplay() {}

void MyDisplay::init() {
  display.begin(0x3C, true);
  display.clearDisplay();
  display.display();
  display.setRotation(3);
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
}
void MyDisplay::showCurrentInformation(int windSpeed, float tempriture,
                                       int hours, int minuits) {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 11, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  display.setCursor(23, 2);
  display.print("SYSTEM STATUS");

  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  display.setCursor(2, 16);
  display.print("Wind Speed: " + String(windSpeed) + " KM/H");

  display.setCursor(2, 29);
  display.print("Temp: " + String(tempriture, 1) + " C");

  display.setCursor(2, 42);
  char timeBuf[6];
  sprintf(timeBuf, "%02d:%02d", hours, minuits);
  display.print("Time: " + String(timeBuf));
  display.display();
}

void MyDisplay::showSettingsMenu(String options[], int numOptions,
                                 int selectedOption) {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 11, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  display.setCursor(30, 2);
  display.print("SETTINGS");

  int windowStart = 0;
  if (numOptions > 3) {
    if (selectedOption == 0)
      windowStart = 0;
    else if (selectedOption >= numOptions - 1)
      windowStart = numOptions - 3;
    else
      windowStart = selectedOption - 1;
  }

  int rowY[3] = {14, 31, 48};
  for (int row = 0; row < 3; row++) {
    int optionIndex = windowStart + row;
    if (optionIndex >= numOptions)
      break;

    bool isSelected = (optionIndex == selectedOption);
    if (isSelected) {
      display.fillRect(0, rowY[row], 128, 15, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    } else {
      display.drawRect(0, rowY[row], 128, 15, SH110X_WHITE);
      display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    }
    display.setCursor(4, rowY[row] + 5);
    display.print(options[optionIndex]);
  }

  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  if (windowStart > 0) {
    display.setCursor(118, 15);
    display.print("^");
  }
  if (windowStart + 3 < numOptions) {
    display.setCursor(118, 52);
    display.print("v");
  }
  display.display();
}

void MyDisplay::showStatusViewSelectonScreen() {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 11, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  display.setCursor(15, 2);
  display.print("SELECT VIEW");

  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  display.drawRect(0, 14, 128, 15, SH110X_WHITE);
  display.setCursor(4, 19);
  display.print("A: Gate Car Sensor");

  display.drawRect(0, 31, 128, 15, SH110X_WHITE);
  display.setCursor(4, 36);
  display.print("B: Pond");

  display.drawRect(0, 48, 128, 15, SH110X_WHITE);
  display.setCursor(4, 53);
  display.print("C: Fountains");
  display.display();
}

void MyDisplay::showTheIsValues(String deviceName, int maxWind,
                                String startTime, String endTime,
                                bool controledDeviceOnOrOff,
                                bool controlingOrManual) {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 11, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  display.setCursor(23, 2);
  display.print(to_upper(deviceName) + " STATUS");

  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  int y = 16, xTrue = 70, xFalse = 100;
  display.setCursor(2, y);
  display.print(deviceName + ":");

  if (controledDeviceOnOrOff) {
    display.fillRect(xTrue - 2, y - 1, 20, 9, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  } else {
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  }
  display.setCursor(xTrue + 2, y);
  display.print("ON");

  if (!controledDeviceOnOrOff) {
    display.fillRect(xFalse - 2, y - 1, 20, 9, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  } else {
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  }
  display.setCursor(xFalse, y);
  display.print("OFF");

  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  if (controlingOrManual) {
    display.fillRect(0, 32, 128, 28, SH110X_WHITE);
    display.setCursor(2, 36);
    display.setTextSize(1);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    display.print(deviceName + " operating in");
    display.setCursor(30, 46);
    display.print("Manual Mode");
  } else {
    display.setCursor(0, 32);
    display.println("Max wind: " + String(maxWind) + " hm/h");
    display.setCursor(0, 42);
    display.println("Operates: " + startTime + "-" + endTime);
  }
  display.display();
}

void MyDisplay::showIntKmhSetting(String label, int value,
                                  int highlightTarget) {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 11, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  display.setCursor(23, 2);
  display.print(to_upper(label));

  display.setTextSize(2);
  int y = 22, x = 20;
  String valueStr = String(value);
  int valueWidth = valueStr.length() * 12;

  if (highlightTarget == 1) {
    display.fillRect(x - 2, y - 2, valueWidth + 2, 18, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  } else {
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  }
  display.setCursor(x, y);
  display.print(valueStr);

  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  display.setCursor(x + valueWidth + 6, y);
  display.print("KM/H");
  display.setTextSize(1);

  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  if (highlightTarget == 2) {
    display.fillRect(0, 52, 40, 12, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  } else {
    display.drawRect(0, 52, 40, 12, SH110X_WHITE);
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  }
  display.setCursor(6, 55);
  display.print("Back");
  display.display();
}

void MyDisplay::showTimeIntervalSetting(String label, int startHour,
                                        int startMin, int endHour, int endMin,
                                        int highlightTarget) {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 11, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  display.setCursor(23, 2);
  display.print(to_upper(label));

  display.setTextSize(2);
  int y = 20;
  char buf[3];
  int boxW = 24, boxH = 18, boxY = y - 2;
  int xStartHour = 3;
  int xColon1 = xStartHour + boxW;
  int xStartMin = xColon1 + 8;
  int xDash = xStartMin + boxW;
  int xEndHour = xDash + 10;
  int xColon2 = xEndHour + boxW;
  int xEndMin = xColon2 + 8;

  sprintf(buf, "%02d", startHour);
  display.setTextColor(highlightTarget == 1 ? SH110X_BLACK : SH110X_WHITE,
                       highlightTarget == 1 ? SH110X_WHITE : SH110X_BLACK);
  if (highlightTarget == 1)
    display.fillRect(xStartHour, boxY, boxW, boxH, SH110X_WHITE);
  display.setCursor(xStartHour + 1, y);
  display.print(buf);

  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  display.setCursor(xColon1 + 2, y);
  display.print(":");

  sprintf(buf, "%02d", startMin);
  display.setTextColor(highlightTarget == 2 ? SH110X_BLACK : SH110X_WHITE,
                       highlightTarget == 2 ? SH110X_WHITE : SH110X_BLACK);
  if (highlightTarget == 2)
    display.fillRect(xStartMin, boxY, boxW, boxH, SH110X_WHITE);
  display.setCursor(xStartMin + 1, y);
  display.print(buf);

  int dashY = y + 7;
  display.drawLine(xDash + 2, dashY, xDash + 7, dashY, SH110X_WHITE);
  display.drawLine(xDash + 2, dashY + 1, xDash + 7, dashY + 1, SH110X_WHITE);

  sprintf(buf, "%02d", endHour);
  display.setTextColor(highlightTarget == 3 ? SH110X_BLACK : SH110X_WHITE,
                       highlightTarget == 3 ? SH110X_WHITE : SH110X_BLACK);
  if (highlightTarget == 3)
    display.fillRect(xEndHour, boxY, boxW, boxH, SH110X_WHITE);
  display.setCursor(xEndHour + 1, y);
  display.print(buf);

  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  display.setCursor(xColon2 + 2, y);
  display.print(":");

  sprintf(buf, "%02d", endMin);
  display.setTextColor(highlightTarget == 4 ? SH110X_BLACK : SH110X_WHITE,
                       highlightTarget == 4 ? SH110X_WHITE : SH110X_BLACK);
  if (highlightTarget == 4)
    display.fillRect(xEndMin, boxY, boxW, boxH, SH110X_WHITE);
  display.setCursor(xEndMin + 1, y);
  display.print(buf);

  display.setTextSize(1);
  if (highlightTarget == 5) {
    display.fillRect(0, 52, 40, 12, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  } else {
    display.drawRect(0, 52, 40, 12, SH110X_WHITE);
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  }
  display.setCursor(6, 55);
  display.print("Back");
  display.display();
}

// ==========================================
// InfoController Implementation
// ==========================================
InfoController::InfoController(MyDisplay &disp) : display(disp) {}

void InfoController::handleInput(uint8_t buttons) {
  if (buttons == 0)
    return;
  switch (screen) {
  case INFO_DEFAULT:
    screen = INFO_SELECT_DEVICE;
    break;
  case INFO_SELECT_DEVICE:
    if (buttons & 0x1) {
      selectedDevice = DEVICE_GATE;
      screen = INFO_SHOW_DEVICE;
    } else if (buttons & 0x2) {
      selectedDevice = DEVICE_POND;
      screen = INFO_SHOW_DEVICE;
    } else if (buttons & 0x4) {
      selectedDevice = DEVICE_FOUNTAINS;
      screen = INFO_SHOW_DEVICE;
    }
    break;
  case INFO_SHOW_DEVICE:
    screen = INFO_DEFAULT;
    break;
  }
}

void InfoController::render() {
  switch (screen) {
  case INFO_DEFAULT: {
    CurrentConditions c;
    getCurrentConditions(c);
    display.showCurrentInformation(c.windSpeed, c.temperature, c.hours,
                                   c.minutes);
    break;
  }
  case INFO_SELECT_DEVICE:
    display.showStatusViewSelectonScreen();
    break;
  case INFO_SHOW_DEVICE: {
    DeviceSettings d;
    getDeviceSettings(selectedDevice, d);
    bool onOrOff = (d.mode != MODE_OFF);
    bool manual = (d.mode == MODE_MANUAL_ON);
    char startBuf[6], endBuf[6];
    sprintf(startBuf, "%02d:%02d", d.startHour, d.startMin);
    sprintf(endBuf, "%02d:%02d", d.endHour, d.endMin);
    display.showTheIsValues(String(d.name), d.windLimit, String(startBuf),
                            String(endBuf), onOrOff, manual);
    break;
  }
  }
}

void InfoController::resetToDefault() { screen = INFO_DEFAULT; }

// ==========================================
// MenuController Implementation
// ==========================================
MenuController::MenuController(MyDisplay &disp) : display(disp) {}

void MenuController::reset() {
  screen = MENU_SELECT_DEVICE;
  listSelection = 0;
  editTarget = 1;
  isEditingField = false;
  exitRequested = false;
}

bool MenuController::wantsExit() { return exitRequested; }

void MenuController::handleInput(uint8_t buttons) {
  if (buttons == 0)
    return;
  switch (screen) {
  case MENU_SELECT_DEVICE:
    if (navigateList(buttons, 4)) {
      if (listSelection == 3) {
        exitRequested = true;
      } else {
        selectedDevice = (DeviceID)listSelection;
        getDeviceSettings(selectedDevice, editBuffer);
        screen = MENU_DEVICE_OPTIONS;
        listSelection = 0;
      }
    }
    break;
  case MENU_DEVICE_OPTIONS:
    if (navigateList(buttons, 4)) {
      switch (listSelection) {
      case 0:
        screen = MENU_EDIT_SCHEDULE;
        editTarget = 1;
        isEditingField = false;
        break;
      case 1:
        screen = MENU_EDIT_WIND;
        editTarget = 1;
        isEditingField = false;
        break;
      case 2:
        screen = MENU_MODE_SELECT;
        listSelection = (int)editBuffer.mode;
        break;
      case 3:
        screen = MENU_SELECT_DEVICE;
        listSelection = (int)selectedDevice;
        break;
      }
    }
    break;
  case MENU_MODE_SELECT:
    if (navigateList(buttons, 4)) {
      if (listSelection != 3) {
        editBuffer.mode = (DeviceMode)listSelection;
        setDeviceSettings(selectedDevice, editBuffer);
      }
      screen = MENU_DEVICE_OPTIONS;
      listSelection = 2;
    }
    break;
  case MENU_EDIT_WIND:
    handleWindEditInput(buttons);
    break;
  case MENU_EDIT_SCHEDULE:
    handleScheduleEditInput(buttons);
    break;
  }
}

void MenuController::render() {
  updateBlink();
  switch (screen) {
  case MENU_SELECT_DEVICE:
    display.showSettingsMenu(deviceListOptions, 4, listSelection);
    break;
  case MENU_DEVICE_OPTIONS:
    display.showSettingsMenu(deviceMenuOptions, 4, listSelection);
    break;
  case MENU_MODE_SELECT:
    display.showSettingsMenu(modeSelectOptions, 4, listSelection);
    break;
  case MENU_EDIT_WIND:
    display.showIntKmhSetting(String(editBuffer.name), editBuffer.windLimit,
                              currentBlinkTarget());
    break;
  case MENU_EDIT_SCHEDULE:
    display.showTimeIntervalSetting(
        String(editBuffer.name), editBuffer.startHour, editBuffer.startMin,
        editBuffer.endHour, editBuffer.endMin, currentBlinkTarget());
    break;
  }
}

bool MenuController::navigateList(uint8_t buttons, int numOptions) {
  if (buttons & 0x1)
    listSelection = (listSelection - 1 + numOptions) % numOptions;
  else if (buttons & 0x4)
    listSelection = (listSelection + 1) % numOptions;
  else if (buttons & 0x2)
    return true;
  return false;
}

void MenuController::handleWindEditInput(uint8_t buttons) {
  if (isEditingField) {
    if (buttons & 0x4)
      editBuffer.windLimit = max(0, editBuffer.windLimit - 1);
    else if (buttons & 0x1)
      editBuffer.windLimit = min(99, editBuffer.windLimit + 1);
    else if (buttons & 0x2) {
      isEditingField = false;
      setDeviceSettings(selectedDevice, editBuffer);
    }
    return;
  }
  if (buttons & (0x1 | 0x4))
    editTarget = (editTarget == 1) ? 2 : 1;
  else if (buttons & 0x2) {
    if (editTarget == 2) {
      screen = MENU_DEVICE_OPTIONS;
      listSelection = 1;
    } else
      isEditingField = true;
  }
}

void MenuController::handleScheduleEditInput(uint8_t buttons) {
  if (isEditingField) {
    int delta = 0;
    if (buttons & 0x4)
      delta = -1;
    else if (buttons & 0x1)
      delta = 1;
    else if (buttons & 0x2) {
      isEditingField = false;
      setDeviceSettings(selectedDevice, editBuffer);
      return;
    }

    if (delta != 0) {
      switch (editTarget) {
      case 1:
        editBuffer.startHour = (editBuffer.startHour + delta + 24) % 24;
        break;
      case 2:
        editBuffer.startMin = (editBuffer.startMin + delta + 60) % 60;
        break;
      case 3:
        editBuffer.endHour = (editBuffer.endHour + delta + 24) % 24;
        break;
      case 4:
        editBuffer.endMin = (editBuffer.endMin + delta + 60) % 60;
        break;
      }
    }
    return;
  }
  if (buttons & 0x1)
    editTarget = (editTarget == 1) ? 5 : editTarget - 1;
  else if (buttons & 0x4)
    editTarget = (editTarget == 5) ? 1 : editTarget + 1;
  else if (buttons & 0x2) {
    if (editTarget == 5) {
      screen = MENU_DEVICE_OPTIONS;
      listSelection = 0;
    } else
      isEditingField = true;
  }
}

void MenuController::updateBlink() {
  if (millis() - lastBlinkToggle > blinkIntervalMs) {
    blinkVisible = !blinkVisible;
    lastBlinkToggle = millis();
  }
}

int MenuController::currentBlinkTarget() {
  if (isEditingField && !blinkVisible)
    return 0;
  return editTarget;
}

// ==========================================
// App Implementation
// ==========================================
App::App() : infoController(display), menuController(display) {}

void App::init() { display.init(); }

void App::loop() {
  uint8_t buttons = button.readButtonsOnClick();
  if (buttons != 0)
    lastInputTime = millis(); // time at button pressed

  handleModeEntry(buttons);

  // if there is no input for longer then the timeout
  // (e.g. 8000ms, equivalent to 8 seconds) then return
  // to initial screen
  if (currentMode == MODE_INFO && (millis() - lastInputTime > infoTimeoutMs)) {
    infoController.resetToDefault();
  }

  switch (currentMode) {
  case MODE_INFO:
    infoController.render();
    break;
  case MODE_MENU:
    menuController.render();
    break;
  }
  delay(10);
}

void App::handleModeEntry(uint8_t buttons) {
  if (buttons != 0 && pendingButtons == 0) {
    pendingButtons = buttons;
    pendingSince = millis();
    return;
  }
  if (buttons != 0 && (millis() - pendingSince) < comboWindowMs) {
    pendingButtons |= buttons;
    return;
  }
  if (buttons == 0 && pendingButtons == 0)
    return;

  uint8_t buttonCombination = pendingButtons | buttons;
  pendingButtons = 0;
  if (buttonCombination == 0)
    return;

  if (buttonCombination == 0b011) {
    currentMode = MODE_MENU;
    menuController.reset();
    return;
  }

  switch (currentMode) {
  // defult view: info about CURRENT CONDITIONS, SELECT VIEW, DEVICE STATUS
  case MODE_INFO:
    infoController.handleInput(buttonCombination);
    break;

  // SETTINGS menu
  case MODE_MENU:
    menuController.handleInput(buttonCombination);
    if (menuController.wantsExit()) {
      currentMode = MODE_INFO;
    }
    break;
  }
}

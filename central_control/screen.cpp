#include "screen.h"

#include "RadioManager.h"
#include "SystemData.h"
#include "api/String.h"

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
    buttons |= BTN_A;
  if (digitalRead(BUTTON_B) == 0)
    buttons |= BTN_B;
  if (digitalRead(BUTTON_C) == 0)
    buttons |= BTN_C;
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

void MyDisplay::showCurrentInformation(int windSpeed, float temperature,
                                       int hours, int minutes) {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 11, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  display.setCursor(23, 2);
  display.print("SYSTEM STATUS");

  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  display.setCursor(2, 16);
  display.print("Wind Speed: " + String(windSpeed) + " KM/H");

  display.setCursor(2, 29);
  display.print("Temp: " + String(temperature, 1) + " C");

  display.setCursor(2, 42);
  char timeBuf[6];
  sprintf(timeBuf, "%02d:%02d", hours, minutes);
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

void MyDisplay::showStatusViewSelectionScreen() {
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

void MyDisplay::showTheIsValues(const char *deviceNameChar, int maxWind,
                                uint8_t startHour, uint8_t startMin,
                                uint8_t endHour, uint8_t endMin,
                                DeviceMode off_auto_manual,
                                RelayState relayState) {
  String deviceName = String(deviceNameChar);
  display.clearDisplay();
  display.fillRect(0, 0, 128, 11, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  display.setCursor(23, 2);
  display.print(to_upper(deviceName) + " STATUS");

  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  int y = 16, xTrue = 70, xFalse = 100;
  display.setCursor(2, y);
  display.print(deviceName + ":");

  if (relayState == RELAY_UNKNOWN) {
    // Node has not reported a state yet (e.g. latching relay before the
    // first command) — show a placeholder instead of guessing.
    display.setTextSize(2);
    display.setCursor(88, y - 4);
    display.print("?");
    display.setTextSize(1);
  } else if (relayState == RELAY_ON) {
    display.fillRect(xTrue - 2, y - 1, 20, 9, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    display.setCursor(xTrue + 2, y);
    display.print("ON");

    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    display.setCursor(xFalse, y);
    display.print("OFF");
  } else {
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    display.setCursor(xTrue + 2, y);
    display.print("ON");

    display.fillRect(xFalse - 2, y - 1, 20, 9, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    display.setCursor(xFalse, y);
    display.print("OFF");
  }

  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  if (off_auto_manual == MODE_OFF || off_auto_manual == MODE_MANUAL_ON) {
    display.fillRect(0, 32, 128, 28, SH110X_WHITE);
    display.setCursor(2, 36);
    display.setTextSize(1);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    display.print(deviceName + " operating in");
    display.setCursor(30, 46);
    display.print(off_auto_manual == MODE_OFF ? "Manual Off" : "Manual On");
  } else {
    display.setCursor(0, 32);
    display.println("Max wind: " + String(maxWind) + " km/h");
    display.setCursor(0, 42);

    String startTime = (startHour < 10 ? "0" : "") + String(startHour) + ":" +
                       (startMin < 10 ? "0" : "") + String(startMin);
    String endTime = (endHour < 10 ? "0" : "") + String(endHour) + ":" +
                     (endMin < 10 ? "0" : "") + String(endMin);

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

  // Large value display
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

  // Back button (highlightTarget == 2)
  if (highlightTarget == 2) {
    display.fillRect(0, 52, 40, 12, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  } else {
    display.drawRect(0, 52, 40, 12, SH110X_WHITE);
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  }
  display.setCursor(6, 54);
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

void MyDisplay::showErrorBanner(const char *message) {
  // Inverted bar across the bottom of the screen; drawn OVER whatever the
  // current screen shows, so callers must redraw the underlying screen when
  // the banner expires (drawCurrentScreen() already does this).
  display.fillRect(0, 52, 128, 12, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(4, 55);
  display.print(message);
  display.display();
}

// ==========================================
// InfoController Implementation
// ==========================================

InfoController::InfoController(MyDisplay &disp) : display(disp) {}

void InfoController::drawDefaultScreen() {
  CurrentConditions c;
  getCurrentConditions(c);
  display.showCurrentInformation(c.windSpeed, c.temperature, c.hours,
                                 c.minutes);
}

void InfoController::drawDeviceStatus() {
  DeviceSettings deviceSettings;
  getDeviceSettings(selectedDevice, deviceSettings);

  // Show the relay state as last reported by the remote node itself,
  // not the locally computed desired state.
  RelayState relayState = RELAY_UNKNOWN;
  switch (selectedDevice) {
  case DEVICE_POND: {
    PondNodeStatus pond;
    radioManager.getPondNodeStatus(pond);
    relayState = pond.pumpState;
    break;
  }
  case DEVICE_FOUNTAINS: {
    NodeStatus fountain1, fountain2;
    radioManager.getFountainStatus(0, fountain1);
    radioManager.getFountainStatus(1, fountain2);
    if (fountain1.relayState == RELAY_ON || fountain2.relayState == RELAY_ON)
      relayState = RELAY_ON; // any running counts as on
    else if (fountain1.relayState == RELAY_OFF &&
             fountain2.relayState == RELAY_OFF)
      relayState = RELAY_OFF;
    else
      relayState = RELAY_UNKNOWN;
    break;
  }
  case DEVICE_GATE:
  default: {
    NodeStatus gate;
    radioManager.getGateStatus(gate);
    relayState = gate.relayState;
    break;
  }
  }

  display.showTheIsValues(deviceSettings.name, deviceSettings.windLimit,
                          deviceSettings.startHour, deviceSettings.startMin,
                          deviceSettings.endHour, deviceSettings.endMin,
                          deviceSettings.mode, relayState);
}

void InfoController::handleInput(uint8_t buttons) {
  if (buttons == 0)
    return;

  switch (screen) {
  case INFO_DEFAULT:
    screen = INFO_SELECT_DEVICE;
    display.showStatusViewSelectionScreen();
    break;

  case INFO_SELECT_DEVICE:
    switch (buttons) {
    case BTN_A:
      selectedDevice = DEVICE_GATE;
      break;
    case BTN_B:
      selectedDevice = DEVICE_POND;
      break;
    case BTN_C:
      selectedDevice = DEVICE_FOUNTAINS;
      break;
    default:
      return; // ignore multi-button combos on this screen
    }
    screen = INFO_SHOW_DEVICE;
    drawDeviceStatus();
    break;

  case INFO_SHOW_DEVICE:
    screen = INFO_DEFAULT;
    drawDefaultScreen();
    break;
  }
}

void InfoController::resetToDefault() {
  screen = INFO_DEFAULT;
  drawDefaultScreen();
}

// ==========================================
// MenuController Implementation
// ==========================================

MenuController::MenuController(MyDisplay &disp) : display(disp) {}

void MenuController::drawCurrentScreen() {
  switch (screen) {
  case MENU_SELECT_DEVICE:
    display.showSettingsMenu(deviceListOptions, 4, listSelection);
    break;
  case MENU_DEVICE_OPTIONS:
    display.showSettingsMenu(deviceMenuOptions, 4, listSelection);
    // Error banner after saving an invalid (start == end) schedule.
    if (scheduleInvalid) {
      if (millis() - invalidSinceMs < INVALID_BANNER_MS) {
        display.showErrorBanner("START=END! USE MANUAL");
      } else {
        scheduleInvalid = false; // banner expired
      }
    }
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

void MenuController::reset() {
  screen = MENU_SELECT_DEVICE;
  listSelection = 0;
  editTarget = 1;
  isEditingField = false;
  exitRequested = false;
  drawCurrentScreen();
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
        return;
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

  // Draw immediately on state change
  drawCurrentScreen();
}

bool MenuController::navigateList(uint8_t buttons, int numOptions) {
  switch (static_cast<ButtonState>(buttons)) {
  case BTN_A: // Up
    listSelection = (listSelection - 1 + numOptions) % numOptions;
    break;
  case BTN_B: // Select
    return true;
  case BTN_C: // Down
    listSelection = (listSelection + 1) % numOptions;
    break;
  default:
    break;
  }
  return false;
}

void MenuController::handleWindEditInput(uint8_t buttons) {
  if (isEditingField) {
    switch (static_cast<ButtonState>(buttons)) {
    case BTN_A: // Increment
      editBuffer.windLimit = min(99, editBuffer.windLimit + 1);
      break;
    case BTN_B: // Confirm/exit edit
      isEditingField = false;
      setDeviceSettings(selectedDevice, editBuffer);
      break;
    case BTN_C: // Decrement
      editBuffer.windLimit = max(0, editBuffer.windLimit - 1);
      break;
    default:
      break;
    }
    return;
  }

  switch (static_cast<ButtonState>(buttons)) {
  case BTN_A:
  case BTN_C: // Toggle between value and Back
    editTarget = (editTarget == 1) ? 2 : 1;
    break;
  case BTN_B: // Enter edit or confirm
    if (editTarget == 2) {
      screen = MENU_DEVICE_OPTIONS;
      listSelection = 1;
    } else {
      isEditingField = true;
    }
    break;
  default:
    break;
  }
}

void MenuController::handleScheduleEditInput(uint8_t buttons) {
  if (isEditingField) {
    int delta = 0;

    switch (static_cast<ButtonState>(buttons)) {
    case BTN_A: // Increment
      delta = 1;
      break;
    case BTN_B: // Confirm/exit edit
      isEditingField = false;
      setDeviceSettings(selectedDevice, editBuffer);
      return;
    case BTN_C: // Decrement
      delta = -1;
      break;
    default:
      break;
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

  switch (static_cast<ButtonState>(buttons)) {
  case BTN_A: // Previous field
    editTarget = (editTarget == 1) ? 5 : editTarget - 1;
    break;
  case BTN_B: { // Enter edit or back out
    if (editTarget == 5) {
      // Reject a start time equal to the end time: isWithinSchedule()
      // treats it as an empty window (always OFF), which is almost never
      // what the user means — MODE_MANUAL_ON exists for "always on".
      if (editBuffer.startHour == editBuffer.endHour &&
          editBuffer.startMin == editBuffer.endMin) {
        scheduleInvalid = true;
        invalidSinceMs = millis();
      } else {
        scheduleInvalid = false;
      }
      screen = MENU_DEVICE_OPTIONS;
      listSelection = 0;
    } else {
      isEditingField = true;
    }
    break;
  }
  case BTN_C: // Next field
    editTarget = (editTarget == 5) ? 1 : editTarget + 1;
    break;
  default:
    break;
  }
}

void MenuController::updateBlink() {
  if (millis() - lastBlinkToggle > blinkIntervalMs) {
    blinkVisible = !blinkVisible;
    lastBlinkToggle = millis();
    if (isEditingField) {
      drawCurrentScreen();
    }
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

void App::init() {
  display.init();
  infoController
      .resetToDefault(); // Render initial state on hardware boot
}

void App::loop() {
  uint8_t buttons = button.readButtonsOnClick();

  if (buttons != 0) {
    lastInputTime = millis();
  }

  // 1. INPUT AND MODE TRANSITION DISPATCH (Triggers immediate drawing)
  handleModeEntry(buttons);

  // 2. INACTIVITY TIMEOUT RESET
  if (currentMode == MODE_INFO && (millis() - lastInputTime > infoTimeoutMs)) {
    if (!infoController.isDefaultScreen()) {
      infoController
          .resetToDefault(); // Draws default view immediately
    }
  }

  // 2b. MENU INACTIVITY TIMEOUT RESET (discards unsaved edits)
  if (currentMode == MODE_MENU && (millis() - lastInputTime > menuTimeoutMs)) {
    currentMode = MODE_INFO;
    infoController.resetToDefault();
  }

  // 3. PERIODIC REFRESH FOR LIVE DATA (500ms ticker)
  static unsigned long lastPeriodicRefresh = 0;
  if (millis() - lastPeriodicRefresh >= 500) {
    lastPeriodicRefresh = millis();
    if (currentMode == MODE_INFO && infoController.isDefaultScreen()) {
      infoController.drawDefaultScreen(); // Updates live clock/wind view
                                          // immediately
    }
  }

  // 4. INLINE EDIT BLINKING TICKER
  if (currentMode == MODE_MENU) {
    menuController.updateBlink();
  }

  delay(10);
}

void App::handleModeEntry(uint8_t buttons) {
  if (buttons != BTN_NONE && pendingButtons == BTN_NONE) {
    pendingButtons = buttons;
    pendingSince = millis();
    return;
  }

  if (buttons != BTN_NONE && (millis() - pendingSince) < comboWindowMs) {
    pendingButtons |= buttons;
    return;
  }

  if (buttons == BTN_NONE && pendingButtons == BTN_NONE)
    return;

  uint8_t buttonCombination = pendingButtons | buttons;
  pendingButtons = BTN_NONE;

  if (buttonCombination == BTN_NONE)
    return;

  // Combo entry to menu mode
  if (buttonCombination == BTN_MENU_COMBO) {
    currentMode = MODE_MENU;
    menuController.reset(); // Renders menu root screen directly
    return;
  }

  switch (currentMode) {
  case MODE_INFO:
    infoController.handleInput(buttonCombination);
    break;

  case MODE_MENU:
    menuController.handleInput(buttonCombination);
    if (menuController.wantsExit()) {
      currentMode = MODE_INFO;
      infoController
          .resetToDefault(); // Renders default info view directly
    }
    break;
  }
}

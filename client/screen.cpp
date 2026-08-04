#include "screen.h"

#include "RadioManager.h"
#include "SystemData.h"
#include "api/String.h"

// Helper: convert a String to uppercase for UI headers.
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

// Read raw button states into a bitmask.
// Uses INPUT_PULLUP wiring, so a pressed button reads 0.
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

// Debounced button reader: returns only buttons that transitioned
// from up -> pressed since the last stable reading.
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
  display.begin(0x3C, true); // I2C address 0x3C, reset= true
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

  // Scroll window: keep the selected row visible in a 3-row viewport.
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

  // Three device-selection tiles (A=Gate, B=Pond, C=Fountains)
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
                                bool controledDeviceOnOrOff) {
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

  if (controledDeviceOnOrOff) {
    // Draw "ON" with a white background highlight
    display.fillRect(xTrue - 2, y - 1, 20, 9, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    display.setCursor(xTrue + 2, y);
    display.print("ON");

    // Draw "OFF" with a black background (no highlight)
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    display.setCursor(xFalse, y);
    display.print("OFF");
  } else {
    // Draw "ON" with a black background (no highlight)
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    display.setCursor(xTrue + 2, y);
    display.print("ON");

    // Draw "OFF" with a white background highlight
    display.fillRect(xFalse - 2, y - 1, 20, 9, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    display.setCursor(xFalse, y);
    display.print("OFF");
  }

  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  // Manual mode banner vs. schedule summary
  if (off_auto_manual == MODE_OFF || off_auto_manual == MODE_MANUAL_ON) {
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

  // Back button
  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  if (highlightTarget == 2) {
    display.fillRect(0, 52, 40, 12, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  } else {
    display.drawRect(0, 52, 40, 12, SH110X_WHITE);
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  }
}
void MyDisplay::showTimeIntervalSetting(String label, int startHour,
                                        int startMin, int endHour, int endMin,
                                        int highlightTarget) {
  display.clearDisplay();
  display.fillRect(0, 0, 128, 11, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK, SH110X_WHITE);
  display.setCursor(23, 2);
  display.print(to_upper(label));

  // Time fields: HH : MM — dash — HH : MM
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

  // Start hour (field 1)
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

  // Start minute (field 2)
  sprintf(buf, "%02d", startMin);
  display.setTextColor(highlightTarget == 2 ? SH110X_BLACK : SH110X_WHITE,
                       highlightTarget == 2 ? SH110X_WHITE : SH110X_BLACK);
  if (highlightTarget == 2)
    display.fillRect(xStartMin, boxY, boxW, boxH, SH110X_WHITE);
  display.setCursor(xStartMin + 1, y);
  display.print(buf);

  // Dash separator
  int dashY = y + 7;
  display.drawLine(xDash + 2, dashY, xDash + 7, dashY, SH110X_WHITE);
  display.drawLine(xDash + 2, dashY + 1, xDash + 7, dashY + 1, SH110X_WHITE);

  // End hour (field 3)
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

  // End minute (field 4)
  sprintf(buf, "%02d", endMin);
  display.setTextColor(highlightTarget == 4 ? SH110X_BLACK : SH110X_WHITE,
                       highlightTarget == 4 ? SH110X_WHITE : SH110X_BLACK);
  if (highlightTarget == 4)
    display.fillRect(xEndMin, boxY, boxW, boxH, SH110X_WHITE);
  display.setCursor(xEndMin + 1, y);
  display.print(buf);

  display.setTextSize(1);

  // Back button (field 5)
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
    // Button bits: 0b001 = device A (Gate), 0b010 = B (Pond), 0b100 = C
    // (Fountains)
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
      // Handle no buttons or unexpected values
      break;
    }
    screen = INFO_SHOW_DEVICE;
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
    DeviceSettings deviceSttings;
    CurrentConditions currentConditions;
    getCurrentConditions(currentConditions);
    getDeviceSettings(selectedDevice, deviceSttings);

    bool controledDeviceOnOrOff = RadioManager::computeDesiredState(
        deviceSttings, false, currentConditions.windSpeed,
        currentConditions.hours, currentConditions.minutes);

    display.showTheIsValues(deviceSttings.name, deviceSttings.windLimit,
                            deviceSttings.startHour, deviceSttings.startMin,
                            deviceSttings.endHour, deviceSttings.endMin,
                            deviceSttings.mode, controledDeviceOnOrOff);

    // display.showTheIsValues(String(d.name), d.windLimit,
    // String(startBuf),
    //                       String(endBuf), onOrOff, manual);
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

// Navigate a 4-row menu list with buttons: B = up, C = down, A = select.
// Returns true on select.
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

// Edit the wind-speed limit while in an active field, or toggle between
// the value and the Back button.
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

  // Not actively editing the value
  switch (static_cast<ButtonState>(buttons)) {
  case BTN_A: // Toggle between value and Back
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
  case BTN_C: // Toggle between value and Back
    editTarget = (editTarget == 1) ? 2 : 1;
    break;

  default:
    break;
  }
}

// Edit schedule fields: hours/minutes start/end + Back button.
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

  // Not actively editing a field
  switch (static_cast<ButtonState>(buttons)) {
  case BTN_A: // Previous field
    editTarget = (editTarget == 1) ? 5 : editTarget - 1;
    break;
  case BTN_B: // Enter edit or back out
    if (editTarget == 5) {
      screen = MENU_DEVICE_OPTIONS;
      listSelection = 0;
    } else {
      isEditingField = true;
    }
    break;
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
  bool needsRedraw = false;

  // 1. INSTANT USER INPUT HANDLING
  if (buttons != 0) {
    lastInputTime = millis(); // Reset inactivity timer
    needsRedraw = true;       // Flag immediate render for button presses!
  }

  // Handle mode transitions or menu/combo inputs
  handleModeEntry(buttons);

  // 2. TIMEOUT INACTIVITY RESET
  if (currentMode == MODE_INFO && (millis() - lastInputTime > infoTimeoutMs)) {
    // Only flag redraw if we were on a sub-screen
    infoController.resetToDefault();
    needsRedraw = true;
  }

  // 3. PERIODIC REFRESH FOR LIVE DATA (500ms = smooth 2Hz clock/wind updates)
  static unsigned long lastPeriodicRefresh = 0;
  if (millis() - lastPeriodicRefresh >= 500) {
    lastPeriodicRefresh = millis();
    needsRedraw = true;
  }

  // 4. RENDER ONLY WHEN DIRTY
  if (needsRedraw) {
    switch (currentMode) {
    case MODE_INFO:
      infoController.render();
      break;
    case MODE_MENU:
      menuController.render();
      break;
    }
  }

  delay(10); // Small pause to prevent aggressive CPU spinning
}
// void App::loop() {
//   uint8_t buttons = button.readButtonsOnClick();
//   if (buttons != 0)
//     lastInputTime = millis(); // reset inactivity timer on any press
//
//   handleModeEntry(buttons);
//
//   // Auto-return from menu after 8s of inactivity
//   if (currentMode == MODE_INFO && (millis() - lastInputTime > infoTimeoutMs))
//   {
//     infoController.resetToDefault();
//   }
//
//   switch (currentMode) {
//   case MODE_INFO:
//     infoController.render();
//     break;
//   case MODE_MENU:
//     menuController.render();
//     break;
//   }
//   delay(10);
// }

// Detect the A+B combo (0b011) to enter menu mode; otherwise treat as
// normal single-button input for the current mode.
void App::handleModeEntry(uint8_t buttons) {
  // Replace 0 with BTN_NONE
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

  // A+B combo enters settings menu (replaces 0b011 with BTN_MENU_COMBO)
  if (buttonCombination == BTN_MENU_COMBO) {
    currentMode = MODE_MENU;
    menuController.reset();
    return;
  }

  switch (currentMode) {
  // Default view: info about CURRENT CONDITIONS, SELECT VIEW, DEVICE STATUS
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

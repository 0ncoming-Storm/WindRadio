#include "SerialUSB.h"
#include "WindRadioCommon.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define MYNODEID 2
#define TONODEID 1

bool currentStateG = true;
bool manual_modeG = false;
class MyDisplay {
private:
  Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);

  uint8_t readButtons(void) {
    uint8_t buttons = 0;

    /* Button A — bit 0 */
    if (digitalRead(BUTTON_A) == 0) {
      buttons |= 0x1;
    }

    /* Button B — bit 1 */
    if (digitalRead(BUTTON_B) == 0) {
      buttons |= 0x2;
    }

    /* Button C — bit 2 */
    if (digitalRead(BUTTON_C) == 0) {
      buttons |= 0x4;
    }

    return buttons;
  }

public:
  static constexpr uint8_t BUTTON_A = 9;
  static constexpr uint8_t BUTTON_B = 6;
  static constexpr uint8_t BUTTON_C = 5;
  MyDisplay() {} // constructor does nothing hardware-related

  void init() {
    display.begin(0x3C, true);
    display.clearDisplay();
    display.display();
    display.setRotation(3);
    pinMode(BUTTON_A, INPUT_PULLUP);
    pinMode(BUTTON_B, INPUT_PULLUP);
    pinMode(BUTTON_C, INPUT_PULLUP);
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  }
  void showCurrentInformation(int windSpeed, float tempriture, int hours,
                              int minuits) {
    display.clearDisplay();

    // -- Title bar
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    display.setCursor(23, 2);
    display.print("SYSTEM STATUS");

    // -- Wind speed
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    display.setCursor(2, 16);
    display.print("Wind Speed: " + String(windSpeed) + " KM/H");

    // -- Temperature
    display.setCursor(2, 29);
    display.print("Temp: " + String(tempriture, 1) + " C");

    // -- Current time
    display.setCursor(2, 42);
    char timeBuf[6];
    sprintf(timeBuf, "%02d:%02d", hours, minuits);
    display.print("Time: " + String(timeBuf));

    display.display();
  }
  void showSettingsMenu(String options[], int numOptions, int selectedOption) {
    display.clearDisplay();

    // -- Title bar
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    display.setCursor(30, 2);
    display.print("SETTINGS");

    // -- Figure out the scroll window (3 visible rows)
    int windowStart = 0;
    if (numOptions > 3) {
      if (selectedOption == 0) {
        windowStart = 0;
      } else if (selectedOption >= numOptions - 1) {
        windowStart = numOptions - 3;
      } else {
        windowStart = selectedOption - 1; // keep selection in the middle row
      }
    }

    // -- Draw the 3 visible rows
    int rowY[3] = {14, 31, 48};
    for (int row = 0; row < 3; row++) {
      int optionIndex = windowStart + row;
      if (optionIndex >= numOptions)
        break; // fewer than 3 options total

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

    // -- Scroll indicators (drawn over the top-right/bottom-right corners)
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    if (windowStart > 0) {
      display.setCursor(118, 15); // up arrow area, near top row
      display.print("^");
    }
    if (windowStart + 3 < numOptions) {
      display.setCursor(118, 52); // down arrow area, near bottom row
      display.print("v");
    }

    display.display();
  }
  void showStatusViewSelectonScreen() {
    display.clearDisplay();

    // -- Title bar
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    display.setCursor(15, 2);
    display.print("SELECT VIEW");

    display.setTextColor(SH110X_WHITE, SH110X_BLACK);

    // -- Option A: Gate Car Sensor
    display.drawRect(0, 14, 128, 15, SH110X_WHITE);
    display.setCursor(4, 19);
    display.print("A: Gate Car Sensor");

    // -- Option B: Pond
    display.drawRect(0, 31, 128, 15, SH110X_WHITE);
    display.setCursor(4, 36);
    display.print("B: Pond");

    // -- Option C: Fountains
    display.drawRect(0, 48, 128, 15, SH110X_WHITE);
    display.setCursor(4, 53);
    display.print("C: Fountains");

    display.display();
  }

  void showTheIsValues(String deviceName, int maxWind, String startTime,
                       String endTime, bool controledDeviceOnOrOff,
                       bool controlingOrManual) {
    display.clearDisplay();
    // -- Line 0: Title
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);

    display.setCursor(23, 2);
    display.print(to_upper(deviceName) + " STATUS");
    // ---- Line 1: True / False toggle with highlight ----

    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    int y = 16;
    int xTrue = 70;
    int xFalse = 100; // adjust spacing based on your text size/rotation
    display.setCursor(2, y);
    display.print(deviceName + ":");
    // "True" option
    if (controledDeviceOnOrOff) {
      // Selected: white box behind black text
      display.fillRect(xTrue - 2, y - 1, 20, 9, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    } else {
      display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    }
    display.setCursor(xTrue + 2, y);
    display.print("ON");

    // "False" option
    if (!controledDeviceOnOrOff) {
      display.fillRect(xFalse - 2, y - 1, 20, 9, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    } else {
      display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    }
    display.setCursor(xFalse, y);
    display.print("OFF");

    // ---- Reset to normal white-on-black for body text ----
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
      // ---- Line 2 ----
      display.setCursor(0, 32);
      display.println("Max wind: " + String(maxWind) + " hm/h");
      //
      // // ---- Line 3 ----
      display.setCursor(0, 42);
      display.println("Operates: " + startTime + "-" + endTime);
    }

    display.display();
  }

  // highlightTarget: 0 = none, 1 = value, 2 = back
  void showIntKmhSetting(String label, int value, int highlightTarget) {
    display.clearDisplay();

    // -- Title bar
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    display.setCursor(23, 2);
    display.print(to_upper(label));

    // -- Value + unit display
    display.setTextSize(2);
    int y = 22;
    int x = 20;

    String valueStr = String(value);
    int valueWidth = valueStr.length() * 12; // ~12px per char at size 2

    // Highlight box sized to just the number
    if (highlightTarget == 1) {
      display.fillRect(x - 2, y - 2, valueWidth + 2, 18, SH110X_WHITE);
      display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    } else {
      display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    }
    display.setCursor(x, y);
    display.print(valueStr);

    // "KM/H" always drawn in normal (non-highlighted) style
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    display.setCursor(x + valueWidth + 6, y);
    display.print("KM/H");

    display.setTextSize(1);

    // -- Back option
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

  // highlightTarget: 0 = none, 1 = start hour, 2 = start min, 3 = end hour, 4 =
  // end min, 5 = back
  void showTimeIntervalSetting(String label, int startHour, int startMin,
                               int endHour, int endMin, int highlightTarget) {
    display.clearDisplay();

    // -- Title bar
    display.fillRect(0, 0, 128, 11, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK, SH110X_WHITE);
    display.setCursor(23, 2);
    display.print(to_upper(label));

    // -- Time interval row: xx:xx-xx:xx
    display.setTextSize(2);
    int y = 20;
    char buf[3];

    // --- Layout Constants for 128px Width ---
    int boxW = 24; // Box width
    int boxH = 18; // Box height
    int boxY = y - 2;

    // Calculate perfect X coordinates for every element
    int xStartHour = 3;              // Start with a 3px margin
    int xColon1 = xStartHour + boxW; // 27 (8px gap)
    int xStartMin = xColon1 + 8;     // 35
    int xDash = xStartMin + boxW;    // 59 (10px gap)
    int xEndHour = xDash + 10;       // 69
    int xColon2 = xEndHour + boxW;   // 93 (8px gap)
    int xEndMin = xColon2 + 8;       // 101

    // Start hour
    sprintf(buf, "%02d", startHour);
    display.setTextColor(highlightTarget == 1 ? SH110X_BLACK : SH110X_WHITE,
                         highlightTarget == 1 ? SH110X_WHITE : SH110X_BLACK);
    if (highlightTarget == 1)
      display.fillRect(xStartHour, boxY, boxW, boxH, SH110X_WHITE);
    display.setCursor(xStartHour + 1,
                      y); // +1 centers the 22px text in the 24px box
    display.print(buf);

    // Colon 1
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    display.setCursor(xColon1 + 2, y); // Centered in its 8px gap
    display.print(":");

    // Start minute
    sprintf(buf, "%02d", startMin);
    display.setTextColor(highlightTarget == 2 ? SH110X_BLACK : SH110X_WHITE,
                         highlightTarget == 2 ? SH110X_WHITE : SH110X_BLACK);
    if (highlightTarget == 2)
      display.fillRect(xStartMin, boxY, boxW, boxH, SH110X_WHITE);
    display.setCursor(xStartMin + 1, y);
    display.print(buf);

    // Dash (Custom shorter line)
    int dashY = y + 7; // Vertically center it with the size 2 numbers

    // Draw two lines to make it 2 pixels thick, matching the size 2 font weight
    // It spans 6 pixels (xDash + 2 to xDash + 7), leaving 2 pixels of padding
    // on each side
    display.drawLine(xDash + 2, dashY, xDash + 7, dashY, SH110X_WHITE);
    display.drawLine(xDash + 2, dashY + 1, xDash + 7, dashY + 1, SH110X_WHITE);

    // End hour
    sprintf(buf, "%02d", endHour);
    display.setTextColor(highlightTarget == 3 ? SH110X_BLACK : SH110X_WHITE,
                         highlightTarget == 3 ? SH110X_WHITE : SH110X_BLACK);
    if (highlightTarget == 3)
      display.fillRect(xEndHour, boxY, boxW, boxH, SH110X_WHITE);
    display.setCursor(xEndHour + 1, y);
    display.print(buf);

    // Colon 2
    display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    display.setCursor(xColon2 + 2, y); // Centered in its 8px gap
    display.print(":");

    // End minute
    sprintf(buf, "%02d", endMin);
    display.setTextColor(highlightTarget == 4 ? SH110X_BLACK : SH110X_WHITE,
                         highlightTarget == 4 ? SH110X_WHITE : SH110X_BLACK);
    if (highlightTarget == 4)
      display.fillRect(xEndMin, boxY, boxW, boxH, SH110X_WHITE);
    display.setCursor(xEndMin + 1, y);
    display.print(buf);

    // -- Back option
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

  uint8_t readButtonsDebounced(void) {
    static uint8_t lastState = 0;
    static unsigned long lastChangeTime = 0;
    const unsigned long debounceMs = 30;

    uint8_t currentState = readButtons();

    if (currentState != lastState) {
      lastChangeTime = millis();
    }

    if ((millis() - lastChangeTime) > debounceMs) {
      lastState = currentState;
    }

    return lastState;
  }
};

enum DeviceMode { MODE_OFF, MODE_AUTO, MODE_MANUAL_ON };

struct DeviceInfo {
  String name;
  int maxWind;
  String startTime;
  String endTime;
  DeviceMode mode;
};

enum DeviceID { DEVICE_GATE, DEVICE_POND, DEVICE_FOUNTAINS, DEVICE_COUNT };

enum InfoScreen { INFO_DEFAULT, INFO_SELECT_DEVICE, INFO_SHOW_DEVICE };

class InfoController {
private:
  MyDisplay &display;
  InfoScreen screen = INFO_DEFAULT;
  DeviceID selectedDevice = DEVICE_GATE;

  // -- Dummy data for now; real values will come from elsewhere later --
  DeviceInfo devices[DEVICE_COUNT] = {
      {"Gate", 15, "16:32", "23:04", MODE_AUTO},
      {"Pond", 10, "08:00", "20:00", MODE_OFF},
      {"Fountains", 20, "09:15", "21:45", MODE_MANUAL_ON}};

  // -- Dummy values for the default info screen --
  int dummyWindSpeed = 23;
  float dummyTemp = 21.5;
  int dummyHours = 14;
  int dummyMinutes = 32;

public:
  InfoController(MyDisplay &disp) : display(disp) {}

  void handleInput(uint8_t buttons) {
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
      screen = INFO_SELECT_DEVICE;
      break;
    }
  }

  void render() {
    switch (screen) {
    case INFO_DEFAULT:
      display.showCurrentInformation(dummyWindSpeed, dummyTemp, dummyHours,
                                     dummyMinutes);
      break;

    case INFO_SELECT_DEVICE:
      display.showStatusViewSelectonScreen();
      break;

    case INFO_SHOW_DEVICE: {
      DeviceInfo &d = devices[selectedDevice];
      bool onOrOff = (d.mode != MODE_OFF);
      bool manual = (d.mode == MODE_MANUAL_ON);
      display.showTheIsValues(d.name, d.maxWind, d.startTime, d.endTime,
                              onOrOff, manual);
      break;
    }
    }
  }

  void resetToDefault() { screen = INFO_DEFAULT; }
};

enum MenuScreen {
  MENU_SELECT_DEVICE,  // Gate/Pond/Fountains/Back
  MENU_DEVICE_OPTIONS, // Schedule/Wind Limit/Mode/Back
  MENU_MODE_SELECT,    // Off/Auto/Manual/Back
  MENU_EDIT_WIND,      // showIntKmhSetting
  MENU_EDIT_SCHEDULE   // showTimeIntervalSetting
};

class MenuController {
private:
  MyDisplay &display;

  MenuScreen screen = MENU_SELECT_DEVICE;
  DeviceID selectedDevice = DEVICE_GATE;

  int listSelection = 0; // shared by all showSettingsMenu-based screens
  int editTarget = 1;    // shared by the two edit screens
  bool isEditingField = false;
  bool exitRequested = false;

  // -- Blink timing for the "editing" state --
  bool blinkVisible = true;
  unsigned long lastBlinkToggle = 0;
  const unsigned long blinkIntervalMs = 400;

  // -- Dummy per-device settings data --
  struct DeviceSettings {
    String name;
    int windLimit;
    int startHour, startMin, endHour, endMin;
    DeviceMode mode;
  };

  DeviceSettings devices[DEVICE_COUNT] = {
      {"Gate", 15, 16, 32, 23, 4, MODE_AUTO},
      {"Pond", 10, 8, 0, 20, 0, MODE_OFF},
      {"Fountains", 20, 9, 15, 21, 45, MODE_MANUAL_ON}};

  // -- Option label arrays, kept as members so pointers stay valid --
  String deviceListOptions[4] = {"Gate", "Pond", "Fountains", "Back"};
  String deviceMenuOptions[4] = {"Schedule", "Wind Limit", "Mode", "Back"};
  String modeSelectOptions[4] = {"Off", "Auto", "Manual", "Back"};

public:
  MenuController(MyDisplay &disp) : display(disp) {}

  void reset() {
    screen = MENU_SELECT_DEVICE;
    listSelection = 0;
    editTarget = 1;
    isEditingField = false;
    exitRequested = false;
  }

  bool wantsExit() { return exitRequested; }

  void handleInput(uint8_t buttons) {
    if (buttons == 0)
      return;

    switch (screen) {
    case MENU_SELECT_DEVICE:
      if (navigateList(buttons, 4)) {
        if (listSelection == 3) { // Back
          exitRequested = true;
        } else {
          selectedDevice = (DeviceID)listSelection;
          screen = MENU_DEVICE_OPTIONS;
          listSelection = 0;
        }
      }
      break;

    case MENU_DEVICE_OPTIONS:
      if (navigateList(buttons, 4)) {
        switch (listSelection) {
        case 0: // Schedule
          screen = MENU_EDIT_SCHEDULE;
          editTarget = 1;
          isEditingField = false;
          break;
        case 1: // Wind Limit
          screen = MENU_EDIT_WIND;
          editTarget = 1;
          isEditingField = false;
          break;
        case 2: // Mode
          screen = MENU_MODE_SELECT;
          listSelection = (int)devices[selectedDevice].mode;
          break;
        case 3: // Back
          screen = MENU_SELECT_DEVICE;
          listSelection = (int)selectedDevice;
          break;
        }
      }
      break;

    case MENU_MODE_SELECT:
      if (navigateList(buttons, 4)) {
        if (listSelection != 3) { // not Back
          devices[selectedDevice].mode = (DeviceMode)listSelection;
        }
        screen = MENU_DEVICE_OPTIONS;
        listSelection = 2; // land back on "Mode" row
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

  void render() {
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

    case MENU_EDIT_WIND: {
      DeviceSettings &d = devices[selectedDevice];
      display.showIntKmhSetting(d.name, d.windLimit, currentBlinkTarget());
      break;
    }

    case MENU_EDIT_SCHEDULE: {
      DeviceSettings &d = devices[selectedDevice];
      display.showTimeIntervalSetting(d.name, d.startHour, d.startMin,
                                      d.endHour, d.endMin,
                                      currentBlinkTarget());
      break;
    }
    }
  }

private:
  // Shared A/C navigation for the three showSettingsMenu-based screens.
  // Returns true if B (confirm) was pressed this call.
  bool navigateList(uint8_t buttons, int numOptions) {
    if (buttons & 0x1) { // A: previous
      listSelection = (listSelection - 1 + numOptions) % numOptions;
    } else if (buttons & 0x4) { // C: next
      listSelection = (listSelection + 1) % numOptions;
    } else if (buttons & 0x2) { // B: confirm
      return true;
    }
    return false;
  }

  // Wind Limit edit screen. Targets: 1 = value, 2 = back.
  void handleWindEditInput(uint8_t buttons) {
    DeviceSettings &d = devices[selectedDevice];

    if (isEditingField) {
      if (buttons & 0x1) { // A: decrement
        d.windLimit = max(0, d.windLimit - 1);
      } else if (buttons & 0x4) { // C: increment
        d.windLimit = min(99, d.windLimit + 1);
      } else if (buttons & 0x2) { // B: confirm, stop editing
        isEditingField = false;
      }
      return;
    }

    if (buttons & (0x1 | 0x4)) { // A or C: only two targets, so either toggles
      editTarget = (editTarget == 1) ? 2 : 1;
    } else if (buttons & 0x2) { // B: select
      if (editTarget == 2) {    // Back
        screen = MENU_DEVICE_OPTIONS;
        listSelection = 1; // "Wind Limit" row
      } else {
        isEditingField = true;
      }
    }
  }

  // Schedule edit screen. Targets: 1=startHour, 2=startMin, 3=endHour,
  // 4=endMin, 5=back.
  void handleScheduleEditInput(uint8_t buttons) {
    DeviceSettings &d = devices[selectedDevice];

    if (isEditingField) {
      int delta = 0;
      if (buttons & 0x1)
        delta = -1;
      else if (buttons & 0x4)
        delta = 1;
      else if (buttons & 0x2) { // B: confirm, stop editing
        isEditingField = false;
        return;
      }

      if (delta != 0) {
        switch (editTarget) {
        case 1:
          d.startHour = (d.startHour + delta + 24) % 24;
          break;
        case 2:
          d.startMin = (d.startMin + delta + 60) % 60;
          break;
        case 3:
          d.endHour = (d.endHour + delta + 24) % 24;
          break;
        case 4:
          d.endMin = (d.endMin + delta + 60) % 60;
          break;
        }
      }
      return;
    }

    if (buttons & 0x1) { // A: previous field
      editTarget = (editTarget == 1) ? 5 : editTarget - 1;
    } else if (buttons & 0x4) { // C: next field
      editTarget = (editTarget == 5) ? 1 : editTarget + 1;
    } else if (buttons & 0x2) { // B: select
      if (editTarget == 5) {    // Back
        screen = MENU_DEVICE_OPTIONS;
        listSelection = 0; // "Schedule" row
      } else {
        isEditingField = true;
      }
    }
  }

  void updateBlink() {
    if (millis() - lastBlinkToggle > blinkIntervalMs) {
      blinkVisible = !blinkVisible;
      lastBlinkToggle = millis();
    }
  }

  int currentBlinkTarget() {
    if (isEditingField && !blinkVisible) {
      return 0; // hide the highlight this frame
    }
    return editTarget;
  }
};

enum AppMode { MODE_INFO, MODE_MENU /*, MODE_DEBUG */ };

class App {
private:
  MyDisplay display;
  InfoController infoController;
  MenuController menuController;
  // DebugController debugController; // to be added later

  AppMode currentMode = MODE_INFO;

  unsigned long lastInputTime = 0;
  const unsigned long infoTimeoutMs =
      8000; // back to default info after 8s idle

  // -- Combo detection state --
  uint8_t pendingButtons = 0;
  unsigned long pendingSince = 0;
  const unsigned long comboWindowMs = 60; // grace period to let a combo form

public:
  App() : infoController(display), menuController(display) {}

  void init() { display.init(); }

  void loop() {
    uint8_t buttons = display.readButtonsDebounced();

    if (buttons != 0) {
      lastInputTime = millis();
    }

    handleModeEntry(buttons);

    // Idle timeout: snap back to default info screen
    if (currentMode == MODE_INFO &&
        (millis() - lastInputTime > infoTimeoutMs)) {
      infoController.resetToDefault();
    }

    switch (currentMode) {
    case MODE_INFO:
      infoController.render();
      break;
    case MODE_MENU:
      menuController.render();
      break;
      // case MODE_DEBUG:
      //   debugController.render();
      //   break;
    }
  }

private:
  void handleModeEntry(uint8_t buttons) {
    // -- Wait for a short window to let combos fully register --
    if (buttons != 0 && pendingButtons == 0) {
      pendingButtons = buttons;
      pendingSince = millis();
      return;
    }

    if (buttons != 0 && (millis() - pendingSince) < comboWindowMs) {
      pendingButtons |= buttons; // more buttons joined within the window
      return;
    }

    if (buttons == 0 && pendingButtons == 0) {
      return; // nothing pressed, nothing pending
    }

    uint8_t finalCombo = pendingButtons | buttons;
    pendingButtons = 0;

    if (finalCombo == 0) {
      return;
    }

    // -- Mode-switch combos (checked first, take priority over in-mode input)
    // --
    if (finalCombo == 0x3) { // A+B -> settings
      currentMode = MODE_MENU;
      menuController.reset();
      return;
    }

    // if (finalCombo == 0x5) { // A+C -> debug (only valid from info screen)
    //   if (currentMode == MODE_INFO) {
    //     currentMode = MODE_DEBUG;
    //     debugController.reset();
    //     return;
    //   }
    // }

    // -- Otherwise, route to whichever controller is active --
    if (currentMode == MODE_INFO) {
      infoController.handleInput(finalCombo);
    } else if (currentMode == MODE_MENU) {
      menuController.handleInput(finalCombo);
      // MenuController is responsible for setting currentMode back to
      // MODE_INFO when the user backs all the way out of settings.
      if (menuController.wantsExit()) {
        currentMode = MODE_INFO;
      }
    }
    // else if (currentMode == MODE_DEBUG) {
    //   debugController.handleInput(finalCombo);
    //   if (debugController.wantsExit()) {
    //     currentMode = MODE_INFO;
    //   }
    // }
  }
};

App app;

void setup() {
  Serial.begin(115200);
  radioSetup(MYNODEID);
  Serial.println("Node " + String(MYNODEID) + " up.");
  blinkNeoPixel(0, 0, 255, 100, 1);
  Serial.println("Feather RP2040 Radio Initialized successfully.");
  delay(250); // wait for the OLED to power up

  app.init();
}

void loop() {
  app.loop();
  blinkNeoPixel(0, 255, 255, 200, 1);
}

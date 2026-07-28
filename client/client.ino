#include "SerialUSB.h"
#include "WindRadioCommon.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

#define MYNODEID 2
#define TONODEID 1
#define BUTTON_A 9
#define BUTTON_B 6
#define BUTTON_C 5

Adafruit_SH1107 display = Adafruit_SH1107(64, 128, &Wire);

bool currentStateG = true;
bool manual_modeG = false;

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
  // It spans 6 pixels (xDash + 2 to xDash + 7), leaving 2 pixels of padding on
  // each side
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

void setup() {
  Serial.begin(115200);
  radioSetup(MYNODEID);
  Serial.println("Node " + String(MYNODEID) + " up.");
  blinkNeoPixel(0, 0, 255, 100, 1);
  Serial.println("Feather RP2040 Radio Initialized successfully.");
  delay(250); // wait for the OLED to power up

  display.begin(0x3C, true); // Address 0x3C default
  display.clearDisplay();
  display.display();
  display.setRotation(3);

  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
}

void loop() {
  static unsigned long lastChange = 0;
  static int screenState = 0;
  const int totalScreens = 8;

  if (millis() - lastChange >= 2000) {
    lastChange = millis();
    screenState = (screenState + 1) % totalScreens;
  }

  switch (1) {
  case 0:
    showCurrentInformation(23, 21.5, 14, 32);
    break;

  case 1: {
    String settingsOptions[] = {"Wind Threshold", "Gate Timeout",
                                "Pond Schedule", "Fountain Mode",
                                "Radio Channel"};
    showSettingsMenu(settingsOptions, 5,
                     2); // mid-scroll, to test scroll arrows
    break;
  }

  case 2:
    showStatusViewSelectonScreen();
    break;

  case 3:
    showTheIsValues("Gate", 15, "16:32", "23:04", true, false);
    break;

  case 4:
    showTheIsValues("Gate", 15, "16:32", "23:04", false,
                    true); // manual mode variant
    break;

  case 5:
    showIntKmhSetting("Wind Threshold", 25, 1); // highlight value
    break;

  case 6:
    showIntKmhSetting("Wind Threshold", 25, 2); // highlight back
    break;

  case 7:
    showTimeIntervalSetting("Pond Schedule", 8, 30, 22, 0,
                            5); // highlight end hour
    break;
  }

  blinkNeoPixel(0, 255, 255, 200, 1);
  delay(2000);
}

#include <Arduino.h>
#line 1 "/home/storm/Projects/WindRadio/client/client.ino"
#include "RadioManager.h"
#include "SerialUSB.h"
#include "SystemData.h"
#include "WindRadioCommon.h"
#include "pico/mutex.h"
#include "screen.h"

#define MYNODEID 1 // Node 1 (Main Node)

// ---- Global State & Mutexes ----
volatile bool core0ReadyG = false; // Synchronizes Core 1 startup

DeviceSettings deviceSettingsG[DEVICE_COUNT] = {
    {"Gate", 15, 16, 32, 23, 4, MODE_AUTO},
    {"Pond", 10, 8, 0, 20, 0, MODE_AUTO},
    {"Fountain", 20, 9, 15, 21, 45, MODE_AUTO}};
mutex_t deviceSettingsMutex;

CurrentConditions currentConditionsG = {23, 21.5, 14, 32}; // dummy defaults
mutex_t currentConditionsMutex;

// ---- Thread-Safe Data Accessors ----
#line 23 "/home/storm/Projects/WindRadio/client/client.ino"
void getDeviceSettings(DeviceID id, DeviceSettings &out);
#line 29 "/home/storm/Projects/WindRadio/client/client.ino"
void setDeviceSettings(DeviceID id, const DeviceSettings &in);
#line 35 "/home/storm/Projects/WindRadio/client/client.ino"
void getCurrentConditions(CurrentConditions &out);
#line 41 "/home/storm/Projects/WindRadio/client/client.ino"
void updateConditionsFromPond(int windSpeed, int hours, int minutes);
#line 55 "/home/storm/Projects/WindRadio/client/client.ino"
void setup();
#line 76 "/home/storm/Projects/WindRadio/client/client.ino"
void loop();
#line 81 "/home/storm/Projects/WindRadio/client/client.ino"
void setup1();
#line 86 "/home/storm/Projects/WindRadio/client/client.ino"
void loop1();
#line 23 "/home/storm/Projects/WindRadio/client/client.ino"
void getDeviceSettings(DeviceID id, DeviceSettings &out) {
  mutex_enter_blocking(&deviceSettingsMutex);
  out = deviceSettingsG[id];
  mutex_exit(&deviceSettingsMutex);
}

void setDeviceSettings(DeviceID id, const DeviceSettings &in) {
  mutex_enter_blocking(&deviceSettingsMutex);
  deviceSettingsG[id] = in;
  mutex_exit(&deviceSettingsMutex);
}

void getCurrentConditions(CurrentConditions &out) {
  mutex_enter_blocking(&currentConditionsMutex);
  out = currentConditionsG;
  mutex_exit(&currentConditionsMutex);
}

void updateConditionsFromPond(int windSpeed, int hours, int minutes) {
  mutex_enter_blocking(&currentConditionsMutex);
  currentConditionsG.windSpeed = windSpeed;
  currentConditionsG.hours = hours;
  currentConditionsG.minutes = minutes;
  // temperature untouched -- no sensor for it yet
  mutex_exit(&currentConditionsMutex);
}

// ---- Application Instances ----
App app;
RadioManager radioManager;

// ================= CORE 0 (Display & Base Setup) =================
void setup() {
  mutex_init(&deviceSettingsMutex);
  mutex_init(&currentConditionsMutex);
  radioManager.init(); // Initializes radio manager mutexes internally

  Serial.begin(115200);
  radioSetup(MYNODEID);
  Serial.println("Node " + String(MYNODEID) + " up.");

  blinkNeoPixel(0, 0, 255, 100, 1);
  Serial.println("Feather RP2040 Radio Initialized successfully.");

  delay(250); // Wait for the OLED to power up

  app.init();

  // Core 0 is finished initializing. Safe for Core 1 to start using
  // hardware/mutexes.
  core0ReadyG = true;
}

void loop() {
  app.loop(); // Handle UI and Display
}

// ================= CORE 1 (Radio Polling & Logic) =================
void setup1() {
  // Wait here passively; actual initialization tied to hardware happens in Core
  // 0's setup()
}

void loop1() {
  if (!core0ReadyG)
    return; // Prevent executing before core 0 completes setup

  radioManager.loop(); // Process background radio tasks and dispatch logic
}


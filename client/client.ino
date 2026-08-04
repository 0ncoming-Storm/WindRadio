/*
 * client.ino — WindRadio base station (front panel + display + radio).
 *
 * Runs on a Feather RP2040 with dual-core support:
 *   Core 0: UI / display / button handling via App class.
 *   Core 1: background radio polling via RadioManager.
 *
 * Wiring:
 *   Buttons: A=GPIO5, B=GPIO6, C=GPIO9 (INPUT_PULLUP)
 *   OLED:    SH1107 on I2C (addr 0x3C)
 *   Radio:   RFM69HW on SPI (CS=RFM69_CS, INT=RFM69_INT, RST=RFM69_RST)
 *   NeoPixel: status LED on GPIO4
 */

#include "RadioManager.h"
#include "SerialUSB.h"
#include "SystemData.h"
#include "WindRadioCommon.h"
#include "pico/mutex.h"
#include "screen.h"

// ---- Global State & Mutexes ----
volatile bool core0ReadyG = false; // Synchronizes Core 1 startup

// Default device settings: {name, windLimit, startHr, startMin, endHr, endMin,
// mode}
DeviceSettings deviceSettingsG[DEVICE_COUNT] = {
    {"Gate", 15, 16, 32, 23, 4, MODE_AUTO},
    {"Pond", 10, 8, 0, 20, 0, MODE_AUTO},
    {"Fountain", 20, 9, 15, 21, 45, MODE_AUTO}};
mutex_t deviceSettingsMutex;

CurrentConditions currentConditionsG = {99, 21.5, 14, 32}; // dummy defaults
mutex_t currentConditionsMutex;

// ---- Thread-Safe Data Accessors ----
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

void updateConditionsFromPond(int windSpeed, float temperature, int hours,
                              int minutes) {
  mutex_enter_blocking(&currentConditionsMutex);
  currentConditionsG.windSpeed = windSpeed;
  currentConditionsG.temperature = temperature;
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
  // while (!Serial) {
  //   ; // Do nothing, just loop
  // }
  radioSetup(NODE_MAIN);
  Wire.begin();
  Wire.setClock(400000);
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
  // Wait here passively; actual initialization tied to hardware happens in
  // Core0's setup()
}

void loop1() {
  if (!core0ReadyG)
    return; // Prevent executing before core 0 completes setup

  radioManager.loop(); // Process background radio tasks and dispatch logic
}

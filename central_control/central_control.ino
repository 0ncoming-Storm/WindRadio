/*
 * central_control.ino — WindRadio base station (front panel + display +
 * radio).
 *
 * Runs on a Feather RP2040 with dual-core support:
 *   Core 0: UI / display / button handling via App class.
 *   Core 1: radio init + background polling via RadioManager.
 *
 * Wiring:
 *   Buttons: A=GPIO5, B=GPIO6, C=GPIO9 (INPUT_PULLUP)
 *   OLED:    SH1107 on I2C (addr 0x3C)
 *   Radio:   RFM69HW on SPI (CS=RFM69_CS, INT=RFM69_INT, RST=RFM69_RST)
 */

#include "RadioManager.h"
#include "SerialUSB.h"
#include "SystemData.h"
#include "WindRadioCommon.h"
#include "pico/mutex.h"
#include "screen.h"

// ---- Global State & Mutexes ----
bool core0ReadyG = false; // Synchronizes Core 1 startup (accessed via atomics)

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
  // !!! DEPLOYMENT: this unbounded wait blocks boot until a USB host is
  // connected — on this core `!Serial` is true while the USB CDC link is
  // down, and Core 1 cannot start either (it waits on core0ReadyG).
  // Intentional for bench work so serial is ready before any output.
  // Before field deployment, remove it or bound it, e.g.:
  //   while (!Serial && millis() < 5000) ;
  while (!Serial) {
    ; // Do nothing, just loop
  }
  Wire.begin();
  Wire.setClock(400000);
  delay(250); // Wait for the OLED to power up
  app.init();

  // Core 0 is finished initializing. Safe for Core 1 to start using
  // hardware/mutexes.
  __atomic_store_n(&core0ReadyG, true, __ATOMIC_RELEASE);
}

void loop() {
  app.loop(); // Handle UI and Display
}

// ================= CORE 1 (Radio Init, Polling & Logic) =================
void setup1() {
  // Wait for core 0 to finish bringing up shared state.
  while (!__atomic_load_n(&core0ReadyG, __ATOMIC_ACQUIRE))
    tight_loop_contents();

  // The radio lives entirely on this core: initialization, its DIO0
  // interrupt, and all SPI traffic. The RFM69 library detaches/re-attaches
  // its interrupt handler during normal operation (every receiveDone()),
  // which must not happen from a different core than the one servicing
  // that interrupt.
  radioSetup(NODE_MAIN);
}

void loop1() {
  radioManager.loop(); // Process background radio tasks and dispatch logic
}

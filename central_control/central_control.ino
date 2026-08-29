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

// ---- Watchdog / dual-core liveness ----
// The remote nodes each carry their own 8 s WDT; the base was the only node
// without one. A hung core — Core 0 stuck in an I2C OLED transfer, Core 1
// wedged in a loop, either core spinning in a hardfault — would otherwise
// leave the system unattended forever. So the base arms the chip WDT and
// BOTH cores feed it, with a twist: a kick from a healthy core would always
// mask a hung one, so each core only kicks the WDT while the OTHER core is
// still producing beats.
//
// Beats are plain volatile uint32 counters — a 32-bit aligned read/write is
// atomic on the M0+, and the watchers only need "did it change" semantics.
// Core 1 stamps progress from RadioManager (see radioCoreBeat()) at every
// loop iteration AND at progress points inside long poll cycles, because
// one transact() can block ~1.5 s worst case (CSMA + send + listen slice).
//
// Timing: 10 s without beats from a core = stalled; the 20 s WDT period
// then forces a reset. Worst case a hung core is reset within ~30 s.
//
// The WDT is armed in setup1() — after core 0 has fully booted — so the
// bench `while (!Serial)` wait in setup() cannot reboot-loop a headless
// board. When you bound/remove that wait for field deployment (see its
// comment), this watchdog also makes WDT-triggered reboots land back in a
// working system.
//
// Coverage gap, by design: a hang DURING setup() (e.g. OLED I2C never
// answers) happens before the WDT is armed, so it is not auto-recovered.
// The safety property still holds there — a base that never polls trips
// every node's 6-minute fail-safe (relay OFF) — only recovery is slower.
#define BASE_WDT_TIMEOUT_MS 20000
#define CORE_STALL_MS 10000

static volatile uint32_t gCore0Beat = 0; // bumped by Core 0 every loop
static volatile uint32_t gCore1Beat = 0; // bumped by Core 1 (see radioCoreBeat)

// Tracks the other core's beat counter; reports whether it has changed
// within CORE_STALL_MS.
class CoreStallWatcher {
public:
  explicit CoreStallWatcher(const volatile uint32_t *beat) : beat(beat) {}
  bool otherCoreAlive() {
    uint32_t b = *beat;
    if (b != lastBeat) {
      lastBeat = b;
      lastBeatMs = millis();
      return true;
    }
    return (millis() - lastBeatMs) < CORE_STALL_MS;
  }

private:
  const volatile uint32_t *beat;
  uint32_t lastBeat = 0;
  unsigned long lastBeatMs = 0;
};

static CoreStallWatcher watchCore1(&gCore1Beat); // used by Core 0
static CoreStallWatcher watchCore0(&gCore0Beat); // used by Core 1

// Liveness stamp for the radio core (Core 1). Called from loop1() and from
// progress points inside RadioManager's poll cycles (declared in
// RadioManager.h so the radio thread can stamp without including this
// sketch).
void radioCoreBeat() { gCore1Beat++; }

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
  gCore0Beat++;
  // Feed the WDT only while Core 1 is still beating — otherwise a healthy
  // Core 0 would mask a dead radio core (see watchdog notes above).
  if (watchCore1.otherCoreAlive())
    rp2040.wdt_reset();

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

  // Arm the board watchdog now that BOTH cores are fully up (see watchdog
  // notes at the top). On any later hang of either core, the other detects
  // the stall, stops kicking, and the WDT resets the board.
  rp2040.wdt_begin(BASE_WDT_TIMEOUT_MS);
}

void loop1() {
  radioCoreBeat(); // stamp liveness for Core 0's watchdog watcher
  if (watchCore0.otherCoreAlive())
    rp2040.wdt_reset();

  radioManager.loop(); // Process background radio tasks and dispatch logic
}

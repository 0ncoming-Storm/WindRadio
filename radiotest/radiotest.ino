/*
 * radiotest.ino — WindRadio mode-auto-detecting test node.
 *
 * Mode detection at startup:
 *   - If an OLED display is found on the I2C bus -> RECEIVER mode.
 *       Listens for packets from the sender, then displays the received data
 *       and the radio signal strength (RSSI) on the OLED.
 *
 *   - If no display is found -> SENDER mode.
 *       Transmits an uptime counter (MM:SS) to the receiver every 30 seconds.
 *
 * Radio addressing:
 *   Sender   (this node when no screen is present): node 1
 *   Receiver (this node when a screen is present) : node 2
 */

#include "SerialUSB.h"
#include "WindRadioCommon.h"

// OLED / I2C display helpers
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Wire.h>

// ---- Radio addressing ----
#define SENDER_NODE_ID 1   // this node's address when acting as sender
#define RECEIVER_NODE_ID 2 // this node's address when acting as receiver
#define TARGET_NODE_ID 2   // sender always transmits to the receiver (node 2)

// ---- Timing ----
#define SEND_INTERVAL_MS 2000UL // sender pushes the data every 2 seconds

// Mode, decided once during setup().
enum Role { ROLE_UNSET, ROLE_SENDER, ROLE_RECEIVER };
Role role = ROLE_UNSET;

// ---- Receiver display / state ----
// Adafruit 128x64 OLED FeatherWing uses the SH1107.
// It is physically 64x128 but we rotate it to 128x64 landscape.
Adafruit_SH1107 displayScreen = Adafruit_SH1107(64, 128, &Wire);
bool haveDisplay = false;

uint8_t lastVal1 = 0;
uint8_t lastVal2 = 0;
int16_t lastRSSI = 0;
bool haveData = false;
unsigned long lastReceiveMs = 0;

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);

  // Bring up I2C first so we can probe for the Display.
  Wire.begin();
  Wire.setClock(400000);
  delay(50);

  // Decide our role based on whether an OLED display is wired up.
  if (displayScreen.begin(0x3C, true)) {
    role = ROLE_RECEIVER;
    radioSetup(RECEIVER_NODE_ID);
    Serial.println("OLED found -> RECEIVER mode. Listening for broadcasts.");
    initDisplay();
  } else {
    role = ROLE_SENDER;
    radioSetup(SENDER_NODE_ID);
    Serial.println("No OLED -> SENDER mode. Broadcasting every 30s.");
  }
}

// ============================================================
// Main loop
// ============================================================
void loop() {
  if (role == ROLE_SENDER) {
    senderLoop();
  } else if (role == ROLE_RECEIVER) {
    receiverLoop();
  }
}

// ============================================================
// SENDER mode — transmit uptime data every 30s
// ============================================================
void senderLoop() {
  static unsigned long lastSend = 0;
  static bool primed = false;
  if (!primed) {
    // Fire the first transmission immediately instead of waiting 30s.
    lastSend = millis() - SEND_INTERVAL_MS;
    primed = true;
  }

  unsigned long now = millis();
  if (now - lastSend < SEND_INTERVAL_MS) {
    return;
  }
  lastSend = now;

  // Create a simulated timestamp based on system uptime (MM:SS)
  unsigned long uptimeSecs = now / 1000;
  uint8_t mins = (uptimeSecs / 60) % 60;
  uint8_t secs = uptimeSecs % 60;

  // Reuse the existing POND_STATUS packet shape to carry the uptime.
  WindRadioPacket pck;
  pck.type = PKT_POND_STATUS;
  pck.pondStatus.hours = mins;   // Store minutes here
  pck.pondStatus.minutes = secs; // Store seconds here
  pck.pondStatus.temperature = 0;
  pck.pondStatus.windSpeed = 0;
  pck.pondStatus.relayOn = false;

  Serial.print("Sending uptime ");
  if (mins < 10)
    Serial.print('0');
  Serial.print(mins);
  Serial.print(':');
  if (secs < 10)
    Serial.print('0');
  Serial.print(secs);

  // sendPacket() requests an ACK, so a true return means the receiver heard us.
  if (sendPacket(TARGET_NODE_ID, pck)) {
    Serial.println("  -> ACKed by receiver.");
  } else {
    Serial.println("  -> no ACK (receiver offline?).");
  }
}

// ============================================================
// RECEIVER mode — listen, then show data + signal strength
// ============================================================
void receiverLoop() {
  WindRadioPacket pck;

  if (receivePacket(pck)) {
    // readRSSI() (from the LowPowerLab RFM69 library) reports the signal
    // strength of the packet we just received.
    lastRSSI = radio.readRSSI();

    if (pck.type == PKT_POND_STATUS) {
      lastVal1 = pck.pondStatus.hours;
      lastVal2 = pck.pondStatus.minutes;
      haveData = true;
      lastReceiveMs = millis();

      Serial.print("Got payload ");
      if (lastVal1 < 10)
        Serial.print('0');
      Serial.print(lastVal1);
      Serial.print(':');
      if (lastVal2 < 10)
        Serial.print('0');
      Serial.print(lastVal2);
      Serial.print("   RSSI ");
      Serial.print(lastRSSI);
      Serial.println(" dBm");
    }

    updateDisplay();
  }
}

// ============================================================
// Display (receiver mode)
// ============================================================
void initDisplay() {
  haveDisplay = true;
  displayScreen.clearDisplay();

  // Rotation 1 sets the Featherwing layout to standard 128x64 landscape
  displayScreen.setRotation(1);
  displayScreen.setTextSize(1);
  displayScreen.setTextColor(SH110X_WHITE, SH110X_BLACK);

  // Initial "waiting" screen.
  displayScreen.clearDisplay();
  displayScreen.setCursor(8, 22);
  displayScreen.print("Waiting for");
  displayScreen.setCursor(8, 36);
  displayScreen.print("signal...");
  displayScreen.display();
}

void updateDisplay() {
  if (!haveDisplay) {
    return;
  }

  displayScreen.clearDisplay();

  // Header bar
  displayScreen.fillRect(0, 0, 128, 11, SH110X_WHITE);
  displayScreen.setTextColor(SH110X_BLACK, SH110X_WHITE);
  displayScreen.setCursor(20, 2);
  displayScreen.print("RADIO LINK");

  // Big Readout (MM:SS)
  displayScreen.setTextColor(SH110X_WHITE, SH110X_BLACK);
  displayScreen.setTextSize(3);
  char buf[6];
  if (haveData) {
    sprintf(buf, "%02d:%02d", lastVal1, lastVal2);
  } else {
    sprintf(buf, "--:--");
  }
  displayScreen.setCursor(24, 18);
  displayScreen.print(buf);
  displayScreen.setTextSize(1);

  // RSSI readout
  displayScreen.setCursor(2, 52);
  displayScreen.print("RSSI ");
  displayScreen.print(haveData ? lastRSSI : 0);
  displayScreen.print(" dBm");

  // Signal-strength bars (5 segments): -50 dBm strong -> -90 dBm weak.
  int bars = 0;
  if (haveData) {
    if (lastRSSI > -50)
      bars = 5;
    else if (lastRSSI > -60)
      bars = 4;
    else if (lastRSSI > -70)
      bars = 3;
    else if (lastRSSI > -80)
      bars = 2;
    else if (lastRSSI > -90)
      bars = 1;
  }
  for (int i = 0; i < 5; i++) {
    int x = 98 + i * 5;
    int h = 4 + i * 3;
    int yTop = 62 - h;
    if (i < bars) {
      displayScreen.fillRect(x, yTop, 3, h, SH110X_WHITE);
    } else {
      displayScreen.drawRect(x, yTop, 3, h, SH110X_WHITE);
    }
  }

  displayScreen.display();
}

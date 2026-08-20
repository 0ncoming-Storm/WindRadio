/*
 * receiver-latching.ino — latching relay node for the gate (NODE_GATE).
 *
 * The relay does NOT drive the gate motor. It sits between the in-ground
 * car sensor and the gate opener's own control board, interrupting the
 * sensor's signal wire:
 *   RELAY_ON  = sensor signal connected  -> gate may auto-open on a car
 *   RELAY_OFF = sensor signal interrupted -> gate will not auto-open
 *
 * The relay is latching: a short pulse on one coil moves it and the
 * position is held without power, surviving power loss. Because of that,
 * the node cannot know its true position at boot and reports
 * RELAY_UNKNOWN until the first command resolves it.
 *
 * The node is purely passive on the radio: it never transmits unless it is
 * answering the central control node.
 */

#include "WindRadioCommon.h"

#define RELAY_SET_PIN 5   // pulse to move relay to ON (signal connected)
#define RELAY_UNSET_PIN 6 // pulse to move relay to OFF (signal interrupted)
#define COIL_PULSE_MS 20

// Watchdog period. Generous: the loop only blocks for radio retries and
// coil pulses. A watchdog reset leaves the relay position unchanged.
#define WATCHDOG_TIMEOUT_MS 8000

static RelayState relayState = RELAY_UNKNOWN;

static void applyRelay(bool on) {
  if (on) {
    digitalWrite(RELAY_SET_PIN, HIGH);
    delay(COIL_PULSE_MS);
    digitalWrite(RELAY_SET_PIN, LOW);
  } else {
    digitalWrite(RELAY_UNSET_PIN, HIGH);
    delay(COIL_PULSE_MS);
    digitalWrite(RELAY_UNSET_PIN, LOW);
  }
  // The two pins are never energized together: each branch fully releases
  // its pin before returning.
  relayState = on ? RELAY_ON : RELAY_OFF;
}

static void replyStatus() {
  WindRadioPacket pkt;
  pkt.type = PKT_RELAY_STATUS;
  pkt.relayStatus.nodeId = NODE_GATE;
  pkt.relayStatus.relayState = relayState;
  sendPacket(NODE_MAIN, pkt);
}

void setup() {
  pinMode(RELAY_SET_PIN, OUTPUT);
  pinMode(RELAY_UNSET_PIN, OUTPUT);
  digitalWrite(RELAY_SET_PIN, LOW);
  digitalWrite(RELAY_UNSET_PIN, LOW);

  radioSetup(NODE_GATE);
  Serial.begin(115200);

  rp2040.wdt_begin(WATCHDOG_TIMEOUT_MS);
}

void loop() {
  rp2040.wdt_reset();

  WindRadioPacket pkt;
  if (!receivePacket(pkt))
    return;
  if (pkt.version != PROTOCOL_VERSION)
    return;

  switch (pkt.type) {
  case PKT_POLL_REQUEST:
    replyStatus();
    break;

  case PKT_SET_RELAY:
    applyRelay(pkt.setRelay.relayOn);
    replyStatus(); // immediate confirmation; next poll re-checks anyway
    break;

  default:
    break;
  }
}

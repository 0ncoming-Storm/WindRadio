/*
 * receiver-nonlatching.ino — non-latching relay node for the fountains
 * (NODE_FOUNTAIN1 / NODE_FOUNTAIN2).
 *
 * Drives a non-latching mains relay for a fountain pump. The relay must be
 * actively held in its commanded state; it defaults to OFF on every boot or
 * reset (including watchdog resets), so a hung node can never leave a pump
 * running unattended.
 *
 * The node is purely passive on the radio: it never transmits unless it is
 * answering the central control node.
 *
 * Node identity is chosen at build time:
 *   make node=receiver-nonlatching flash            -> NODE_FOUNTAIN1
 *   make node=receiver-nonlatching FOUNTAIN=2 flash -> NODE_FOUNTAIN2
 */

#include "WindRadioCommon.h"

#ifdef FOUNTAIN_NODE_ID
#define MY_NODE_ID FOUNTAIN_NODE_ID
#else
#warning "FOUNTAIN_NODE_ID not set, defaulting to NODE_FOUNTAIN1"
#define MY_NODE_ID NODE_FOUNTAIN1
#endif

#define RELAY_PIN 13

// Watchdog period. Generous: the loop only blocks for radio retries.
#define WATCHDOG_TIMEOUT_MS 8000

static RelayState relayState = RELAY_OFF;

static void applyRelay(bool on) {
  digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  relayState = on ? RELAY_ON : RELAY_OFF;
}

static void replyStatus() {
  WindRadioPacket pkt;
  pkt.type = PKT_RELAY_STATUS;
  pkt.relayStatus.nodeId = MY_NODE_ID;
  pkt.relayStatus.relayState = relayState;
  sendPacket(NODE_MAIN, pkt);
}

void setup() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // pumps always start OFF

  radioSetup(MY_NODE_ID);
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

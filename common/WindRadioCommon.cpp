#include "WindRadioCommon.h"

// WindRadioCommon.cpp — shared implementation for radio setup and packet I/O.
// Included by every node (base station + remote sensors) to avoid duplication.

// RFM69 radio instance. Pin layout matches the shared WindRadioCommon.h
// aliases. The 4th argument (PIN_RFM_DIO0) doubles as the RST pin on RFM69HW
// modules.
RFM69 radio(RFM69_CS, RFM69_INT, true, PIN_RFM_DIO0);

// --- Radio Initialization ---
// Resets the RFM69 chip, initializes it on the configured frequency/network,
// enables high-power mode, and sets the encryption key.
void radioSetup(uint8_t myNodeId) {
  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, HIGH); // hold reset
  delay(10);
  digitalWrite(RFM69_RST, LOW); // release reset
  delay(10);

  if (!radio.initialize(FREQUENCY, myNodeId, NETWORKID)) {
    Serial.println("Radio init failed!");
  } else {
    Serial.println("Radio init good");
  }
  radio.setHighPower();      // enable PA boost for RFM69HW
  radio.setPowerLevel(23);   // ~23/31 max power
  radio.encrypt(ENCRYPTKEY); // 16-byte AES key (password must match all nodes)
}

// --- Send a packet and request an ACK ---
// Returns true if the destination ACKed within the retry window.
bool sendPacket(uint8_t toNodeId, const WindRadioPacket &pkt) {
  return radio.sendWithRetry(toNodeId, (const void *)&pkt, sizeof(pkt));
}

// --- Poll for an incoming packet ---
// Returns true and populates outPkt if a valid WindRadioPacket is received.
// Sends an ACK if the sender requested one, even on payload mismatch.
bool receivePacket(WindRadioPacket &outPkt) {
  if (!radio.receiveDone())
    return false;

  if (radio.DATALEN != sizeof(WindRadioPacket)) {
    // Wrong size — not a packet we understand; optionally ACK so sender
    // doesn't waste time retrying.
    if (radio.ACKRequested())
      radio.sendACK();
    return false;
  }

  memcpy(&outPkt, (const void *)radio.DATA, sizeof(outPkt));
  outPkt.fromNode = radio.SENDERID; // stamp who actually sent it

  if (radio.ACKRequested())
    radio.sendACK();

  return true;
}

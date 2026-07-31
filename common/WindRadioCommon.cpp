#include "WindRadioCommon.h"
#include "SerialUSB.h"
#include <cstdint>
#include <sys/_types.h>

RFM69 radio(RFM69_CS, RFM69_INT, true, PIN_RFM_DIO0);

void radioSetup(uint8_t myNodeId) {
  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, HIGH);
  delay(10);
  digitalWrite(RFM69_RST, LOW);
  delay(10);

  if (!radio.initialize(FREQUENCY, myNodeId, NETWORKID)) {
    Serial.println("Radio init failed!");
  } else {
    Serial.println("Radio init good");
  }
  radio.setHighPower();
  radio.setPowerLevel(23);
  radio.encrypt(ENCRYPTKEY);
}

bool sendPacket(uint8_t toNodeId, const WindRadioPacket &pkt) {
  return radio.sendWithRetry(toNodeId, (const void *)&pkt, sizeof(pkt));
}

bool receivePacket(WindRadioPacket &outPkt) {
  if (!radio.receiveDone()) {
    return false;
  }

  if (radio.DATALEN != sizeof(WindRadioPacket)) {
    // Wrong size -- not a packet we understand, drop it.
    if (radio.ACKRequested())
      radio.sendACK();
    return false;
  }

  memcpy(&outPkt, (const void *)radio.DATA, sizeof(outPkt));

  if (radio.ACKRequested()) {
    radio.sendACK();
  }

  return true;
}

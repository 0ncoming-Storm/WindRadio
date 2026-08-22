/*
 * radio-test-ping.ino — minimal radio self-test, TRANSMITTER side.
 *
 * Uses WindRadioCommon (sendPacket / receivePacket) — byte-for-byte what
 * central_control does. Now with per-exchange diagnostics:
 *   - pre-send radio register dump (mode / IRQ flags)
 *   - post-send dump: did the TX actually complete?
 *   - if sendPacket bailed via the new library timeouts it still returns 1,
 *     so we check IRQ flags ourselves to detect a silent failed TX.
 */

#include "WindRadioCommon.h"

#define NODE_ID 90 // this test node
#define PEER_ID 91 // the pong board

static void dumpRadio(const char *tag) {
  Serial.printf("[%lu] %s: opmode=0x%02x irq1=0x%02x irq2=0x%02x\n", millis(),
                tag, (unsigned)radio.readReg(0x01),
                (unsigned)radio.readReg(0x27), (unsigned)radio.readReg(0x28));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000)
    ; // bounded wait so headless boot still proceeds

  radioSetup(NODE_ID);
  Serial.printf("sizeof(WindRadioPacket)=%u\n",
                (unsigned)sizeof(WindRadioPacket));
  Serial.println("Radio init good — sending POLL every 1s");
}

void loop() {
  WindRadioPacket pkt;
  pkt.type = PKT_POLL_REQUEST;

  dumpRadio("pre-send ");
  Serial.printf("[%lu] -> sendPacket(POLL_REQ) to %u\n", millis(), PEER_ID);
  bool ok = sendPacket(PEER_ID, pkt);
  Serial.printf("[%lu] <- sendPacket returned %d\n", millis(), (int)ok);
  // irq2 bit3 = PacketSent (0x08). If clear here the TX was cut short.
  uint8_t irq2 = radio.readReg(0x28);
  if (!(irq2 & 0x08))
    dumpRadio("TX-FAULT ");

  // Listen briefly for the pong node's status reply.
  unsigned long deadline = millis() + 100;
  while ((long)(millis() - deadline) < 0) {
    WindRadioPacket rx;
    if (receivePacket(rx)) {
      Serial.printf("[%lu] <- RX from %u type=%d v%u rssi=%d\n", millis(),
                    rx.fromNode, (int)rx.type, rx.version, (int)radio.RSSI);
      break;
    }
    delay(1);
  }

  delay(1000);
}

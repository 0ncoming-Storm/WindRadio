/*
 * radio-test-pong.ino — minimal radio self-test, RECEIVER side.
 *
 * Same raw-RFM69 test as before, but now receives WindRadioPacket structs
 * via WindRadioCommon (receivePacket) and answers polls with a real
 * PKT_RELAY_STATUS sent through sendPacketRetried() — byte-for-byte what a
 * receiver does. If THIS hangs while the plain-text version worked, the
 * problem is in WindRadioCommon or the packet struct, not the radio.
 */

#include "WindRadioCommon.h"

#define NODE_ID 91 // this test node

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000)
    ; // bounded wait so headless boot still proceeds

  radioSetup(NODE_ID);
  Serial.printf("sizeof(WindRadioPacket)=%u\n",
                (unsigned)sizeof(WindRadioPacket));
  Serial.println("Radio init good — answering POLLs with RELAY_STATUS");
}

void loop() {
  WindRadioPacket pkt;
  if (!receivePacket(pkt))
    return;

  Serial.printf("[%lu] <- RX from %u type=%d v%u\n", millis(), pkt.fromNode,
                (int)pkt.type, pkt.version);
  if (pkt.version != PROTOCOL_VERSION)
    return;
  if (pkt.type != PKT_POLL_REQUEST)
    return;

  Serial.printf("[%lu] -> replyStatus via sendPacketRetried (x%d, %dms)\n",
                millis(), STATUS_SEND_ATTEMPTS, STATUS_RETRY_DELAY_MS);
  WindRadioPacket reply;
  reply.type = PKT_RELAY_STATUS;
  reply.relayStatus.nodeId = NODE_ID;
  reply.relayStatus.relayState = RELAY_ON;
  sendPacketRetried(pkt.fromNode, reply, STATUS_SEND_ATTEMPTS,
                    STATUS_RETRY_DELAY_MS);
  Serial.printf("[%lu] <- replyStatus done\n", millis());
}

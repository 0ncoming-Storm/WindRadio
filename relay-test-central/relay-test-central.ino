/*
 * relay-test-central.ino — manual relay toggle console for the base station.
 *
 * Runs the radio via WindRadioCommon but ZERO control logic: no polling
 * cycles, no scheduling, no desired-state computation. Just a serial menu:
 *
 *   1) Toggle GATE   (node 3, PKT_SET_RELAY)
 *   2) Toggle POND   (node 2, PKT_SET_POND)
 *   3) Toggle FOUNTAIN1 (node 4, PKT_SET_RELAY)
 *   4) Toggle FOUNTAIN2 (node 5, PKT_SET_RELAY)
 *   p) Poll a node's status (asks which node first)
 *   s) Show last known states
 *
 * Type the option + Enter in the Arduino Serial Monitor. The base tracks
 * only what it has been TOLD — no automatic state assumptions.
 */

#include "WindRadioCommon.h"

struct NodeState {
  const char *name;
  uint8_t nodeId;
  PacketType cmdType; // PKT_SET_RELAY or PKT_SET_POND
  bool on;            // last commanded state
};

static NodeState nodes[4] = {
    {"GATE", NODE_GATE, PKT_SET_RELAY, false},
    {"POND", NODE_POND, PKT_SET_POND, false},
    {"FOUNTAIN1", NODE_FOUNTAIN1, PKT_SET_RELAY, false},
    {"FOUNTAIN2", NODE_FOUNTAIN2, PKT_SET_RELAY, false},
};

static void printMenu() {
  Serial.println("\n=== Relay Test Console ===");
  for (uint8_t i = 0; i < 4; i++) {
    Serial.printf("  %d) toggle %s (currently %s)\n", i + 1, nodes[i].name,
                  nodes[i].on ? "ON" : "OFF");
  }
  Serial.println("  p) poll node status");
  Serial.println("  s) show states");
  Serial.print("> ");
}

// Wait up to timeoutMs for a status packet from `fromNode`, discarding it.
static bool waitStatus(uint8_t fromNode, PacketType expectType,
                       unsigned long timeoutMs, WindRadioPacket &out) {
  unsigned long deadline = millis() + timeoutMs;
  while ((long)(millis() - deadline) < 0) {
    WindRadioPacket rx;
    if (receivePacket(rx)) {
      if (rx.version == PROTOCOL_VERSION && rx.fromNode == fromNode &&
          rx.type == expectType) {
        out = rx;
        return true;
      }
      Serial.printf("  (ignored stray pkt type=%d from=%u)\n", (int)rx.type,
                    rx.fromNode);
    }
    delay(1);
  }
  return false;
}

static void toggleNode(uint8_t idx) {
  NodeState &n = nodes[idx];
  bool target = !n.on;

  WindRadioPacket cmd;
  cmd.type = n.cmdType;
  cmd.setRelay.relayOn = target;

  Serial.printf("-> %s SET_RELAY=%s\n", n.name, target ? "ON" : "OFF");
  sendPacket(n.nodeId, cmd);

  PacketType expect = (n.cmdType == PKT_SET_POND) ? PKT_POND_STATUS
                                                  : PKT_RELAY_STATUS;
  WindRadioPacket reply;
  if (waitStatus(n.nodeId, expect, 500, reply)) {
    RelayState reported =
        (expect == PKT_POND_STATUS) ? reply.pondStatus.pumpState
                                    : reply.relayStatus.relayState;
    Serial.printf("<- %s replied, reports relay=%s\n", n.name,
                  reported == RELAY_ON    ? "ON"
                  : reported == RELAY_OFF ? "OFF"
                                          : "UNKNOWN");
    n.on = (reported == RELAY_ON); // trust the node's report, not our wish
  } else {
    Serial.printf("<- %s: NO REPLY within 500ms\n", n.name);
    // Don't update n.on — the command may still have landed.
  }
}

static void pollNode() {
  Serial.print("Poll which node? 1=GATE 2=POND 3=FN1 4=FN2\n> ");
  while (!Serial.available())
    delay(10);
  char c = Serial.read();
  Serial.println(c);
  if (c < '1' || c > '4') {
    Serial.println("invalid");
    return;
  }
  NodeState &n = nodes[c - '1'];

  WindRadioPacket req;
  req.type = PKT_POLL_REQUEST;
  Serial.printf("-> %s POLL_REQ\n", n.name);
  sendPacket(n.nodeId, req);

  PacketType expect = (n.nodeId == NODE_POND) ? PKT_POND_STATUS
                                              : PKT_RELAY_STATUS;
  WindRadioPacket reply;
  if (waitStatus(n.nodeId, expect, 500, reply)) {
    if (expect == PKT_RELAY_STATUS) {
      Serial.printf("<- %s relay=%s\n", n.name,
                    reply.relayStatus.relayState == RELAY_ON    ? "ON"
                    : reply.relayStatus.relayState == RELAY_OFF ? "OFF"
                                                                : "UNKNOWN");
      n.on = (reply.relayStatus.relayState == RELAY_ON);
    } else {
      Serial.printf("<- POND time=%02u:%02u wind=%d temp=%.1f pump=%s rtcOk=%d\n",
                    reply.pondStatus.hours, reply.pondStatus.minutes,
                    reply.pondStatus.windSpeed,
                    (double)reply.pondStatus.temperature,
                    reply.pondStatus.pumpState == RELAY_ON    ? "ON"
                    : reply.pondStatus.pumpState == RELAY_OFF ? "OFF"
                                                              : "UNKNOWN",
                    (int)reply.pondStatus.rtcOk);
      n.on = (reply.pondStatus.pumpState == RELAY_ON);
    }
  } else {
    Serial.printf("<- %s: NO REPLY\n", n.name);
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000)
    ;
  radioSetup(NODE_MAIN);
  printMenu();
}

void loop() {
  if (!Serial.available())
    return;

  char c = Serial.read();
  Serial.println(c);
  switch (c) {
  case '1':
  case '2':
  case '3':
  case '4':
    toggleNode(c - '1');
    break;
  case 'p':
  case 'P':
    pollNode();
    break;
  case 's':
  case 'S':
    for (uint8_t i = 0; i < 4; i++)
      Serial.printf("%-10s %s\n", nodes[i].name, nodes[i].on ? "ON" : "OFF");
    break;
  case '\r':
  case '\n':
    return; // ignore newlines after commands
  default:
    Serial.println("? (1-4, p, s)");
  }
  printMenu();
}

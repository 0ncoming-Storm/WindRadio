#pragma once
#include <Arduino.h>
#include <RFM69.h>
#include <SPI.h>
#include <Wire.h>
#include <stdint.h>

// WindRadioCommon.h — shared definitions used by all nodes (base station,
// pond, gate, fountains). Defines the radio protocol version, node addresses,
// hardware pin mappings, and the fixed-size packet structure.

// --- Protocol Version ---
// v4: no RFM69 hardware ACK stack. Reliability comes from application-level
// retransmits plus the rule that every request is answered with a data packet.
// v5: PKT_ACK removed entirely — commands are confirmed by a status packet too,
// and status replies are sent with retries.
#define PROTOCOL_VERSION 5

// --- Relay State (wire format) ---
// Latching relays (e.g. the gate) keep their position through power loss,
// so a node may genuinely not know its state until the first command.
enum RelayState : uint8_t {
  RELAY_OFF = 0,
  RELAY_ON = 1,
  RELAY_UNKNOWN = 2,
};

// --- Physical Node Addresses ---
#define NODE_MAIN 1
#define NODE_POND 2
#define NODE_GATE 3
#define NODE_FOUNTAIN1 4
#define NODE_FOUNTAIN2 5

// --- Shared Pin Definitions ---
#define RFM69_CS PIN_RFM_CS
#define RFM69_INT PIN_RFM_DIO0
#define RFM69_RST PIN_RFM_RST

// --- Shared Radio Config ---
#define NETWORKID 100
#define FREQUENCY RF69_915MHZ
#define ENCRYPTKEY "WindRadioEstate1" // MUST be exactly 16 chars (AES-128)
#define IS_RFM69HW_HCW

// Status replies are blind-retried this many times; the base takes the first
// one that arrives and duplicates are harmless (absolute snapshots).
#define STATUS_SEND_ATTEMPTS 3
#define STATUS_RETRY_DELAY_MS 20

// --- Packet Types ---
enum PacketType : uint8_t {
  PKT_POLL_REQUEST, // Sent by base to request a status update from a node;
                    // the node's status reply doubles as the acknowledgement
  PKT_POND_STATUS,  // Pond node -> base: wind speed + relay state
  PKT_RELAY_STATUS, // Relay node -> base: relay on/off + error counters
  PKT_SET_RELAY,    // Base -> relay node: command relay on/off
  PKT_SET_POND,     // Base -> pond node: command pond relay on/off
};

// --- Single Fixed-Size Packet ---
// Packed so struct size is identical on every platform — critical for radio.
#pragma pack(push, 1)
struct WindRadioPacket {
  uint8_t version = PROTOCOL_VERSION;
  PacketType type;
  uint8_t fromNode = 0; // node ID of the packet's sender (stamped on receive)

  union {
    struct { // PKT_POND_STATUS
      uint8_t hours;
      uint8_t minutes;
      uint8_t month;  // 1-12; needed by the base for DST conversion
      uint8_t day;    // 1-31
      int windSpeed;
      float temperature;
      RelayState pumpState;
      bool rtcOk; // false = node's RTC is dead; time/temp are not real data
    } pondStatus;

    struct { // PKT_RELAY_STATUS
      uint8_t nodeId;
      RelayState relayState;
    } relayStatus;

    struct { // PKT_SET_RELAY / PKT_SET_POND
      bool relayOn; // absolute target state, so retries are idempotent
    } setRelay;
  };
};
#pragma pack(pop)

// --- Shared Objects (Defined in WindRadioCommon.cpp) ---
extern RFM69 radio;

// --- Shared Function Prototypes ---
void radioSetup(uint8_t myNodeId);

// Fire-and-forget transmit (no hardware ACK, no retries). Returns true if
// the radio accepted the packet for transmission.
bool sendPacket(uint8_t toNodeId, const WindRadioPacket &pkt);

// Application-level retransmit: blind-sends the packet up to `attempts`
// times with `retryDelayMs` between sends. Delivery is NOT confirmed here;
// callers verify by waiting for the expected reply themselves.
void sendPacketRetried(uint8_t toNodeId, const WindRadioPacket &pkt,
                       uint8_t attempts, unsigned long retryDelayMs);

// Poll for an incoming packet. Returns true and populates outPkt if a valid
// WindRadioPacket is received. Never acknowledges anything at the radio
// level — replies are the caller's decision.
bool receivePacket(WindRadioPacket &outPkt);

// Send a custom application-level ACK confirming that `ackedType` was
// received and fully processed (nodes acknowledge commands only after
// acting on them).


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
#define PROTOCOL_VERSION 3

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

// --- Packet Types ---
enum PacketType : uint8_t {
  PKT_POLL_REQUEST, // Sent by base to request a status update from a node
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
      int windSpeed;
      float temperature;
      RelayState pumpState;
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
bool sendPacket(uint8_t toNodeId, const WindRadioPacket &pkt);
bool receivePacket(WindRadioPacket &outPkt);

#pragma once
#include <Arduino.h>

#define PROTOCOL_VERSION 1

// Physical node addresses
#define NODE_MAIN 1
#define NODE_POND 2
#define NODE_GATE 3
#define NODE_FOUNTAIN1 4
#define NODE_FOUNTAIN2 5

enum PacketType : uint8_t {
  PKT_POLL_REQUEST,
  PKT_POND_STATUS,
  PKT_RELAY_STATUS,
  PKT_SET_RELAY,
  PKT_SET_POND,
};

struct WindRadioPacket {
  uint8_t version = PROTOCOL_VERSION;
  PacketType type;

  union {
    struct { // PKT_POND_STATUS
      uint8_t hours;
      uint8_t minutes;
      int windSpeed;
      bool relayOn;
    } pondStatus;

    struct { // PKT_RELAY_STATUS
      uint8_t nodeId;
      bool relayOn;
    } relayStatus;

    struct { // PKT_SET_RELAY / PKT_SET_POND (same shape, reused)
      bool relayOn;
    } setRelay;
  };
};

// Assuming these are defined in your underlying radio .cpp file
extern void radioSetup(uint8_t nodeId);
extern bool sendPacket(uint8_t destNodeId, const WindRadioPacket &packet);
extern bool receivePacket(WindRadioPacket &outPacket);

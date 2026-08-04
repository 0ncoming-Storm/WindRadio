#pragma once
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <RFM69.h>
#include <SPI.h>
#include <Wire.h>
#include <stdint.h>

// WindRadioCommon.h — shared definitions used by all nodes (base station,
// pond, gate, fountains). Defines the radio protocol version, node addresses,
// hardware pin mappings, and the fixed-size packet structure.

// --- Protocol Version ---
#define PROTOCOL_VERSION 1

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
#define NEOPIXEL_PIN 4
#define NUM_PIXELS 1

// --- Shared Radio Config ---
#define NETWORKID 100
#define FREQUENCY RF69_915MHZ
#define ENCRYPTKEY "WindRadio"
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

  union {
    struct { // PKT_POND_STATUS
      uint8_t hours;
      uint8_t minutes;
      int windSpeed;
      float temperature;
      bool relayOn;
    } pondStatus;

    struct { // PKT_RELAY_STATUS
      uint8_t nodeId;
      bool relayOn;
    } relayStatus;

    struct { // PKT_SET_RELAY / PKT_SET_POND
      bool relayOn;
    } setRelay;
  };
};
#pragma pack(pop)

// --- Shared Objects (Defined in WindRadioCommon.cpp) ---
extern RFM69 radio;
extern Adafruit_NeoPixel strip;

// --- Shared Function Prototypes ---
void blinkNeoPixel(uint8_t red, uint8_t green, uint8_t blue, uint16_t delay_ms,
                   uint8_t blinks);
void radioSetup(uint8_t myNodeId);
bool sendPacket(uint8_t toNodeId, const WindRadioPacket &pkt);
bool receivePacket(WindRadioPacket &outPkt);

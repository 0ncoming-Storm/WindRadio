#pragma once
#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <RFM69.h>
#include <SPI.h>
#include <Wire.h>
#include <stdint.h>

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
  PKT_POLL_REQUEST,
  PKT_POND_STATUS,
  PKT_RELAY_STATUS,
  PKT_SET_RELAY,
  PKT_SET_POND,
};

// --- Single Fixed-Size Packet ---
#pragma pack(push, 1)
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

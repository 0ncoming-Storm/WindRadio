#pragma once
#include <Adafruit_NeoPixel.h>
#include <RFM69.h>
#include <SPI.h>
#include <Wire.h>

// Shared pin defs
#define RFM69_CS PIN_RFM_CS
#define RFM69_INT PIN_RFM_DIO0
#define RFM69_RST PIN_RFM_RST
#define NEOPIXEL_PIN 4
#define NUM_PIXELS 1

// Shared radio config
#define NETWORKID 100
#define FREQUENCY RF69_915MHZ
#define ENCRYPTKEY "WindRadio"
#define IS_RFM69HW_HCW
// Shared objects (defined in the .cpp, declared extern here)
extern RFM69 radio;
extern Adafruit_NeoPixel strip;

// Shared stucts
#pragma once
#include <stdint.h>

// --- Boolean flag bit positions ---
#define GATE_STATUS (1 << 0)
#define LARGE_FOUNTAIN_STATUS (1 << 1)
#define SMALL_FOUNTAIN_STATUS (1 << 2)
#define TRIGGER_GATE_NOW (1 << 3)

// add more as needed, up to 16 flags in a uint16_t (32 if you use uint32_t)

#pragma pack(push, 1)
struct WindRadioPacket {
  uint32_t timestamp;  // Unix time from RTC (or 0 / millis-based if no RTC)
  uint16_t wind_speed; // your primary integer payload
  uint16_t flags;      // bitmask of up to 16 booleans
  float temperature;   // example of an arbitrary float field
  uint8_t nodeId;      // optional: sender ID, useful with multiple nodes
};
#pragma pack(pop)

// Shared functions
void blinkNeoPixel(uint8_t red, uint8_t green, uint8_t blue, uint16_t delay_ms,
                   uint8_t blinks);
void radioSetup(uint8_t myNodeId);

// Radio packet helpers
bool sendPacket(uint8_t toNodeId, const WindRadioPacket &pkt);
bool receivePacket(WindRadioPacket &outPkt);

// String helpers
String to_upper(String str);

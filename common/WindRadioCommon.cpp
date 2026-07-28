#include "WindRadioCommon.h"
#include "SerialUSB.h"
#include "pico/time.h"

RFM69 radio(RFM69_CS, RFM69_INT, true, PIN_RFM_DIO0);
Adafruit_NeoPixel strip(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Hidden file-scope variables (invisible outside this file)
static struct repeating_timer blinkTimer;
static uint8_t b_r = 0, b_g = 0, b_b = 0;
static uint8_t remainingToggles = 0;
static bool pixelOn = false;

// Hardware interrupt callback (runs automatically in background)
static bool blinkTimerCallback(struct repeating_timer *t) {
  if (remainingToggles == 0)
    return false;

  pixelOn = !pixelOn;
  strip.setPixelColor(0, pixelOn ? strip.Color(b_r, b_g, b_b) : 0);
  strip.show();

  remainingToggles--;
  return (remainingToggles > 0); // Returning false auto-destroys the timer
}

void blinkNeoPixel(uint8_t r, uint8_t g, uint8_t b, uint16_t delay_ms,
                   uint8_t blinks) {
  if (blinks == 0)
    return;

  // Stop any ongoing blink sequence
  cancel_repeating_timer(&blinkTimer);

  b_r = r;
  b_g = g;
  b_b = b;
  remainingToggles = (blinks * 2) - 1;

  // Turn ON immediately
  strip.setPixelColor(0, strip.Color(r, g, b));
  strip.show();
  pixelOn = true;

  // Start hardware timer (-delay_ms ensures fixed execution interval)
  add_repeating_timer_ms(-delay_ms, blinkTimerCallback, NULL, &blinkTimer);
}
void radioSetup(uint8_t myNodeId) {
  strip.begin();
  strip.show();

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

String to_upper(String str) {
  for (auto &c : str) {
    c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
  }
  return str;
}

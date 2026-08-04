#include "RTClib.h"
#include "SerialUSB.h"
#include "WindRadioCommon.h"
#include "api/String.h"

// brain.ino — WindRadio "pond sensor" node.
//
// Runs on an Adafruit Feather RP2040 with:
//   RFM69HW radio for communication with the base station.
//   DS3231 RTC for timekeeping (I2C).
//   Analog wind sensor on GPIO26.
//   NeoPixel status LED on GPIO4.
//
// Role: polls the wind sensor + RTC, bundles the data into a WindRadioPacket,
// and transmits it to the base station (NODE_MAIN) every 700 ms.

#define MYNODEID 1 // This pond node's radio address
#define TONODEID 2 // Destination base station address
RTC_DS3231 rtc;

void setup() {
  Serial.begin(115200);
  //  while (!Serial)
  //  delay(1);
  radioSetup(MYNODEID);
  Serial.println("Node " + String(MYNODEID) + " up.");
  blinkNeoPixel(0, 0, 255, 1000, 1); // blue = starting up
  Serial.println("Feather RP2040 Radio Initialized successfully.");
  // Initialize I2C communication
  if (!rtc.begin()) {
    Serial.println("Error: Could not find RTC. Check your connections.");
    while (1)
      ; // Halt execution if RTC is missing
  }
  analogReadResolution(12);
}

// Main Loop: Runs continuously after setup.
// Reads wind sensor + RTC, then sends a status packet to the base.
void loop() {
  Serial.print("CS: ");
  Serial.println(RFM69_CS);
  Serial.print("INT: ");
  Serial.println(RFM69_INT);
  Serial.print("RST: ");
  Serial.println(RFM69_RST);
  Serial.print("SDA: ");
  Serial.println(SDA);
  Serial.print("SCL: ");
  Serial.println(SCL);

  WindRadioPacket pck;
  pck.temperature = rtc.getTemperature();
  pck.timestamp = rtc.now().unixtime();
  pck.wind_speed = readWind(26);

  Serial.println(pck.temperature);
  Serial.println(pck.timestamp);

  if (sendPacket(TONODEID, pck)) {
    blinkNeoPixel(0, 255, 0, 100, 1); // green = send OK
  } else {
    blinkNeoPixel(255, 0, 0, 100, 1); // red = send failed
  }
  delay(700);
}

// Read wind speed from a proportional-wind analog sensor.
// Returns the speed in tenths of m/s (e.g. 50 = 5.0 m/s).
int readWind(int windPin) {
  analogReadResolution(12);

  int rawValue = analogRead(windPin);

  // 0.4V maps to ~496 raw ADC value on a 3.3V RP2040
  // 2.0V maps to ~2482 raw ADC value on a 3.3V RP2040
  const int RAW_MIN = 496;
  const int RAW_MAX = 2482;

  // Wind speed limits scaled by 10 (0.5 m/s to 50.0 m/s)
  const int SPEED_MIN = 5;   // 0.5 m/s
  const int SPEED_MAX = 500; // 50.0 m/s

  // Return 0 if below the sensor's minimum threshold
  if (rawValue < RAW_MIN) {
    return 0;
  }

  // Clamp to maximum measurable range
  if (rawValue > RAW_MAX) {
    return SPEED_MAX;
  }

  // Pure integer linear mapping with built-in rounding
  long numerator = (long)(rawValue - RAW_MIN) * (SPEED_MAX - SPEED_MIN);
  long denominator = RAW_MAX - RAW_MIN; // 1986

  // Adding (denominator / 2) achieves rounding instead of truncation
  int windSpeedTenths =
      SPEED_MIN + (numerator + (denominator / 2)) / denominator;

  return windSpeedTenths;
}

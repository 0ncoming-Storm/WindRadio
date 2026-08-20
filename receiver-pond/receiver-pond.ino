/*
 * receiver-pond.ino — pond sensor + pump node (NODE_POND).
 *
 * The system's environmental source: measures wind speed and temperature
 * and keeps the current time of day. The central control node derives all
 * scheduling and wind-safety decisions from this node's reports, so its
 * clock and readings must be accurate.
 *
 * Hardware:
 *   - Anemometer on an analog input (0.4–2.0 V -> speed; see mapWindSpeed).
 *   - DS3231 RTC over I2C. The RTC is battery-backed and already set —
 *     this node only reads it, never writes it.
 *   - Non-latching relay for the pond pump. Defaults to OFF on every boot
 *     or reset (including watchdog resets).
 *
 * The node is purely passive on the radio: it never transmits unless it is
 * answering the central control node.
 */

#include "WindRadioCommon.h"
#include <RTClib.h>
#include <Wire.h>

#define PUMP_RELAY_PIN 13
#define WIND_SENSOR_PIN A0

// Watchdog period. Generous: the loop only blocks for radio retries.
#define WATCHDOG_TIMEOUT_MS 8000

static RTC_DS3231 rtc;
static bool rtcOk = false;

static RelayState pumpState = RELAY_OFF;

static void applyPump(bool on) {
  digitalWrite(PUMP_RELAY_PIN, on ? HIGH : LOW);
  pumpState = on ? RELAY_ON : RELAY_OFF;
}

// TODO(calibration): placeholder mapping from the first hardware attempt.
// Replace with the real transfer function once the anemometer datasheet is
// available. Assumes a 10-bit ADC (0–1023) over 3.3 V and a linear
// 0.4–2.0 V output span covering 0–32.4 km/h.
static float mapWindSpeed(int raw) {
  float voltage = raw * 3.3f / 1024.0f;
  if (voltage < 0.4f)
    return 0.0f;
  if (voltage > 2.0f)
    return 32.4f;
  return (voltage - 0.4f) / (2.0f - 0.4f) * 32.4f;
}

static void replyStatus() {
  WindRadioPacket pkt;
  pkt.type = PKT_POND_STATUS;

  uint8_t hours = 0, minutes = 0;
  float temperature = 0.0f;
  if (rtcOk) {
    DateTime now = rtc.now();
    hours = now.hour();
    minutes = now.minute();
    temperature = rtc.getTemperature();
  }

  pkt.pondStatus.hours = hours;
  pkt.pondStatus.minutes = minutes;
  pkt.pondStatus.windSpeed = (int)mapWindSpeed(analogRead(WIND_SENSOR_PIN));
  pkt.pondStatus.temperature = temperature;
  pkt.pondStatus.pumpState = pumpState;
  sendPacket(NODE_MAIN, pkt);
}

void setup() {
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  digitalWrite(PUMP_RELAY_PIN, LOW); // pump always starts OFF

  Wire.begin();
  rtcOk = rtc.begin();
  if (!rtcOk)
    Serial.println("RTC failed; reporting zeroed time/temp");

  radioSetup(NODE_POND);
  Serial.begin(115200);

  rp2040.wdt_begin(WATCHDOG_TIMEOUT_MS);
}

void loop() {
  rp2040.wdt_reset();

  WindRadioPacket pkt;
  if (!receivePacket(pkt))
    return;
  if (pkt.version != PROTOCOL_VERSION)
    return;

  switch (pkt.type) {
  case PKT_POLL_REQUEST:
    replyStatus();
    break;

  case PKT_SET_POND:
    applyPump(pkt.setRelay.relayOn);
    replyStatus(); // immediate confirmation; next poll re-checks anyway
    break;

  default:
    break;
  }
}

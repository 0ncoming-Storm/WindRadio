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

// Set to 0 to silence serial debug (clock dumps etc).
#define POND_DEBUG 1

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
  uint8_t seconds = 0;
  float temperature = 0.0f;
  if (rtcOk) {
    DateTime now = rtc.now();
    hours = now.hour();
    minutes = now.minute();
    seconds = now.second();
    temperature = rtc.getTemperature();
  }

#if POND_DEBUG
  // Clock debug: full timestamp + sanity checks on every status reply.
  if (rtcOk) {
    DateTime now = rtc.now();
    bool sane = now.year() >= 2024 && now.year() <= 2099 &&
                now.month() >= 1 && now.month() <= 12 &&
                now.day() >= 1 && now.day() <= 31 &&
                now.hour() < 24 && now.minute() < 60 && now.second() < 60;
    Serial.printf("[%lu] RTC: %04u-%02u-%02u %02u:%02u:%02u %s\n", millis(),
                  (unsigned)now.year(), (unsigned)now.month(),
                  (unsigned)now.day(), (unsigned)now.hour(),
                  (unsigned)now.minute(), (unsigned)now.second(),
                  sane ? "ok" : "OUT OF RANGE");
    if (!sane)
      Serial.println("RTC: WARNING - date/time implausible, clock may be unset");
  } else {
    Serial.printf("[%lu] RTC: DEAD - reporting zeros\n", millis());
  }
#endif

  pkt.pondStatus.hours = hours;
  pkt.pondStatus.minutes = minutes;
  if (rtcOk) {
    DateTime now = rtc.now();
    pkt.pondStatus.month = now.month();
    pkt.pondStatus.day = now.day();
  }
  pkt.pondStatus.windSpeed = (int)mapWindSpeed(analogRead(WIND_SENSOR_PIN));
  pkt.pondStatus.temperature = temperature;
  pkt.pondStatus.pumpState = pumpState;
  pkt.pondStatus.rtcOk = rtcOk;
  // Retried blind sends: the base accepts the first valid reply, and
  // duplicates are harmless because the packet is an absolute snapshot.
  sendPacketRetried(NODE_MAIN, pkt, STATUS_SEND_ATTEMPTS,
                    STATUS_RETRY_DELAY_MS);
}

// --- Serial clock setting (bench tool, POND_DEBUG builds only) ---
// Protocol: host sends "T<epoch>" where <epoch> is a UTC unix timestamp.
// The sketch applies it to the DS3231 and echoes the resulting LOCAL
// date/time for verification. The host is responsible for converting to
// the desired timezone before sending.
#if POND_DEBUG
static uint32_t pendingEpoch = 0;      // epoch awaiting confirmation
static unsigned long setClockArmedAt = 0;
static const unsigned long SETCLOCK_WINDOW_MS = 10000;

static void handleSetClock() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == 'T' || c == 't') { // start of T<epoch> command
      String num;
      unsigned long start = millis();
      while ((millis() - start) < 3000) {
        if (!Serial.available()) {
          delay(1);
          continue;
        }
        char d = Serial.read();
        if (d == '\n' || d == '\r')
          break;
        if (isDigit(d))
          num += d;
      }
      if (num.length() == 0) {
        Serial.println("SETCLOCK ERR: no digits after T");
        return;
      }
      pendingEpoch = (uint32_t)num.toInt();
      rtc.adjust(DateTime(pendingEpoch));
      DateTime now = rtc.now();
      Serial.printf("RTC SET: %04u-%02u-%02u %02u:%02u:%02u\n",
                    (unsigned)now.year(), (unsigned)now.month(),
                    (unsigned)now.day(), (unsigned)now.hour(),
                    (unsigned)now.minute(), (unsigned)now.second());
      return;
    }
  }
}
#endif // POND_DEBUG

void setup() {
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  digitalWrite(PUMP_RELAY_PIN, LOW); // pump always starts OFF
   while (!Serial) {
    ; // Do nothing, just loop
  }
  Wire.begin();
  rtcOk = rtc.begin();
  if (!rtcOk) {
    Serial.println("RTC failed; reporting zeroed time/temp");
  } else {
#if POND_DEBUG
    DateTime now = rtc.now();
    Serial.printf("RTC ok at boot: %04u-%02u-%02u %02u:%02u:%02u\n",
                  (unsigned)now.year(), (unsigned)now.month(),
                  (unsigned)now.day(), (unsigned)now.hour(),
                  (unsigned)now.minute(), (unsigned)now.second());
#endif
  }

  radioSetup(NODE_POND);
  Serial.begin(115200);

  rp2040.wdt_begin(WATCHDOG_TIMEOUT_MS);
}

void loop() {
  rp2040.wdt_reset();

#if POND_DEBUG
  // Bench clock-setting: watch serial for T<epoch> commands. Only active in
  // debug builds — deployed firmware ignores serial entirely.
  if (Serial.available() > 0) {
    char peeked = (char)Serial.peek();
    if (peeked == 'T' || peeked == 't') {
      handleSetClock();
      return; // done this loop, radio next time
    }
    // Drain other stray characters so they don't accumulate.
    while (Serial.available())
      Serial.read();
  }
#endif

  // Idle sleep: receivePacket() returns in microseconds when no packet is
  // waiting, so without this the core would spin at 100% CPU (and run hot)
  // polling the radio over SPI. Worst-case command latency is ~2ms.
  delay(2);

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
    // Protocol v5: no separate ACK packet. The freshly-applied pump state is
    // the confirmation — reply with a status snapshot (retried).
    replyStatus();
    break;

  default:
    break;
  }
}

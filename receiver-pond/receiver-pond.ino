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

// --- Wind sampling / filtering ---
// The anemometer feeds a SAFETY decision, so the reported value must
// represent recent wind, not one noisy instant. Pipeline:
//   * every 100 ms take the MEDIAN of 3 ADC samples (rejects single-shot
//     spikes from vibration / electrical noise),
//   * keep a 30-second rolling window of those filtered samples,
//   * report the window MAX.
// The peak is the conservative choice for shutoff logic (a gust that
// outlasts one sample tick is always captured; under-reporting wind would
// keep the gate open in gusts), and a window max only changes when the
// maximum changes — smooth, no per-sample jitter on the base's display.
#define WIND_SAMPLE_MS 100
#define WIND_WINDOW_MS 30000
#define WIND_WINDOW_SAMPLES (WIND_WINDOW_MS / WIND_SAMPLE_MS)

static uint8_t windWindow[WIND_WINDOW_SAMPLES];
static uint16_t windWindowIdx = 0;

static int medianOf3(int a, int b, int c) {
  // Three conditional swaps sort the triple ascending (a <= b <= c).
  if (a > b) {
    int t = a;
    a = b;
    b = t;
  }
  if (b > c) {
    int t = b;
    b = c;
    c = t;
  }
  if (a > b) {
    int t = a;
    a = b;
    b = t;
  }
  return b; // median
}

// Call from loop(): takes one filtered sample when due.
static void windTick() {
  static unsigned long lastSampleMs = 0;
  unsigned long now = millis();
  if (now - lastSampleMs < WIND_SAMPLE_MS)
    return;
  lastSampleMs = now;
  int med = medianOf3(analogRead(WIND_SENSOR_PIN),
                      analogRead(WIND_SENSOR_PIN),
                      analogRead(WIND_SENSOR_PIN));
  float kmh = mapWindSpeed(med);
  if (kmh > 255.0f)
    kmh = 255.0f; // window is uint8; the current map caps far below this
  windWindow[windWindowIdx] = (uint8_t)kmh;
  windWindowIdx = (uint16_t)((windWindowIdx + 1) % WIND_WINDOW_SAMPLES);
}

// Max over the rolling window (0 until the first samples land).
static int currentWindKmh() {
  int maxKmh = 0;
  for (uint16_t i = 0; i < WIND_WINDOW_SAMPLES; i++)
    if (windWindow[i] > maxKmh)
      maxKmh = windWindow[i];
  return maxKmh;
}

static void replyStatus() {
  WindRadioPacket pkt;
  pkt.type = PKT_POND_STATUS;

  uint8_t hours = 0, minutes = 0;
  uint8_t weekday = 0;
  float temperature = 0.0f;
  if (rtcOk) {
    DateTime now = rtc.now();
    hours = now.hour();
    minutes = now.minute();
    weekday = now.dayOfTheWeek(); // 0=Sunday..6=Saturday (base DST test)
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
    pkt.pondStatus.weekday = weekday;
  }
  // Filtered value: median-of-3 @100ms samples, max over the last 30 s
  // (see windTick / currentWindKmh above).
  pkt.pondStatus.windSpeed = currentWindKmh();
  pkt.pondStatus.temperature = temperature;
  pkt.pondStatus.pumpState = pumpState;
  pkt.pondStatus.rtcOk = rtcOk;
  // Retried blind sends: the base accepts the first valid reply, and
  // duplicates are harmless because the packet is an absolute snapshot.
  sendPacketRetried(NODE_MAIN, pkt, STATUS_SEND_ATTEMPTS,
                    STATUS_RETRY_DELAY_MS);
}

void setup() {
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  digitalWrite(PUMP_RELAY_PIN, LOW); // pump always starts OFF

  // Serial FIRST so the RTC/radio init results below are actually visible.
  Serial.begin(115200);

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

  rp2040.wdt_begin(WATCHDOG_TIMEOUT_MS);
}

void loop() {
  rp2040.wdt_reset();

  // Idle sleep: receivePacket() returns in microseconds when no packet is
  // waiting, so without this the core would spin at 100% CPU (and run hot)
  // polling the radio over SPI. Worst-case command latency is ~2ms.
  delay(2);

  windTick(); // keep the filtered wind sample stream fresh

  // Fail-safe: no base poll for 6+ minutes means the base is dead or wedged.
  // The state check makes this act ONCE per silence period, not every loop.
  if (basePollExpired() && pumpState != RELAY_OFF) {
    applyPump(false);
    Serial.printf("[%lu] FAILSAFE: no base poll for 6+ min - pump OFF\n",
                  millis());
  }

  WindRadioPacket pkt;
  if (!receivePacket(pkt))
    return;
  if (pkt.version != PROTOCOL_VERSION)
    return;

  switch (pkt.type) {
  case PKT_POLL_REQUEST:
    if (pkt.fromNode == NODE_MAIN)
      markBasePollSeen(); // liveness for the 6-minute fail-safe above
    replyStatus();
    break;

  case PKT_SET_POND:
    applyPump(pkt.setRelay.relayOn);
    // Protocol v6: no separate ACK packet. The freshly-applied pump state is
    // the confirmation — reply with a status snapshot (retried).
    replyStatus();
    break;

  default:
    break;
  }
}

/*
 * rtc-set-clock.ino — one-off bench sketch to set the DS3231 RTC.
 *
 * NOT part of the deployed system. Flash this to the pond board, run
 * tools/pond-set-clock.sh, verify the printed time, then flash the real
 * receiver-pond firmware back. The RTC is battery-backed, so the time
 * survives the reflash — this is the intended workflow:
 *
 *   1. make node=rtc-set-clock flash
 *   2. ./tools/pond-set-clock.sh [port]
 *   3. make node=receiver-pond flash
 *
 * Listens for "T<epoch>" (UTC unix seconds) and writes it to the RTC,
 * then prints the resulting date/time once per second so you can watch
 * it tick and confirm correctness before reflashing.
 */

#include <RTClib.h>
#include <Wire.h>

static RTC_DS3231 rtc;

void setup() {
  Serial.begin(115200);
  // Wait up to 8s for a serial host — this sketch is useless headless,
  // but don't hang forever in case someone flashes it by accident.
  unsigned long start = millis();
  while (!Serial && millis() - start < 8000)
    ;

  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("ERROR: RTC not found on I2C");
    while (true)
      delay(1000);
  }

  DateTime now = rtc.now();
  Serial.printf("rtc-set-clock ready. Current RTC time: %04u-%02u-%02u %02u:%02u:%02u\n",
                (unsigned)now.year(), (unsigned)now.month(),
                (unsigned)now.day(), (unsigned)now.hour(),
                (unsigned)now.minute(), (unsigned)now.second());
  Serial.println("Send T<epoch> (UTC unix seconds) to set.");
}

static void handleSetClock() {
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
  uint32_t epoch = (uint32_t)num.toInt();
  rtc.adjust(DateTime(epoch));
  Serial.printf("ECHO: T%lu\n", (unsigned long)epoch);
}

void loop() {
  if (Serial.available() > 0) {
    char peeked = (char)Serial.peek();
    if (peeked == 'T' || peeked == 't') {
      Serial.read(); // consume the T
      handleSetClock();
    } else {
      Serial.read(); // drain stray chars
    }
  }

  static unsigned long lastTick = 0;
  if (millis() - lastTick >= 1000) {
    lastTick = millis();
    DateTime now = rtc.now();
    Serial.printf("RTC: %04u-%02u-%02u %02u:%02u:%02u\n",
                  (unsigned)now.year(), (unsigned)now.month(),
                  (unsigned)now.day(), (unsigned)now.hour(),
                  (unsigned)now.minute(), (unsigned)now.second());
  }
}

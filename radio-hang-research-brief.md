# WindRadio / RFM69 + RP2040 intermittent TX failure — research brief

## Ask

Help identify why `RFM69::send()` on an Adafruit Feather RP2040 RFM69
(earlephilhower arduino-pico core) intermittently fails to complete a
transmission — sometimes hanging forever, now (after patching) bailing via a
timeout — even though SPI communication and RX work perfectly. Interested in:
root cause, known issues with this library/core combination, and robust fixes.

## Hardware & software

| Item | Value |
|---|---|
| Board | Adafruit Feather RP2040 with RFM69HCW (`rp2040:rp2040:adafruit_feather_rfm`) |
| Core | earlephilhower arduino-pico 6.0.0 |
| Radio lib | LowPowerLab RFM69 (LocalPowerLab/RFM69_LowPowerLab), current release, locally patched (see below) |
| Wiring (variant pins_arduino.h) | RFM_CS=GPIO16, RFM_RST=GPIO17, DIO0=GPIO21; SPI0 on SCK=14/MOSI=15/MISO=8 |
| Frequency | 915 MHz, FSK, AES encryption enabled (16-byte key) |
| Power | `setHighPower()` (HCW), `setPowerLevel(23)` |
| Two identical boards on the same desk, recommended antennas |

## Application protocol context

Custom protocol v5 over the radio. No hardware ACKs used anywhere — only
`radio.send()`. Base polls nodes; nodes answer with status packets sent blind,
retried 3× with 20ms gaps. Packet struct is 15 bytes (packed). Both directions
confirmed working end-to-end when the link is up.

## Symptom timeline

### Phase 1 — original firmware
Central node polled every 30 s. After ~30–60 s the node froze permanently
inside `radio.send()`. Serial showed the last poll started but never finished.
Root suspicion: unbounded busy-waits in the library.

### Phase 2 — library patched with timeouts
Patched `RFM69.cpp::sendFrame()`:

```cpp
// was: while ((readReg(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY) == 0x00);
uint32_t mrStart = millis();
while ((readReg(REG_IRQFLAGS1) & RF_IRQFLAGS1_MODEREADY) == 0x00) {
  if (millis() - mrStart > 50) return;   // RF69_MODE_READY_TIMEOUT_MS
}
...
setMode(RF69_MODE_TX);
// was: while ((readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PACKETSENT) == 0x00);
uint32_t psStart = millis();
while ((readReg(REG_IRQFLAGS2) & RF_IRQFLAGS2_PACKETSENT) == 0x00) {
  if (millis() - psStart > 100) break;   // RF69_TX_TIMEOUT_MS
}
setMode(RF69_MODE_STANDBY);
```

Result: no more permanent hang, but sends now silently fail instead — see log.

### Phase 3 — minimal ping/pong test sketches
Two standalone test programs using the same shared radio init code:

* **ping** (node 90): every 1000 ms calls `sendPacket()` → `radio.send(peer,
  &pkt, 15)` then listens 100 ms for a reply.
* **pong** (node 91): `receiveDone()` loop; answers each poll with a 15-byte
  status packet sent 3× (blind, 20 ms apart).

Ping-side serial instrumentation reads registers directly:
opmode = reg 0x01, irq1 = reg 0x27, irq2 = reg 0x28, plus IRQ2 bit 3
(PacketSent) checked right after `send()` returns.

### Observed log (ping side)

```
Radio init good
sizeof(WindRadioPacket)=15
[3208] -> sendPacket(POLL_REQ) to 91
[3208] radio: sent 15 bytes to node 91        <- our own print AFTER radio.send returns
[3208] <- sendPacket returned 1               <- sendPacket() always returns true
[3208] TX-FAULT : opmode=0x0c irq1=0x10 irq2=0x00   <- PacketSent NOT set!
[3215] radio: receiveDone DATALEN=15 RSSI=-27       <- RX works great now
[3215] <- RX from 91 type=2 v5 rssi=-27             <- pong's reply arrived fine
[4215] pre-send : opmode=0x04 irq1=0x80 irq2=0x00   <- before 2nd send: RX mode, ModeReady set
[4215] -> sendPacket(POLL_REQ) to 91
< HANGS HERE — none of the timeout-bounded waits should allow this >
```

Key observations:

1. **First send fails**: radio ends up in opmode=0x0C (TX bits set) but
   PacketSent never asserts within 100 ms → bails. Yet the pong node *did*
   apparently hear something (its reply arrives immediately after).
2. **Second send hangs completely** — despite every wait in the call path now
   being timeout-bounded:
   - `send()` → CSMA loop `while (!canSend() && millis()-now < 1000)`
     (bounded)
   - `sendFrame()` → ModeReady wait (bounded, patched)
   - `sendFrame()` → PacketSent wait (bounded, patched)
   
   The hang occurs after our `-> sendPacket(...)` print and before our
   `radio: sent ...` print, i.e. somewhere inside `radio.send()` itself.
3. RSSI values are now sane (−27 dBm desk distance) after we fixed an unrelated
   constructor bug (see below), so signal quality is not the issue.
4. With the receiver board powered OFF entirely, the very first send already
   shows TX-FAULT — so this does NOT require any incoming traffic to trigger.

## Already found & fixed (unrelated but relevant history)

The radio object had been constructed with a bogus 4th argument:

```cpp
RFM69 radio(RFM69_CS, RFM69_INT, true, PIN_RFM_DIO0); // BUG: 4th arg is SPIClass*, not a pin
RFM69 radio(RFM69_CS, RFM69_INT, true);               // FIXED
```

This passed GPIO number 21 as an `SPIClass*` — a garbage pointer the library
called `_spi->beginTransaction()/transfer()/endTransaction()` on. It silently
corrupted memory on every SPI transaction while still mostly working. Fixing
this restored sane RSSI readings, but the TX fault/hang behavior remains.

## Questions

1. What can make `radio.send()` block indefinitely on RP2040 when every loop
   inside it is bounded by `millis()` differences? Candidates we're considering:
   - `Serial.printf` deadlock (we print from the same core around the send)?
   - Interrupt-storm on DIO0 (GPIO21) starving the main loop?
   - `millis()` frozen due to IRQ priority issues between the RFM69 ISR and
     the SysTick/alarm used for millis?
   - Deadlock inside `SPI.beginTransaction()` / pico-sdk spinlocks?
2. Is there a known interaction bug between RFM69_LowPowerLab and
   arduino-pico 6.0.0 (e.g. DIO0 interrupt attached with RISING triggering
   repeatedly, or the ISR calling `setMode()` reentrantly while `send()` runs)?
3. Why would opmode read back 0x0C (TX) with irq1=0x10 (ModeReady NOT set?)
   and irq2=0x00 (PacketSent NOT set)? Is the radio actually stuck switching
   into TX, e.g. because `setHighPowerRegs(true)` (TESTPA1/TESTPA2 writes)
   are needed for HCW and were skipped/corrupted?
4. Recommended robust pattern for this library + dual-core RP2040: should all
   radio access be wrapped in `noInterrupts()`-style protection around
   `send()`, or should the DIO0 attachInterrupt be detached during TX?
5. Any known issue where `canSend()`/CSMA (`readRSSI()` forced trigger) leaves
   the radio mid-state such that a following `setMode(TX)` never completes?

Any pointers to known GitHub issues, errata, or datasheet sections (RFM69HCW
RegOpMode/IRQF flags, high-power PA sequencing) would help.

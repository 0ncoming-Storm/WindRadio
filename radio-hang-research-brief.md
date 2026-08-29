# WindRadio / RFM69 + RP2040 TX failure — implementation record

> This file started as an open research brief (phases 1–3 below) asking for
> help with intermittent `RFM69::send()` failures. It is now a record of what
> was actually implemented in this tree, where it lives, and what remains
> open. Original symptom description is kept, condensed, for context.

## Original symptom (condensed)

| Item | Value |
|---|---|
| Board | Adafruit Feather RP2040 with RFM69HCW (`rp2040:rp2040:adafruit_feather_rfm`) |
| Core | earlephilhower arduino-pico 6.0.0 |
| Radio lib | LowPowerLab RFM69, **vendored + locally patched** in `lib/RFM69_LowPowerLab/` |
| Frequency / security | 915 MHz, FSK, AES-128 |
| Wiring | RFM_CS=GPIO16, RFM_RST=GPIO17, DIO0=GPIO21, SPI0 SCK=14/MOSI=15/MISO=8 |

Phase 1 (original firmware): the base polled every 30 s; after 30–60 s a node
froze permanently inside `radio.send()` (unbounded busy-waits in the library).

Phase 2 (library patched with `millis()` bounds): no more permanent hang, but
sends began to fail *silently* — the packet was accepted, `send()` returned,
PacketSent never asserted within the timeout.

Phase 3 (minimal ping/pong): register instrumentation on the failing send
showed `opmode=0x0C` (TX), `irq1=0x10`, `irq2=0x00`.

**Decode correction** (the original brief misread the log): in this library
`RF_IRQFLAGS1_MODEREADY` is bit 7 (`0x80`); `0x10` is `PLLLOCK`. So the failed
state was: the mode register says TX, **ModeReady genuinely never asserted**,
PLL was locked, and **all FIFO flags in IRQ2 are clear — the FIFO looks
empty**, even though 18 bytes had just been written to it. The radio was
stuck mid-transition to TX with (apparently) nothing in the FIFO to send.

Already found and fixed before this record: the radio object had been
constructed with the DIO0 *pin number* as the 4th constructor argument, which
is an `SPIClass*` — a garbage pointer the library dereferenced on every SPI
transaction (silent memory corruption). `common/WindRadioCommon.cpp` now
constructs `RFM69 radio(RFM69_CS, RFM69_INT, true)` and documents the trap.

## Root-cause status

The exact trigger of the wedged state was never pinned down. Remaining
candidates, all still plausible after the constructor fix:

- The FIFO write was lost (SPI glitch / CS timing) while the radio was
  completing the standby→TX transition, so TX started with an empty FIFO and
  ModeReady never comes up.
- PA high-power register (TESTPA1/2) corruption from a mode-change collision.
- Supply sag from the ~130 mA TX current spike when bench-powered from a
  laptop USB port (known to brown the PLL at high power levels).
- DIO0 ISR activity racing the RX→TX mode change (the deferred-ISR pattern
  makes this unlikely, not impossible).

The design goal became: make the wedge **bounded, detectable, recoverable,
and safe** so the system works regardless of which cause fires. That is what
the implementation below does.

## What was implemented (current tree)

### 1. Bounded waits everywhere in the active code paths
`lib/RFM69_LowPowerLab/RFM69.cpp` (vendored, compiled via the Makefile's
`--library` flag so Library Manager updates can't break the build):

- `sendFrame()` ModeReady wait: 50 ms (`RF69_MODE_READY_TIMEOUT_MS`)
- `sendFrame()` PacketSent wait: 100 ms (`RF69_TX_TIMEOUT_MS`)
- `readRSSI(true)` RSSI-done wait: 10 ms
- `canSend()` CSMA loop: 1000 ms (library default, `RF69_CSMA_LIMIT_MS`)
- `initialize()` register probe + ModeReady waits: 50 ms

The only remaining unbounded waits in the library are in listen-mode/OTA
code, which is disabled (`RF69_LISTENMODE_ENABLE` is commented out) and
unused.

### 2. `lastTxOk` TX-verification latch
`RFM69.h`/`RFM69.cpp`: `sendFrame()` records
`lastTxOk = (IRQ2 & PACKETSENT) != 0` at the end of the (bounded) wait.
The PacketSent flag clears when the radio leaves TX mode, so it is read
*inside* the loop and must NOT be re-polled afterwards (the pre-send register
dumps at debug level 3 read the registers, but never write them).

### 3. Hardened transmit path
`common/WindRadioCommon.cpp::sendPacket()` — every transmit on every node:

1. force `STANDBY` + 500 µs before TX (regulator settling before the PA
   spike; clean starting mode for CSMA),
2. `detachIsr()` around the entire blocking `send()` (the RX interrupt is
   not needed during a blocking TX, and this removes the ISR-vs-mode-change
   race entirely; packets arriving in the window are dropped by design and
   covered by the application-level retries in point 6),
3. on fault: log `irq1` + consecutive-fault count, resume RX.

### 4. Runtime radio reset on consecutive TX faults
`common/WindRadioCommon.cpp`: a consecutive-fault counter is cleared on any
successful send. Why it is needed at all: after a wedge, the library's
internal mode bookkeeping still says STANDBY while the chip is stuck, so
every later `setMode()` call **early-returns without touching the chip** —
no new mode transition is issued, ModeReady never re-asserts, and every
subsequent send times out forever. The radio cannot self-heal.
At `TX_FAULT_RESET_THRESHOLD` (5) consecutive faults, `radioHardReset()`
pulses RST and re-runs the full `doRadioSetup()` (reset, initialize,
high-power, power level, AES key). Boot and recovery share the same init
path.

### 5. Watchdogs on all nodes
`receiver-pond`, `receiver-latching`, `receiver-nonlatching`:
`rp2040.wdt_begin(8000)`. A hung node resets itself; non-latching relays
default OFF on reset (safe direction), the latching gate relay holds
position through reset by design and is resolved by the 6-minute fail-safe
below.

The base station gets one too — it was the only node without a WDT, so a
hung core there (UI core stuck in an I2C OLED transfer, radio core wedged
in a loop, either core in a hardfault) would leave the network unattended
forever. The RP2040 has a single chip watchdog, so both cores must share
it with a twist: an unconditional `wdt_reset()` from a healthy core would
always mask a hung one. Each core instead bumps a volatile beat counter
every loop and only kicks the WDT while the *other* core's counter has
changed within 10 s (`CoreStallWatcher` in `central_control.ino`). Core 1
also stamps beats from RadioManager progress points
(`radioCoreBeat()`: `loop()` top, each `transact()` attempt) because one
transact() can block ~1.5 s worst case (CSMA + send + listen slice) —
still ≪ the 10 s threshold. The 20 s WDT period then forces the reset:
a hung core is recovered within ~30 s worst case. The WDT is armed in
`setup1()` (after both cores are up), deliberately NOT in `setup()`, so
the bench `while (!Serial)` wait in `setup()` cannot reboot-loop a
headless board. Note for whoever bounds/removes that wait for field
deployment (see its comment): with the watchdog in place, a WDT-triggered
reboot lands back in a working system, which is exactly the property the
wait currently lacks. Known coverage gap: a hang *during* `setup()`
(e.g. OLED I2C never answers) happens before the WDT is armed, so it is
not auto-recovered; the 6-minute node fail-safe still takes the relays
OFF in that case — safety holds, recovery is just slower.

### 6. Application protocol v6 (no radio-level ACKs, ever)
`common/WindRadioCommon.h` + `RadioManager::transact()`:

- Every request (poll *or* command) must be answered with a **fresh status
  snapshot**; there is no separate ACK type.
- Status replies are blind-retried 3×, 20 ms apart.
- Packets are absolute snapshots, so duplicates — including a stale copy
  left in the radio FIFO after a wedged TX — are harmless; the first valid
  reply wins.
- The version byte rejects cross-firmware traffic (v6 added the RTC weekday
  to `PKT_POND_STATUS`; mixed v5/v6 nodes must not share a network).
- Liveness = polls answered; the base flags a node offline after 3 missed
  polls and treats stale wind/time as "everything OFF".

### 7. Node-side base-liveness fail-safe (all receivers)
`common/WindRadioCommon.{h,cpp}` + each receiver's loop: each node tracks
the last `PKT_POLL_REQUEST` from the base (`markBasePollSeen()`) and shuts
its relay off after `BASE_POLL_TIMEOUT_MS` (6 minutes = 12 missed polls) of
silence. The base's wind-based shutoffs only run while the base is alive;
this covers base death — e.g. the gate node disconnects the car-sensor wire
so the gate cannot auto-open unattended. The state check (`state != OFF`)
makes it act once per silence period, so the latching coil is pulsed once,
not every loop.

### 8. Instrumentation
- `RadioManager.cpp` `RLOG_LEVEL` 1–3: level 3 adds pre-send register dumps
  (mode/IRQ1/IRQ2/RSSI) before every transaction.
- TX-fault log lines carry `irq1` and a running fault count so a wedge
  event is visible without a logic analyzer.
- Ping/pong test nodes (`radio-test-*`) exercise the identical
  `WindRadioCommon` transmit/receive path, so library regressions show up
  in 10 lines of output.

## Verification & status

- Base station: stable through long continuous cycling runs (22 h+).
- Ping/pong: repeated TX/RX with per-exchange register dumps; TX faults are
  now logged and recovered instead of hanging.
- Link quality is not the issue (RSSI ≈ −27 dBm at desk distance after the
  constructor fix).
- Field test with sensors wired is still outstanding, as is anemometer
  calibration (`mapWindSpeed` is a placeholder).

## Deployment notes (checked before flashing production units)

- `central_control/central_control.ino` still contains an unbounded
  `while (!Serial)` — intentional for bench work, **blocks boot on a
  headless unit** (on this core `!Serial` is true until a USB host
  connects). In-code comment marks it; bound or remove before deployment.
- `RadioManager.cpp` still ships `RLOG_LEVEL 3` — chatty, and it forces an
  RSSI measurement + register reads before every send. Set to 1 before
  deployment (in-code comment marks it).
- `common/WindRadioCommon.cpp` runs `setPowerLevel(16)` (≈12–15 dBm) while
  the README recommends 23 (≈17–20 dBm) for the 100–140 m no-LOS links —
  pick one deliberately.
- Bench-powering the HCW from a laptop USB port can brown out the PLL
  during the 130 mA TX spike; use dedicated 5 V supplies in the field.

## If the wedge recurs: what to capture

Pre-send dump is already logged at RLOG_LEVEL 3. Add/keep: post-fault
readback of `REG_OPMODE` (0x01), `REG_IRQFLAGS1` (0x27), `REG_IRQFLAGS2`
(0x28), FIFO level `REG_FIFOLEVEL` (0x35), and the supply voltage across the
TX spike (scope on 5 V / 3.3 V at the radio). A stuck TX with `FIFOLEVEL ==
0` confirms the "FIFO write lost" candidate; a non-empty FIFO with no
PacketSent points at the PA/high-power path or the supply.

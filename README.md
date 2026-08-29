# WindRadio

RFM69-based estate automation on Adafruit Feather RP2040 boards (915 MHz).
A central control node monitors wind conditions and schedules, then commands
remote relay nodes: an automatic gate, a pond pump, and two decorative
fountains. Anything that moves shuts off automatically in high wind.

## Hardware

| Node | Board | Role |
|---|---|---|
| `central_control` | Feather RP2040 RFM69 + SH1107 OLED + 3 buttons | Base station: polling, decisions, UI |
| `receiver-pond` | Feather RP2040 RFM69 | Anemometer (ADC), DS3231 RTC, non-latching pump relay |
| `receiver-latching` | Feather RP2040 RFM69 | Latching relay interrupting the gate's car-sensor wire |
| `receiver-nonlatching` | Feather RP2040 RFM69 | Non-latching mains relays for the fountains |

Fountain node identity is chosen at build time (`FOUNTAIN=1` or `FOUNTAIN=2`).

## Protocol (v6)

Single fixed-size packed struct (`WindRadioPacket`, 18 bytes) with a version
byte. The RFM69 hardware ACK stack is **never used** — reliability comes from
application-level rules:

- **Every request is answered with a status data packet.** A poll elicits
  fresh data; a command elicits fresh data too (the applied state *is* the
  confirmation). No separate ACK packet exists.
- **Status replies are blind-retried** (3×, 20 ms apart). Duplicates are
  harmless because packets are absolute snapshots.
- **Liveness = polls answered.** The base tracks consecutive missed polls per
  node; after 3 misses (~90 s) a node is flagged offline and treated as stale.
- **Base liveness at the nodes:** if a node has not seen a base poll for
  6 minutes (12 missed polls) it shuts its own relay off — this covers base
  station death (the base's wind shutoffs only run while the base is alive,
  e.g. the gate node disconnects the car-sensor wire so the gate cannot
  auto-open unattended).
- **Fail-safe:** stale wind data, dead pond RTC, or unreachable nodes all mean
  everything stays OFF.
- Commands are idempotent absolute-state writes (`relayOn` true/false), so
  retransmission after a lost reply is safe.
- v6 adds the RTC weekday to the pond status packet so the base's DST test is
  exact (wire format change — flash all nodes from the same tree).

The DIO0 interrupt is detached around every transmit (RP2040 ISR/mode-change
collision workaround), and sends are verified via the library's latched
`lastTxOk` flag.

## Build & flash

Requires `arduino-cli` with the `rp2040` core (earlephilhower) plus libraries:
`_RFM69` (LowPowerLab), `RTClib`, Adafruit GFX/SH110X/BusIO. See
`make setup-deps`.

```sh
make node=<dir> compile        # build one node
make node=<dir> flash          # build + upload over USB
make monitor                   # serial console at 115200

# Fountains share one sketch; pick the node identity at build time:
make node=receiver-nonlatching flash            # fountain 1
make node=receiver-nonlatching FOUNTAIN=2 flash # fountain 2
```

Compiles are cached — only changed sources trigger rebuilds. `make clean`
wipes a node's build dir.

## Repository layout

```
common/                 WindRadioCommon.{h,cpp} — shared radio setup, packet
                        I/O, protocol definitions. Used by every node.
central_control/        Base station firmware (Core 0: UI/display,
                        Core 1: radio). RadioManager polls nodes and decides
                        relay states; screen.cpp implements the menus.
receiver-pond/          Pond sensor + pump node.
receiver-latching/      Gate node.
receiver-nonlatching/   Fountain node (FOUNTAIN=1/2).
relay-test-central/     Standalone serial console for manually toggling any
                        relay — no control logic. Bench-testing tool.
radio-test-ping/        Minimal raw-radio TX test (protocol structs).
radio-test-pong/        Minimal raw-radio RX test; answers pings.
```

## Central control logic

Every 30 s Core 1 runs a poll cycle: query pond → gate → fountains → decide
commands. Desired state per device:

- `MODE_OFF` — always off
- `MODE_MANUAL_ON` — always on
- `MODE_AUTO` — inside schedule window AND wind ≤ limit

Wind data and time come from the pond node. Wind is sampled on the pond node
as median-of-3 ADC reads every 100 ms, and the reported value is the max over
a 30 s rolling window (gust capture for safety decisions, no per-sample
jitter). Time is UTC on the RTC; the base converts to Alberta local time with
an exact DST test (RTC weekday + date + UTC hour). If the wind/time data is
stale (node down or its RTC dead) desired state is OFF everywhere —
fail-safe. Commands fire only when desired differs from the node's last
reported state; unconfirmed commands retry next cycle.

Settings (schedule, wind limit, mode) live in RAM and are edited through the
on-device menu (buttons A/B/C; hold A+B to enter menu).

## Notes / gotchas

- **The RFM69 library is vendored in this repo** at `lib/RFM69_LowPowerLab/`,
  locally patched: bounded ModeReady/PacketSent/RSSI waits, `lastTxOk` TX
  verification latch, and IRQ attach/detach helpers. The Makefile compiles
  against the in-repo copy, so Library Manager updates can't break the build
  (the library sources are part of the build stamp — patching the library
  triggers a recompile). If you reinstall the library system-wide
  (`~/Arduino/libraries/`), either re-copy from `lib/` or delete the
  installed copy — an unpatched version WILL hang the firmware.
- **Wedged-radio recovery:** a stuck RFM69 (mode transition that never
  completes) cannot self-heal — the library's mode bookkeeping no-ops every
  later `setMode()`. The shared radio layer therefore counts consecutive TX
  faults and, after 5 in a row, pulses RST and re-initializes the radio
  (`common/WindRadioCommon.cpp`, see `radio-hang-research-brief.md`).
- The RFM69 constructor must NOT be given a pin number as 4th argument — it's
  an `SPIClass*`. Passing GPIO 21 there silently corrupted memory (past bug).
- HCW modules draw ~130 mA TX spikes; bench powering from a laptop USB port
  can brown out the PLL at high power levels. Use dedicated supplies for
  deployment, keep `setPowerLevel(23)`.
- Encryption key exists because the radio requires one; this system does not
  aim for RF security/replay resistance.
- Serial debug levels: `RLOG_LEVEL` 1–3 in `central_control/RadioManager.cpp`.
  Level 3 is very chatty; use 1 for deployed units. A 5 s heartbeat line with
  per-node health prints when debug is enabled.

## Status

Working: radio link with retries/fault recovery, gate relay toggling end to
end, base station stable >22 h continuous cycling, error screen for offline
nodes / dead RTC, manual relay test console.

TODO: full-system field test with sensors wired, anemometer calibration
(`mapWindSpeed` in receiver-pond is a placeholder transfer function).

#include "WindRadioCommon.h"

// WindRadioCommon.cpp — shared implementation for radio setup and packet I/O.
// Included by every node (base station + remote sensors) to avoid duplication.

// RFM69 radio instance. Pin layout matches the shared WindRadioCommon.h
// aliases (CS, DIO0/interrupt); reset is pulsed via RFM69_RST in
// radioSetup() below.
// NOTE: the 4th ctor arg is an SPIClass*, NOT a pin number — passing
// PIN_RFM_DIO0 here was silently corrupting memory on every SPI transaction
// (garbage pointer). Leave it unset so the library uses the default SPI bus.
RFM69 radio(RFM69_CS, RFM69_INT, true);

// --- Radio Initialization ---
// Resets the RFM69 chip, initializes it on the configured frequency/network,
// enables high-power mode, and sets the encryption key. Shared by boot and
// the runtime recovery path (radioHardReset below).
static uint8_t myNodeIdG = 0; // node ID passed to radioSetup(); recovery re-inits with it

static void doRadioSetup() {
  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, HIGH); // hold reset (board polarity: HIGH = reset)
  delay(10);
  digitalWrite(RFM69_RST, LOW); // release reset
  delay(10);

  if (!radio.initialize(FREQUENCY, myNodeIdG, NETWORKID)) {
    Serial.println("Radio init failed!");
  } else {
    Serial.println("Radio init good");
  }
  radio.setHighPower(); // enable PA boost for RFM69HW
  // HCW level 16 = PA1+PA2, roughly 12-15 dBm. NOTE: README recommends
  // setPowerLevel(23) (~17-20 dBm, +HiPower) for the 100-140 m field links;
  // this lower level is the bench setting. Decide one and keep them in sync.
  radio.setPowerLevel(16);
  radio.encrypt(ENCRYPTKEY); // 16-byte AES key (password must match all nodes)
}

void radioSetup(uint8_t myNodeId) {
  myNodeIdG = myNodeId;
  doRadioSetup();
}

// --- TX Fault Tracking & Recovery ---
// A wedged radio (stuck mode transition: opmode reads TX while ModeReady
// never asserts) does NOT recover by itself. After the wedge the library's
// internal mode bookkeeping still says STANDBY, so every later setMode()
// call is a no-op — no new mode transition is ever issued, ModeReady never
// re-asserts, and every send bails out of the bounded waits with lastTxOk
// false, forever. The only reliable reset is the RST pin, so once the
// consecutive fault count hits the threshold we pulse RST and re-run the
// full initialization.
#define TX_FAULT_RESET_THRESHOLD 5
static uint8_t consecutiveTxFaults = 0;

static void radioHardReset() {
  Serial.printf(
      "[%lu] radio: %u consecutive TX faults - hard reset (RST pulse + "
      "re-init)\n",
      millis(), (unsigned)TX_FAULT_RESET_THRESHOLD);
  doRadioSetup();
  consecutiveTxFaults = 0;
}

// --- Transmit (no hardware ACK) ---
// Fire-and-forget: the RFM69's built-in ACK/retry stack is deliberately
// unused. Delivery is proven at the application layer instead — every
// request (poll or command) must be answered with a status data packet.
//
// The DIO0 interrupt is DETACHED for the duration of the send. The library's
// ISR path (entered via receiveDone() inside send()'s CSMA loop) can collide
// with the RX->TX mode transition and corrupt the radio's FIFO/mode state,
// leaving it stuck with opmode=TX, ModeReady=0, PacketSent never firing.
// send() blocks until the packet is on air anyway, so we don't need RX
// interrupts during that window.
bool sendPacket(uint8_t toNodeId, const WindRadioPacket &pkt) {
  // Force standby first: stabilizes the regulator before the ~130mA PA spike,
  // and guarantees canSend()/CSMA starts from a clean mode instead of mid-RX.
  radio.setMode(RF69_MODE_STANDBY);
  delayMicroseconds(500);

  radio.detachIsr();
  radio.send(toNodeId, (const void *)&pkt, sizeof(pkt));
  radio.attachIsr();

  // radio.lastTxOk was latched inside the PacketSent wait loop — the IRQ2
  // flag itself auto-clears on leaving TX mode, so it must not be polled
  // here. False means the send timed out; recover to RX and report failure.
  if (radio.lastTxOk) {
    consecutiveTxFaults = 0;
    return true;
  }

  consecutiveTxFaults++;
  Serial.printf(
      "[%lu] radio: TX fault sending to node %u (irq1=0x%02x, faults=%u)\n",
      millis(), toNodeId, (unsigned)radio.readReg(0x27),
      (unsigned)consecutiveTxFaults);
  radio.setMode(RF69_MODE_RX); // resume listening despite the failed send

  // A wedge does not self-heal (see radioHardReset docs): escalate to an RST
  // reset + re-init once faults run consecutively.
  if (consecutiveTxFaults >= TX_FAULT_RESET_THRESHOLD)
    radioHardReset();
  return false;
}

// --- Transmit with application-level retransmits ---
// Blind retransmission: no hardware ACK involved. The caller is responsible
// for listening for the expected reply; this just improves the odds that a
// single transmission getting lost in noise doesn't end the exchange.
void sendPacketRetried(uint8_t toNodeId, const WindRadioPacket &pkt,
                       uint8_t attempts, unsigned long retryDelayMs) {
  if (attempts < 1)
    attempts = 1;
  for (uint8_t i = 0; i < attempts; i++) {
    if (sendPacket(toNodeId, pkt))
      return; // one confirmed-good send is enough
    // Only retry when the previous attempt actually faulted at the radio
    // level; never delay after the final attempt either way.
    if (i == attempts - 1)
      break;
    delay(retryDelayMs);
  }
}

// --- Poll for an incoming packet ---
// Returns true and populates outPkt if a valid WindRadioPacket is received.
bool receivePacket(WindRadioPacket &outPkt) {
  if (!radio.receiveDone())
    return false;

  Serial.printf("[%lu] radio: receiveDone, DATALEN=%u (want %u) RSSI=%d\n",
                millis(), (unsigned)radio.DATALEN,
                (unsigned)sizeof(WindRadioPacket), (int)radio.RSSI);

  if (radio.DATALEN != sizeof(WindRadioPacket))
    return false; // wrong size — not a packet we understand

  memcpy(&outPkt, (const void *)radio.DATA, sizeof(outPkt));
  outPkt.fromNode = radio.SENDERID; // stamp who actually sent it

  return true;
}

// --- Base-Liveness Fail-Safe (used by all receiver nodes) ---
// 0 = "never seen a poll"; millis() subtraction wraps correctly.
static unsigned long lastBasePollMs = 0;

void markBasePollSeen() { lastBasePollMs = millis(); }

bool basePollExpired() {
  return (millis() - lastBasePollMs) >= BASE_POLL_TIMEOUT_MS;
}

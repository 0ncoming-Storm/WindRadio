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
// enables high-power mode, and sets the encryption key.
void radioSetup(uint8_t myNodeId) {
  pinMode(RFM69_RST, OUTPUT);
  digitalWrite(RFM69_RST, HIGH); // hold reset
  delay(10);
  digitalWrite(RFM69_RST, LOW); // release reset
  delay(10);

  if (!radio.initialize(FREQUENCY, myNodeId, NETWORKID)) {
    Serial.println("Radio init failed!");
  } else {
    Serial.println("Radio init good");
  }
  radio.setHighPower();      // enable PA boost for RFM69HW
  radio.setPowerLevel(16);   // ~23/31 max power
  radio.encrypt(ENCRYPTKEY); // 16-byte AES key (password must match all nodes)
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
  if (radio.lastTxOk)
    return true;
  Serial.printf("[%lu] radio: TX fault sending to node %u (irq1=0x%02x)\n",
                millis(), toNodeId, (unsigned)radio.readReg(0x27));
  radio.setMode(RF69_MODE_RX); // resume listening despite the failed send
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

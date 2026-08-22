#include "RadioManager.h"
#include "SystemData.h"

// RadioManager.cpp — Core 1 thread for polling remote nodes and controlling
// relays.

// Set to 0 to silence all radio debug output on USB serial.
// Levels (RLOG_LEVEL):
//   1 = errors and cycle summaries only
//   2 = level 1 + per-attempt send/reply/result lines
//   3 = level 2 + every listen-slice tick and radio state dumps
#define RADIO_DEBUG 1
#define RLOG_LEVEL 3

#if RADIO_DEBUG
#define RLOG(...)                          \
  do {                                     \
    if (RLOG_LEVEL >= 1)                   \
      Serial.printf(__VA_ARGS__);          \
  } while (0)
#define RLOG2(...)                         \
  do {                                     \
    if (RLOG_LEVEL >= 2)                   \
      Serial.printf(__VA_ARGS__);          \
  } while (0)
#define RLOG3(...)                         \
  do {                                     \
    if (RLOG_LEVEL >= 3)                   \
      Serial.printf(__VA_ARGS__);          \
  } while (0)
static const char *relayStateName(RelayState s) {
  switch (s) {
  case RELAY_OFF:
    return "OFF";
  case RELAY_ON:
    return "ON";
  default:
    return "UNKNOWN";
  }
}
static const char *packetTypeName(PacketType t) {
  switch (t) {
  case PKT_POLL_REQUEST:
    return "POLL_REQ";
  case PKT_POND_STATUS:
    return "POND_STATUS";
  case PKT_RELAY_STATUS:
    return "RELAY_STATUS";
  case PKT_SET_RELAY:
    return "SET_RELAY";
  case PKT_SET_POND:
    return "SET_POND";
  default:
    return "?";
  }
}
#else
#define RLOG(...)
#endif

void RadioManager::init() {
  mutex_init(&nodeStatusMutex);

  // Initialize cached status structs in a fail-safe state: every node is
  // assumed unreachable and its relay state unknown until the first
  // successful poll, so no relay commands are ever derived from the dummy
  // boot-time conditions.
  pondStatus = {0, 0, 0, 0.0f, RELAY_UNKNOWN, false, 255, NODE_ERR_TIMEOUT, 0};
  gateStatus = {RELAY_UNKNOWN, 255, NODE_ERR_TIMEOUT, 0};
  fountain1Status = {RELAY_UNKNOWN, 255, NODE_ERR_TIMEOUT, 0};
  fountain2Status = {RELAY_UNKNOWN, 255, NODE_ERR_TIMEOUT, 0};
}

void RadioManager::loop() {
  // Run a full poll cycle every POLL_CYCLE_MS
  unsigned long now = millis();
  if (now - lastPollCycle >= POLL_CYCLE_MS) {
    lastPollCycle = now;
    runPollCycle();
    return;
  }
  // Nothing to do yet: sleep until the next cycle instead of spinning the
  // core flat-out.
  unsigned long remaining = POLL_CYCLE_MS - (now - lastPollCycle);
  delay(min(remaining, 250UL));

#if RADIO_DEBUG
  // Heartbeat: proves Core 1's radio loop is alive between cycles and dumps
  // per-node health so a hang vs. an offline-node is obvious on serial.
  static unsigned long lastHeartbeat = 0;
  if (now - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = now;
    mutex_enter_blocking(&nodeStatusMutex);
    RLOG("[%lu] alive: pond(err=%u miss=%u rtc=%d wind=%d) "
         "gate(err=%u miss=%u) f1(err=%u miss=%u) f2(err=%u miss=%u)\n",
         now, pondStatus.error, pondStatus.missedPolls,
         (int)pondStatus.rtcOk, pondStatus.windSpeed, gateStatus.error,
         gateStatus.missedPolls, fountain1Status.error,
         fountain1Status.missedPolls, fountain2Status.error,
         fountain2Status.missedPolls);
    mutex_exit(&nodeStatusMutex);
  }
#endif
}

void RadioManager::getPondNodeStatus(PondNodeStatus &out) {
  mutex_enter_blocking(&nodeStatusMutex);
  out = pondStatus;
  mutex_exit(&nodeStatusMutex);
}

void RadioManager::getGateStatus(NodeStatus &out) {
  mutex_enter_blocking(&nodeStatusMutex);
  out = gateStatus;
  mutex_exit(&nodeStatusMutex);
}

void RadioManager::getFountainStatus(uint8_t index, NodeStatus &out) {
  if (index > 1)
    index = 1; // clamp: only fountain 1 and 2 exist
  mutex_enter_blocking(&nodeStatusMutex);
  out = (index == 0) ? fountain1Status : fountain2Status;
  mutex_exit(&nodeStatusMutex);
}

// Send a request and wait for the expected reply, retransmitting at the
// application level while it doesn't arrive. The RFM69 hardware ACK stack
// is unused: reliability comes from the protocol itself — every request is
// supposed to elicit exactly one reply packet — status data for polls and
// for commands alike (a node answers a command with its fresh status) — so a
// received reply proves delivery of both directions.
//
// The timeout budget is split across POLL_ATTEMPTS send/listen rounds. Only
// version-matching packets of `expectType` from `nodeId` count as a reply;
// anything else is logged and ignored. Returns false on timeout.
bool RadioManager::transact(uint8_t nodeId, const WindRadioPacket &request,
                            PacketType expectType, WindRadioPacket &outResponse,
                            unsigned long timeoutMs) {
  const unsigned long listenSlice = timeoutMs / POLL_ATTEMPTS;

  // --- Radio state dump before the exchange (level 3) ---
#if RADIO_DEBUG
  if (RLOG_LEVEL >= 3) {
    Serial.printf("[%lu] radio: pre-send mode=0x%02x irq1=0x%02x irq2=0x%02x "
                  "rssi=%d\n",
                  millis(), (unsigned)radio.readReg(0x01),
                  (unsigned)radio.readReg(0x27), (unsigned)radio.readReg(0x28),
                  (int)radio.readRSSI(true));
  }
#endif

  for (uint8_t attempt = 0; attempt < POLL_ATTEMPTS; attempt++) {
    RLOG("[%lu] -> node %u %s (attempt %u/%u, listen %lums)\n", millis(),
         nodeId, packetTypeName(request.type), attempt + 1,
         (unsigned)POLL_ATTEMPTS, listenSlice);
    if (!sendPacket(nodeId, request)) {
      // Radio-level TX fault: no packet went out, so waiting for a reply is
      // pointless — skip straight to the next attempt.
      RLOG2("[%lu] .. node %u: attempt %u TX fault, skipping listen\n",
            millis(), nodeId, attempt + 1);
      continue;
    }

    unsigned long deadline = millis() + listenSlice;
    while ((long)(millis() - deadline) < 0) {
      WindRadioPacket incomingPacket;
      if (receivePacket(incomingPacket)) {
        RLOG3("[%lu] radio: got %u-byte pkt from node %u type=%s v%u\n",
              millis(), (unsigned)sizeof(incomingPacket), incomingPacket.fromNode,
              packetTypeName(incomingPacket.type), incomingPacket.version);
        if (incomingPacket.version != PROTOCOL_VERSION) {
          RLOG("[%lu] <- node %u: dropped v%u packet\n", millis(),
               incomingPacket.fromNode, incomingPacket.version);
          continue; // ignore outdated protocol versions
        }
        if (incomingPacket.fromNode != nodeId) {
          RLOG("[%lu] <- stray %s packet from node %u\n", millis(),
               packetTypeName(incomingPacket.type), incomingPacket.fromNode);
          continue; // ignore packets from a different node
        }
        if (incomingPacket.type != expectType) {
          RLOG("[%lu] <- node %u: unexpected %s (wanted %s)\n", millis(),
               nodeId, packetTypeName(incomingPacket.type),
               packetTypeName(expectType));
          continue;
        }
        RLOG("[%lu] <- node %u replied %s\n", millis(), nodeId,
             packetTypeName(incomingPacket.type));
        outResponse = incomingPacket;
        return true;
      }
      // Incoming packets are captured by the radio's interrupt handler, so a
      // 1ms harvest interval cannot miss anything; without this the loop
      // would hammer the SPI bus at full CPU speed for the whole timeout.
      delay(1);
    }
    RLOG2("[%lu] .. node %u: attempt %u timed out after %lums\n", millis(),
          nodeId, attempt + 1, listenSlice);
  }
  RLOG("[%lu] <- node %u: no reply within %lums\n", millis(), nodeId,
       timeoutMs);
  return false;
}

// Poll the pond node for wind data + pump relay status, then feed the
// wind conditions into the shared CurrentConditions struct.
void RadioManager::pollPondNode() {
  WindRadioPacket requestPacket;
  requestPacket.type = PKT_POLL_REQUEST;
  WindRadioPacket responsePacket;
  // The status reply doubles as the poll acknowledgement.
  bool ok = transact(NODE_POND, requestPacket, PKT_POND_STATUS, responsePacket,
                     POLL_TIMEOUT_MS);

  uint8_t missed = 0;
  mutex_enter_blocking(&nodeStatusMutex);
  if (ok) {
    pondStatus.hours = responsePacket.pondStatus.hours;
    pondStatus.minutes = responsePacket.pondStatus.minutes;
    pondStatus.windSpeed = responsePacket.pondStatus.windSpeed;
    pondStatus.temperature = responsePacket.pondStatus.temperature;
    pondStatus.pumpState = responsePacket.pondStatus.pumpState;
    pondStatus.rtcOk = responsePacket.pondStatus.rtcOk;
    pondStatus.missedPolls = 0;
    pondStatus.error = NODE_OK;
    pondStatus.lastSuccessMs = millis();
  } else {
    if (pondStatus.missedPolls < 255)
      pondStatus.missedPolls++;
    missed = pondStatus.missedPolls;
    if (pondStatus.missedPolls >= 3)
      pondStatus.error = NODE_ERR_TIMEOUT;
  }
  mutex_exit(&nodeStatusMutex);

#if RADIO_DEBUG
  if (ok) {
    RLOG("[%lu] pond: time=%02u:%02u (%s) wind=%d km/h temp=%.1fC pump=%s\n",
         millis(), responsePacket.pondStatus.hours,
         responsePacket.pondStatus.minutes,
         responsePacket.pondStatus.rtcOk ? "rtc ok" : "RTC DEAD",
         responsePacket.pondStatus.windSpeed,
         (double)responsePacket.pondStatus.temperature,
         relayStateName(responsePacket.pondStatus.pumpState));
  } else {
    RLOG("[%lu] pond: no data (missed %u in a row)\n", millis(), missed);
  }
#endif

  // Only feed the shared conditions when the node's clock is actually alive.
  // With a dead RTC its time (and DS3231-derived temperature) are fake
  // values, and schedules must not be evaluated against them. Wind speed
  // comes from the analog anemometer, not the RTC, so it is still valid —
  // but decideAndSendCommands() treats any rtcOk==false as fully stale to be
  // safe (fail-safe = everything OFF, same as a missed pond).
  if (ok && responsePacket.pondStatus.rtcOk) {
    updateConditionsFromPond(responsePacket.pondStatus.windSpeed,
                             responsePacket.pondStatus.temperature,
                             responsePacket.pondStatus.hours,
                             responsePacket.pondStatus.minutes);
  }
}

// Poll a relay (gate or fountain) node for on/off status + error tracking.
void RadioManager::pollRelayNode(uint8_t nodeId, NodeStatus &status) {
  WindRadioPacket requestPacket;
  requestPacket.type = PKT_POLL_REQUEST;
  WindRadioPacket response;
  // The status reply doubles as the poll acknowledgement.
  bool ok = transact(nodeId, requestPacket, PKT_RELAY_STATUS, response,
                     POLL_TIMEOUT_MS);

  uint8_t missed = 0;
  mutex_enter_blocking(&nodeStatusMutex);
  if (ok) {
    status.relayState = response.relayStatus.relayState;
    status.missedPolls = 0;
    status.error = NODE_OK;
    status.lastSuccessMs = millis();
  } else {
    if (status.missedPolls < 255)
      status.missedPolls++;
    missed = status.missedPolls;
    if (status.missedPolls >= 3)
      status.error = NODE_ERR_TIMEOUT;
  }
  mutex_exit(&nodeStatusMutex);

#if RADIO_DEBUG
  if (ok) {
    RLOG("[%lu] node %u: relay=%s\n", millis(), nodeId,
         relayStateName(response.relayStatus.relayState));
  } else {
    RLOG("[%lu] node %u: no data (missed %u in a row)\n", millis(), nodeId,
         missed);
  }
#endif
}

// Check whether the given hour/minute falls within the device's schedule.
// Handles overnight windows (e.g. 22:00 → 06:00).
bool RadioManager::isWithinSchedule(int hours, int minutes,
                                    const DeviceSettings &s) {
  int nowMin = hours * 60 + minutes;
  int startMin = s.startHour * 60 + s.startMin;
  int endMin = s.endHour * 60 + s.endMin;

  if (startMin <= endMin) {
    return nowMin >= startMin && nowMin < endMin;
  } else {
    // Window wraps past midnight
    return nowMin >= startMin || nowMin < endMin;
  }
}

// Decide whether a device's relay should be on, given mode + conditions.
// Returns false if wind data is stale (no recent pond reading).
bool RadioManager::computeDesiredState(const DeviceSettings &settingsStruct,
                                       bool windStale, int windSpeed, int hours,
                                       int minutes) {
  if (windStale)
    return false;

  switch (settingsStruct.mode) {
  case MODE_OFF:
    return false;
  case MODE_MANUAL_ON:
    return true;
  case MODE_AUTO:
  default:
    return isWithinSchedule(hours, minutes, settingsStruct) &&
           windSpeed <= settingsStruct.windLimit;
  }
}

// Send a relay on/off command to a remote node.
// Protocol v5: there is no ACK packet. The node applies the command and then
// immediately answers with its status data, so `expectType` is the status
// packet kind for that node. The reply's payload is deliberately ignored —
// the authoritative state comes from the next poll cycle; this reply only
// proves delivery. Commands are idempotent absolute-state writes, so a lost
// reply simply means decideAndSendCommands() retries next cycle.
bool RadioManager::sendRelayCommand(uint8_t nodeId, PacketType type,
                                    bool relayOn) {
  WindRadioPacket cmd;
  cmd.type = type;
  cmd.setRelay.relayOn = relayOn;

  PacketType expectType =
      (type == PKT_SET_POND) ? PKT_POND_STATUS : PKT_RELAY_STATUS;

  WindRadioPacket reply;
  bool ok = transact(nodeId, cmd, expectType, reply, POLL_TIMEOUT_MS);
  RLOG("[%lu] cmd -> node %u: %s\n", millis(), nodeId,
       ok ? "CONFIRMED (status reply)" : "NO REPLY");
  return ok;
}

// Core decision loop: compare desired vs. actual relay states and send
// commands only when a change is needed. The cached state is updated only
// when the node acknowledges, so unacknowledged commands are retried on
// the next poll cycle.
void RadioManager::decideAndSendCommands() {
  DeviceSettings gateSettings, pondSettings, fountainSettings;
  getDeviceSettings(DEVICE_GATE, gateSettings);
  getDeviceSettings(DEVICE_POND, pondSettings);
  getDeviceSettings(DEVICE_FOUNTAINS, fountainSettings);

  mutex_enter_blocking(&nodeStatusMutex);
  // Fail-safe conditions: no fresh pond data, OR the pond node's RTC is
  // dead (its reported time can't be trusted, so schedules are meaningless).
  // Either way everything stays OFF until good data returns.
  bool windStale =
      (pondStatus.error != NODE_OK) || !pondStatus.rtcOk;
  int windSpeed = pondStatus.windSpeed;
  int hours = pondStatus.hours;
  int minutes = pondStatus.minutes;
  RelayState gateLastKnown = gateStatus.relayState;
  RelayState pondRelayLastKnown = pondStatus.pumpState;
  RelayState fountain1LastKnown = fountain1Status.relayState;
  RelayState fountain2LastKnown = fountain2Status.relayState;
  mutex_exit(&nodeStatusMutex);

  // Desired states are plain bools; comparing against a RelayState works
  // because RELAY_OFF/RELAY_ON map to 0/1 and RELAY_UNKNOWN (2) differs
  // from both — so an unknown state always triggers a resolving command.
  bool gateDesired =
      computeDesiredState(gateSettings, windStale, windSpeed, hours, minutes);
  bool pondDesired =
      computeDesiredState(pondSettings, windStale, windSpeed, hours, minutes);
  bool fountainDesired = computeDesiredState(fountainSettings, windStale,
                                             windSpeed, hours, minutes);

#if RADIO_DEBUG
  RLOG("[%lu] desired: gate=%s pond=%s fountains=%s | wind=%d km/h (%s)\n",
       millis(), gateDesired ? "ON" : "OFF", pondDesired ? "ON" : "OFF",
       fountainDesired ? "ON" : "OFF", windSpeed,
       windStale ? "STALE" : "fresh");
#endif

  // Only send commands when the desired state differs from reality
  if (gateDesired != gateLastKnown) {
    RLOG("[%lu] gate needs change: %s -> %s\n", millis(),
         relayStateName(gateLastKnown), gateDesired ? "ON" : "OFF");
    if (sendRelayCommand(NODE_GATE, PKT_SET_RELAY, gateDesired)) {
      mutex_enter_blocking(&nodeStatusMutex);
      gateStatus.relayState = gateDesired ? RELAY_ON : RELAY_OFF;
      mutex_exit(&nodeStatusMutex);
    }
  }
  if (pondDesired != pondRelayLastKnown) {
    RLOG("[%lu] pond pump needs change: %s -> %s\n", millis(),
         relayStateName(pondRelayLastKnown), pondDesired ? "ON" : "OFF");
    if (sendRelayCommand(NODE_POND, PKT_SET_POND, pondDesired)) {
      mutex_enter_blocking(&nodeStatusMutex);
      pondStatus.pumpState = pondDesired ? RELAY_ON : RELAY_OFF;
      mutex_exit(&nodeStatusMutex);
    }
  }
  if (fountainDesired != fountain1LastKnown) {
    RLOG("[%lu] fountain1 needs change: %s -> %s\n", millis(),
         relayStateName(fountain1LastKnown), fountainDesired ? "ON" : "OFF");
    if (sendRelayCommand(NODE_FOUNTAIN1, PKT_SET_RELAY, fountainDesired)) {
      mutex_enter_blocking(&nodeStatusMutex);
      fountain1Status.relayState = fountainDesired ? RELAY_ON : RELAY_OFF;
      mutex_exit(&nodeStatusMutex);
    }
  }
  if (fountainDesired != fountain2LastKnown) {
    RLOG("[%lu] fountain2 needs change: %s -> %s\n", millis(),
         relayStateName(fountain2LastKnown), fountainDesired ? "ON" : "OFF");
    if (sendRelayCommand(NODE_FOUNTAIN2, PKT_SET_RELAY, fountainDesired)) {
      mutex_enter_blocking(&nodeStatusMutex);
      fountain2Status.relayState = fountainDesired ? RELAY_ON : RELAY_OFF;
      mutex_exit(&nodeStatusMutex);
    }
  }
}

// Full poll cycle: poll pond + all relay nodes, then decide on commands.
void RadioManager::runPollCycle() {
  RLOG("[%lu] --- poll cycle ---\n", millis());
  pollPondNode();
  pollRelayNode(NODE_GATE, gateStatus);
  pollRelayNode(NODE_FOUNTAIN1, fountain1Status);
  pollRelayNode(NODE_FOUNTAIN2, fountain2Status);
  decideAndSendCommands();
}

// Snapshot of active system errors for the UI (thread-safe). Node timeouts
// are listed first — they mean devices are uncontrolled; RTC death is last
// because it only degrades schedule quality. ERR_NONE entries fill the
// remainder of the array when fewer than 5 errors are active.
void RadioManager::getSystemErrors(SystemErrors &out) {
  mutex_enter_blocking(&nodeStatusMutex);
  out.count = 0;
  for (uint8_t i = 0; i < 5; i++)
    out.codes[i] = ERR_NONE;

  if (gateStatus.error != NODE_OK)
    out.codes[out.count++] = ERR_GATE_OFFLINE;
  if (pondStatus.error != NODE_OK)
    out.codes[out.count++] = ERR_POND_OFFLINE;
  if (fountain1Status.error != NODE_OK)
    out.codes[out.count++] = ERR_FOUNTAIN1_OFFLINE;
  if (fountain2Status.error != NODE_OK)
    out.codes[out.count++] = ERR_FOUNTAIN2_OFFLINE;
  if (!pondStatus.rtcOk && pondStatus.error == NODE_OK)
    out.codes[out.count++] = ERR_RTC_DEAD;
  mutex_exit(&nodeStatusMutex);
}

#line 1 "/home/storm/Projects/WindRadio/client/RadioManager.cpp"
#include "RadioManager.h"
#include "SystemData.h"

// RadioManager.cpp — Core 1 thread for polling remote nodes and controlling
// relays.

void RadioManager::init() {
  mutex_init(&nodeStatusMutex);

  // Initialize cached status structs with safe defaults
  pondStatus = {0, 0, 0, 0.0, false, 0, NODE_OK, 0};
  gateStatus = {false, 0, NODE_OK, 0};
  fountain1Status = {false, 0, NODE_OK, 0};
  fountain2Status = {false, 0, NODE_OK, 0};
}

void RadioManager::loop() {
  // Run a full poll cycle every POLL_CYCLE_MS
  if (millis() - lastPollCycle >= POLL_CYCLE_MS) {
    lastPollCycle = millis();
    runPollCycle();
  }
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
  mutex_enter_blocking(&nodeStatusMutex);
  out = (index == 0) ? fountain1Status : fountain2Status;
  mutex_exit(&nodeStatusMutex);
}

// Poll a single node and return its response packet. Returns false on timeout.
bool RadioManager::pollNode(uint8_t nodeId, WindRadioPacket &outResponse,
                            unsigned long timeoutMs) {
  WindRadioPacket requestPacket;
  requestPacket.type = PKT_POLL_REQUEST;

  if (!sendPacket(nodeId, requestPacket)) {
    return false;
  }

  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    WindRadioPacket incomingPacket;
    if (receivePacket(incomingPacket)) {
      if (incomingPacket.version != PROTOCOL_VERSION) {
        continue; // ignore outdated protocol versions
      }
      if (incomingPacket.fromNode != nodeId) {
        continue; // ignore packets from a different node
      }
      outResponse = incomingPacket;
      return true;
    }
  }
  return false;
}

// Poll the pond node for wind data + pump relay status, then feed the
// wind conditions into the shared CurrentConditions struct.
void RadioManager::pollPondNode() {
  WindRadioPacket responsePacket;
  CurrentConditions receivedCurrentConditions;
  bool ok = pollNode(NODE_POND, responsePacket, POLL_TIMEOUT_MS) &&
            responsePacket.type == PKT_POND_STATUS;

  mutex_enter_blocking(&nodeStatusMutex);
  if (ok) {
    pondStatus.hours = responsePacket.pondStatus.hours;
    pondStatus.minutes = responsePacket.pondStatus.minutes;
    pondStatus.windSpeed = responsePacket.pondStatus.windSpeed;
    pondStatus.temperature = responsePacket.pondStatus.temperature;
    pondStatus.relayOn = responsePacket.pondStatus.relayOn;
    pondStatus.missedPolls = 0;
    pondStatus.error = NODE_OK;
    pondStatus.lastSuccessMs = millis();
  } else {
    if (pondStatus.missedPolls < 255)
      pondStatus.missedPolls++;
    if (pondStatus.missedPolls >= 3)
      pondStatus.error = NODE_ERR_TIMEOUT;
  }
  mutex_exit(&nodeStatusMutex);

  if (ok) {
    updateConditionsFromPond(responsePacket.pondStatus.windSpeed,
                             responsePacket.pondStatus.temperature,
                             responsePacket.pondStatus.hours,
                             responsePacket.pondStatus.minutes);
  }
}

// Poll a relay (gate or fountain) node for on/off status + error tracking.
void RadioManager::pollRelayNode(uint8_t nodeId, NodeStatus &status) {
  WindRadioPacket response;
  bool ok = pollNode(nodeId, response, POLL_TIMEOUT_MS) &&
            response.type == PKT_RELAY_STATUS;

  mutex_enter_blocking(&nodeStatusMutex);
  if (ok) {
    status.relayOn = response.relayStatus.relayOn;
    status.missedPolls = 0;
    status.error = NODE_OK;
    status.lastSuccessMs = millis();
  } else {
    if (status.missedPolls < 255)
      status.missedPolls++;
    if (status.missedPolls >= 3)
      status.error = NODE_ERR_TIMEOUT;
  }
  mutex_exit(&nodeStatusMutex);
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
void RadioManager::sendRelayCommand(uint8_t nodeId, PacketType type,
                                    bool relayOn) {
  WindRadioPacket cmd;
  cmd.type = type;
  cmd.setRelay.relayOn = relayOn;
  sendPacket(nodeId, cmd);
}

// Core decision loop: compare desired vs. actual relay states and send
// commands only when a change is needed.
void RadioManager::decideAndSendCommands() {
  DeviceSettings gateSettings, pondSettings, fountainSettings;
  getDeviceSettings(DEVICE_GATE, gateSettings);
  getDeviceSettings(DEVICE_POND, pondSettings);
  getDeviceSettings(DEVICE_FOUNTAINS, fountainSettings);

  mutex_enter_blocking(&nodeStatusMutex);
  bool windStale = (pondStatus.error != NODE_OK);
  int windSpeed = pondStatus.windSpeed;
  int hours = pondStatus.hours;
  int minutes = pondStatus.minutes;
  bool gateLastKnown = gateStatus.relayOn;
  bool pondRelayLastKnown = pondStatus.relayOn;
  bool fountain1LastKnown = fountain1Status.relayOn;
  bool fountain2LastKnown = fountain2Status.relayOn;
  mutex_exit(&nodeStatusMutex);

  bool gateDesired =
      computeDesiredState(gateSettings, windStale, windSpeed, hours, minutes);
  bool pondDesired =
      computeDesiredState(pondSettings, windStale, windSpeed, hours, minutes);
  bool fountainDesired = computeDesiredState(fountainSettings, windStale,
                                             windSpeed, hours, minutes);

  // Only send commands when the desired state differs from reality
  if (gateDesired != gateLastKnown) {
    sendRelayCommand(NODE_GATE, PKT_SET_RELAY, gateDesired);
  }
  if (pondDesired != pondRelayLastKnown) {
    sendRelayCommand(NODE_POND, PKT_SET_POND, pondDesired);
  }
  if (fountainDesired != fountain1LastKnown) {
    sendRelayCommand(NODE_FOUNTAIN1, PKT_SET_RELAY, fountainDesired);
  }
  if (fountainDesired != fountain2LastKnown) {
    sendRelayCommand(NODE_FOUNTAIN2, PKT_SET_RELAY, fountainDesired);
  }
}

// Full poll cycle: poll pond + all relay nodes, then decide on commands.
void RadioManager::runPollCycle() {
  Serial.println("starting poll cycle");
  pollPondNode();
  pollRelayNode(NODE_GATE, gateStatus);
  pollRelayNode(NODE_FOUNTAIN1, fountain1Status);
  pollRelayNode(NODE_FOUNTAIN2, fountain2Status);
  decideAndSendCommands();
}

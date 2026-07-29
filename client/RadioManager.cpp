#include "RadioManager.h"

void RadioManager::init() {
  mutex_init(&nodeStatusMutex);

  // Initialize structs with safe defaults
  pondStatus = {0, 0, 0, false, 0, NODE_OK, 0};
  gateStatus = {false, 0, NODE_OK, 0};
  fountain1Status = {false, 0, NODE_OK, 0};
  fountain2Status = {false, 0, NODE_OK, 0};
}

void RadioManager::loop() {
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

bool RadioManager::pollNode(uint8_t nodeId, WindRadioPacket &outResponse,
                            unsigned long timeoutMs) {
  WindRadioPacket req;
  req.type = PKT_POLL_REQUEST;

  if (!sendPacket(nodeId, req)) {
    return false;
  }

  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    WindRadioPacket incoming;
    if (receivePacket(incoming)) {
      if (incoming.version != PROTOCOL_VERSION) {
        continue;
      }
      outResponse = incoming;
      return true;
    }
  }
  return false;
}

void RadioManager::pollPondNode() {
  WindRadioPacket response;
  bool ok = pollNode(NODE_POND, response, POLL_TIMEOUT_MS) &&
            response.type == PKT_POND_STATUS;

  mutex_enter_blocking(&nodeStatusMutex);
  if (ok) {
    pondStatus.hours = response.pondStatus.hours;
    pondStatus.minutes = response.pondStatus.minutes;
    pondStatus.windSpeed = response.pondStatus.windSpeed;
    pondStatus.relayOn = response.pondStatus.relayOn;
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
    updateConditionsFromPond(response.pondStatus.windSpeed,
                             response.pondStatus.hours,
                             response.pondStatus.minutes);
  }
}

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

bool RadioManager::isWithinSchedule(int hours, int minutes,
                                    const DeviceSettings &s) {
  int nowMin = hours * 60 + minutes;
  int startMin = s.startHour * 60 + s.startMin;
  int endMin = s.endHour * 60 + s.endMin;

  if (startMin <= endMin) {
    return nowMin >= startMin && nowMin < endMin;
  } else {
    return nowMin >= startMin || nowMin < endMin;
  }
}

bool RadioManager::computeDesiredState(const DeviceSettings &s, bool windStale,
                                       int windSpeed, int hours, int minutes) {
  if (windStale)
    return false;

  switch (s.mode) {
  case MODE_OFF:
    return false;
  case MODE_MANUAL_ON:
    return true;
  case MODE_AUTO:
  default:
    return isWithinSchedule(hours, minutes, s) && windSpeed <= s.windLimit;
  }
}

void RadioManager::sendRelayCommand(uint8_t nodeId, PacketType type,
                                    bool relayOn) {
  WindRadioPacket cmd;
  cmd.type = type;
  cmd.setRelay.relayOn = relayOn;
  sendPacket(nodeId, cmd);
}

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

void RadioManager::runPollCycle() {
  pollPondNode();
  pollRelayNode(NODE_GATE, gateStatus);
  pollRelayNode(NODE_FOUNTAIN1, fountain1Status);
  pollRelayNode(NODE_FOUNTAIN2, fountain2Status);
  decideAndSendCommands();
}

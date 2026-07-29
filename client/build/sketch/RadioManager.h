#line 1 "/home/storm/Projects/WindRadio/client/RadioManager.h"
#pragma once
#include "SystemData.h"
#include "WindRadioCommon.h"
#include "pico/mutex.h"
#include <Arduino.h>

class RadioManager {
public:
  void init();
  void loop();

  // Data accessors for Core 0 (e.g. for display purposes)
  void getPondNodeStatus(PondNodeStatus &out);
  void getGateStatus(NodeStatus &out);
  void getFountainStatus(uint8_t index, NodeStatus &out);

private:
  PondNodeStatus pondStatus;
  NodeStatus gateStatus;
  NodeStatus fountain1Status;
  NodeStatus fountain2Status;
  mutex_t nodeStatusMutex;

  unsigned long lastPollCycle = 0;
  const unsigned long POLL_CYCLE_MS = 30000;
  const unsigned long POLL_TIMEOUT_MS = 500;

  bool pollNode(uint8_t nodeId, WindRadioPacket &outResponse,
                unsigned long timeoutMs);
  void pollPondNode();
  void pollRelayNode(uint8_t nodeId, NodeStatus &status);

  bool isWithinSchedule(int hours, int minutes, const DeviceSettings &s);
  bool computeDesiredState(const DeviceSettings &s, bool windStale,
                           int windSpeed, int hours, int minutes);

  void sendRelayCommand(uint8_t nodeId, PacketType type, bool relayOn);
  void decideAndSendCommands();
  void runPollCycle();
};

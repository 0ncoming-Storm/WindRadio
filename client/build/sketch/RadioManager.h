#line 1 "/home/storm/Projects/WindRadio/client/RadioManager.h"
#pragma once
#include "SystemData.h"
#include "WindRadioCommon.h"
#include "pico/mutex.h"
#include <Arduino.h>

// RadioManager.h — runs on Core 1 of the base station.
// Polls remote radio nodes (pond, gate, fountains) on a fixed interval,
// tracks their status, and decides whether relays should be toggled.
//
// Public accessors are safe to call from Core 0 (display) — they use a
// mutex to read the latest cached status.

class RadioManager {
public:
  void init();
  void loop();

  // Data accessors for Core 0 (e.g. for display purposes)
  void getPondNodeStatus(PondNodeStatus &out);
  void getGateStatus(NodeStatus &out);
  void getFountainStatus(uint8_t index, NodeStatus &out);
  static bool computeDesiredState(const DeviceSettings &s, bool windStale,
                                  int windSpeed, int hours, int minutes);

private:
  // Cached status from each remote node
  PondNodeStatus pondStatus;
  NodeStatus gateStatus;
  NodeStatus fountain1Status;
  NodeStatus fountain2Status;
  mutex_t nodeStatusMutex;

  unsigned long lastPollCycle = 0;
  const unsigned long POLL_CYCLE_MS = 30000; // poll every 30s
  const unsigned long POLL_TIMEOUT_MS = 500; // per-node response timeout

  // Low-level radio helpers
  bool pollNode(uint8_t nodeId, WindRadioPacket &outResponse,
                unsigned long timeoutMs);
  void pollPondNode();
  void pollRelayNode(uint8_t nodeId, NodeStatus &status);

  // Scheduling / decision logic
  static bool isWithinSchedule(int hours, int minutes, const DeviceSettings &s);

  void sendRelayCommand(uint8_t nodeId, PacketType type, bool relayOn);
  void decideAndSendCommands();
  void runPollCycle();
};

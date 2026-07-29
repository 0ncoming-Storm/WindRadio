#line 1 "/home/storm/Projects/WindRadio/client/SystemData.h"
#pragma once
#include <Arduino.h>

enum DeviceMode { MODE_OFF, MODE_AUTO, MODE_MANUAL_ON };
enum DeviceID { DEVICE_GATE, DEVICE_POND, DEVICE_FOUNTAINS, DEVICE_COUNT };

struct DeviceSettings {
  char name[12];
  int windLimit;
  int startHour, startMin, endHour, endMin;
  DeviceMode mode;
};

struct CurrentConditions {
  int windSpeed;
  float temperature;
  int hours;
  int minutes;
};

enum NodeError : uint8_t {
  NODE_OK,
  NODE_ERR_TIMEOUT, // 3+ consecutive missed polls
};

struct NodeStatus {
  bool relayOn;
  uint8_t missedPolls;
  NodeError error;
  unsigned long lastSuccessMs;
};

struct PondNodeStatus {
  uint8_t hours;
  uint8_t minutes;
  int windSpeed;
  bool relayOn;
  uint8_t missedPolls;
  NodeError error;
  unsigned long lastSuccessMs;
};

// Thread-safe accessors implemented in main.cpp
extern void getDeviceSettings(DeviceID id, DeviceSettings &out);
extern void setDeviceSettings(DeviceID id, const DeviceSettings &in);
extern void getCurrentConditions(CurrentConditions &out);
extern void updateConditionsFromPond(int windSpeed, int hours, int minutes);

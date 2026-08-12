#pragma once
#include <Arduino.h>

// SystemData.h — shared data types used across all WindRadio firmware.
// These structs are passed between Core 0 (UI) and Core 1 (radio) in the
// base station, and referenced by the remote pond/gate/fountain nodes.

enum DeviceMode { MODE_OFF, MODE_AUTO, MODE_MANUAL_ON };
enum DeviceID { DEVICE_GATE, DEVICE_POND, DEVICE_FOUNTAINS, DEVICE_COUNT };

// Per-device settings. RAM only: changes are lost on reboot.
struct DeviceSettings {
  char name[12];
  int windLimit;               // max wind speed (KM/H) before auto-shutoff
  uint8_t startHour, startMin; // schedule start
  uint8_t endHour, endMin;     // schedule end
  DeviceMode mode;             // OFF / AUTO / MANUAL_ON
};

// Snapshot of current environmental conditions.
struct CurrentConditions {
  int windSpeed;     // from pond anemometer, in KM/H
  float temperature; // from pond sensor (if equipped)
  int hours;         // from pond RTC
  int minutes;
};

// Per-node communication health.
enum NodeError : uint8_t {
  NODE_OK,
  NODE_ERR_TIMEOUT, // 3+ consecutive missed polls
};

// Status reported by a relay node (gate or fountain).
struct NodeStatus {
  bool relayOn;
  uint8_t missedPolls;
  NodeError error;
  unsigned long lastSuccessMs;
};

// Extended status from the pond node (includes weather data).
struct PondNodeStatus {
  uint8_t hours;
  uint8_t minutes;
  int windSpeed;
  float temperature;
  bool relayOn;
  uint8_t missedPolls;
  NodeError error;
  unsigned long lastSuccessMs;
};

// Thread-safe accessors implemented in client.ino
extern void getDeviceSettings(DeviceID id, DeviceSettings &out);
extern void setDeviceSettings(DeviceID id, const DeviceSettings &in);
extern void getCurrentConditions(CurrentConditions &out);
extern void updateConditionsFromPond(int windSpeed, float temperature,
                                     int hours, int minutes);

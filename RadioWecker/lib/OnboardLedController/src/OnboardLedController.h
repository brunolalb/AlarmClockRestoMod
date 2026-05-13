#pragma once

#include <Arduino.h>

class HeartbeatLedController {
 public:
  explicit HeartbeatLedController(unsigned long intervalMs);

  bool update(unsigned long nowMs);

 private:
  unsigned long intervalMs_;
  unsigned long lastToggleMs_;
  bool state_;
};

class ActivityPulseController {
 public:
  ActivityPulseController();

  void trigger(unsigned long nowMs, unsigned long durationMs);
  bool isActive(unsigned long nowMs) const;

 private:
  unsigned long activeUntilMs_;
};

class OnboardLedController {
 public:
  explicit OnboardLedController(uint8_t pin, unsigned long heartbeatIntervalMs = 500);

  void begin();
  void pulseActivity(unsigned long durationMs = 120);
  void update();

 private:
  uint8_t pin_;
  HeartbeatLedController heartbeat_;
  ActivityPulseController activity_;
};

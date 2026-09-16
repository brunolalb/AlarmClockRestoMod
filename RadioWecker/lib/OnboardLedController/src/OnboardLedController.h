#pragma once

#include <Arduino.h>
#include <timers.h>

class OnboardLedController {
 public:
  explicit OnboardLedController(uint8_t pin, uint32_t heartbeatIntervalMs = 500);

  bool initialize();
  void pulseActivity(unsigned long durationMs = 120);
  void update();

 private:
  uint8_t pin_;
  uint32_t heartbeatIntervalMs_;
  TimerHandle_t xTimer;
  static void timerCallback(TimerHandle_t timer);
};

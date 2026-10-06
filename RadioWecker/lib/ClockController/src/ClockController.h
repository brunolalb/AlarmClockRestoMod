#pragma once

#include <Arduino.h>
#include <thread>
#include <RTClib.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/timers.h>

class Coordinator;

class ClockController {
 public:
  struct TimeConfig {
    String ntpServer;
    String timezonePosix;
    int16_t timeOffsetMinutes;
    int daylightOffsetSeconds;
    uint32_t ntpSyncIntervalMs;
  };
  ClockController(uint8_t rtcSqwPin,
                  uint8_t i2cSdaPin,
                  uint8_t i2cSclPin,
                  uint32_t i2cFrequencyHz = 100000);
  bool initialize(const TimeConfig* default_config, Coordinator* coordinator);

  bool isReady() const;
  bool externalRTCReady() const;
  DateTime ntpLastSynchronized() const;
  void applyTimeConfig(const String& timezonePosix, int16_t timeOffsetMinutes);
  String timezonePosix() const;
  int16_t timeOffsetMinutes() const;

 private:
  struct HardwareConfig {
    uint8_t rtcSqwPin_;
    uint8_t i2cSdaPin_;
    uint8_t i2cSclPin_;
    uint32_t i2cFrequencyHz_;
  };
  HardwareConfig hwConfig_;
  TimeConfig config_;
  Coordinator* coordinator_ = nullptr;
  std::thread _updateTask;

  enum class Message : uint8_t { NTPSynced, NextMinute }; // internal messages
  QueueHandle_t messageQueue_ = nullptr; // queue for messages
  static ClockController* activeInstance_; // for the static callback functions to access the active instance
  TimerHandle_t nextMinuteTimer_ = nullptr; // timer for scheduling the next minute message

  void updateTask();
  void syncExternalRtc();

  static void sntpSyncCallback(struct timeval* tv); // callback for SNTP synchronization
  static void onNextMinuteTimer(TimerHandle_t xTimer); // callback for the next minute timer

  bool rtcReady_ = false;
  int displayedHHMM_ = 0;
  DateTime ntpLastSynchronized_;
};

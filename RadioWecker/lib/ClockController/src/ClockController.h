#pragma once

#include <Arduino.h>
#include <RTClib.h>

class ClockController {
 public:
  struct TimeConfig {
    String ntpServer;
    String timezonePosix;
    int16_t timeOffsetMinutes;
    int daylightOffsetSeconds;
    uint32_t ntpSyncIntervalMs;
    uint32_t ntpRetryIntervalMs;
  };
  ClockController(uint8_t rtcSqwPin,
                  uint8_t i2cSdaPin,
                  uint8_t i2cSclPin,
                  uint32_t i2cFrequencyHz = 100000);
  bool initialize(const TimeConfig* default_config);
  void update();

  bool isReady() const;
  bool isTimeValid() const;
  int displayValueHHMM() const;
  bool isNtpSynchronized() const;
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
  bool initializeClockFromDateTime(const DateTime& now);
  bool initializeRtcTimeFromChip();
  void updateDisplayedValue();
  void advanceSoftwareClockOneSecond();
  void syncFromNtpIfNeeded();
  bool syncFromNtp();
  void onRtcSecondTick();

  static void IRAM_ATTR handleRtcSecondTickISR();
  static ClockController* activeInstance_;

  HardwareConfig hwConfig_;
  TimeConfig config_;

  bool ready_ = false; //todo: rename to RTC_ready or something
  bool timeValid_ = false;
  DateTime currentTime_;
  int displayedHHMM_ = 0;
  volatile bool secondTick_ = false;
  bool ntpConfigured_ = false;
  bool ntpSynchronized_ = false;
  unsigned long nextNtpSyncAttemptMs_ = 0;
};

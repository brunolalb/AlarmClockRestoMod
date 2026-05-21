#pragma once

#include <Arduino.h>

class ClockController {
 public:
  ClockController(uint8_t rtcSqwPin,
                  uint8_t i2cSdaPin,
                  uint8_t i2cSclPin,
                  uint32_t i2cFrequencyHz = 100000,
                  const char* ntpServer = "pool.ntp.org",
                  long gmtOffsetSeconds = 0,
                  int daylightOffsetSeconds = 0,
                  uint32_t ntpSyncIntervalMs = 6UL * 60UL * 60UL * 1000UL,
                  uint32_t ntpRetryIntervalMs = 60UL * 1000UL);
  bool begin();
  void update();

  bool isReady() const;
  bool isTimeValid() const;
  int displayValueHHMM() const;
  bool isNtpSynchronized() const;
  void applyTimeConfig(const String& timezonePosix, int16_t timeOffsetMinutes);
  String timezonePosix() const;
  int16_t timeOffsetMinutes() const;

 private:
  bool initializeClockFromTm(const struct tm& now);
  bool initializeClockFromTimeString(const String& timeStr);
  bool initializeRtcTimeFromChip();
  void updateDisplayedValue();
  void advanceSoftwareClockOneSecond();
  void syncFromNtpIfNeeded();
  bool syncFromNtp();
  void onRtcSecondTick();

  static void IRAM_ATTR handleRtcSecondTickISR();
  static ClockController* activeInstance_;

  uint8_t rtcSqwPin_;
  uint8_t i2cSdaPin_;
  uint8_t i2cSclPin_;
  uint32_t i2cFrequencyHz_;
  const char* ntpServer_;
  long gmtOffsetSeconds_;
  int daylightOffsetSeconds_;
  uint32_t ntpSyncIntervalMs_;
  uint32_t ntpRetryIntervalMs_;
  String timezonePosix_;
  int16_t timeOffsetMinutes_ = 0;

  bool ready_ = false;
  bool timeValid_ = false;
  uint8_t hour_ = 0;
  uint8_t minute_ = 0;
  uint8_t second_ = 0;
  int displayedHHMM_ = 0;
  volatile bool secondTick_ = false;
  bool ntpConfigured_ = false;
  bool ntpSynchronized_ = false;
  unsigned long nextNtpSyncAttemptMs_ = 0;
};

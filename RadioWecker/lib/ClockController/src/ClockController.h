#pragma once

#include <Arduino.h>

class ClockController {
 public:
  bool begin(uint8_t rtcSqwPin);
  void update();

  bool isReady() const;
  bool isTimeValid() const;
  int displayValueMMSS() const;

 private:
  bool initializeClockFromTm(const struct tm& now);
  bool initializeClockFromTimeString(const String& timeStr);
  bool initializeRtcTimeFromChip();
  void advanceSoftwareClockOneSecond();
  void onRtcSecondTick();

  static void IRAM_ATTR handleRtcSecondTickISR();
  static ClockController* activeInstance_;

  bool ready_ = false;
  bool timeValid_ = false;
  uint8_t hour_ = 0;
  uint8_t minute_ = 0;
  uint8_t second_ = 0;
  int displayedMMSS_ = 0;
  volatile bool secondTick_ = false;
};

#pragma once

#include <Arduino.h>
#include <TM1637.h>

class DisplayManager {
 public:
  enum class SeparatorMode : uint8_t {
    None,
    Colon,
    Dots,
  };

  DisplayManager(uint8_t clkPin, uint8_t dioPin, SeparatorMode separatorMode = SeparatorMode::Colon);

  void begin(uint8_t brightness = 7);
  void setBrightness(uint8_t brightness);
  uint8_t brightness() const;
  void showTimeHHMM(int timeValue);
  void showRtcFailure();
  void showSdFailure();
  void showSdSelfTestResult(bool passed);

 private:
  void applySeparatorMode(SeparatorMode separatorMode);

  TM1637 display_;
  uint8_t brightness_ = 7;
  SeparatorMode separatorMode_ = SeparatorMode::Colon;
};

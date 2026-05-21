#pragma once

#include <Arduino.h>
#include <TM1637.h>

class DisplayManager {
 public:
  DisplayManager(uint8_t clkPin, uint8_t dioPin);

  void begin(uint8_t brightness = 7);
    void showTimeHHMM(int timeValue);
  void showRtcFailure();
  void showSdFailure();
  void showSdSelfTestResult(bool passed);

 private:
    TM1637 display_;
};

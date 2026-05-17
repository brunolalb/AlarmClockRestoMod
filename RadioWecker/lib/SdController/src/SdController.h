#pragma once

#include <Arduino.h>

class SdController {
 public:
  struct InitResult {
    bool ready;
    bool selfTestPassed;
  };
  SdController(uint8_t csPin);

  InitResult initialize();
  bool isReady() const;
  bool selfTestPassed() const;

 private:
  bool runSelfTest();

  uint8_t csPin_;
  bool ready_ = false;
  bool selfTestPassed_ = false;
};

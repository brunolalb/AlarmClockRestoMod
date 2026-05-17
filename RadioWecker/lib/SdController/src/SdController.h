#pragma once

#include <Arduino.h>

class SdController {
 public:
  struct InitResult {
    bool ready;
    bool selfTestPassed;
  };

  InitResult initialize(uint8_t csPin);
  bool isReady() const;
  bool selfTestPassed() const;

 private:
  bool runSelfTest();

  bool ready_ = false;
  bool selfTestPassed_ = false;
};

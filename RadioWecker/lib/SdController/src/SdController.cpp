#include "SdController.h"

#include <SD.h>

SdController::SdController(uint8_t csPin)
    : csPin_(csPin) {}

bool SdController::runSelfTest() {
  static const char* kTestPath = "/sdtest.txt";
  static const char* kMarker = "TM1637_SD_OK";

  if (SD.exists(kTestPath) && !SD.remove(kTestPath)) {
    return false;
  }

  File out = SD.open(kTestPath, FILE_WRITE);
  if (!out) {
    return false;
  }
  out.println(kMarker);
  out.close();

  File in = SD.open(kTestPath, FILE_READ);
  if (!in) {
    return false;
  }

  String line = in.readStringUntil('\n');
  in.close();
  line.trim();

  return line == kMarker;
}

SdController::InitResult SdController::initialize() {
  ready_ = SD.begin(csPin_);
  selfTestPassed_ = false;

  if (!ready_) {
    return {ready_, selfTestPassed_};
  }

  selfTestPassed_ = runSelfTest();
  return {ready_, selfTestPassed_};
}

bool SdController::isReady() const {
  return ready_;
}

bool SdController::selfTestPassed() const {
  return selfTestPassed_;
}

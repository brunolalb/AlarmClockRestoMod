#include "SdController.h"

#include <SD.h>

SdController::SdController(uint8_t csPin,
                           uint8_t spiSckPin,
                           uint8_t spiMisoPin,
                           uint8_t spiMosiPin,
                           uint32_t spiFrequencyHz,
                           SPIClass& spiBus)
    : csPin_(csPin),
      spiSckPin_(spiSckPin),
      spiMisoPin_(spiMisoPin),
      spiMosiPin_(spiMosiPin),
      spiFrequencyHz_(spiFrequencyHz),
      spiBus_(&spiBus) {}

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
  spiBus_->begin(spiSckPin_, spiMisoPin_, spiMosiPin_, csPin_);
  ready_ = SD.begin(csPin_, *spiBus_, spiFrequencyHz_);
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

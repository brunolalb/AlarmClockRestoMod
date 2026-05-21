#pragma once

#include <Arduino.h>
#include <SPI.h>

class SdController {
 public:
  struct InitResult {
    bool ready;
    bool selfTestPassed;
  };
  SdController(uint8_t csPin,
               uint8_t spiSckPin,
               uint8_t spiMisoPin,
               uint8_t spiMosiPin,
               uint32_t spiFrequencyHz = 10000000,
               SPIClass& spiBus = SPI);

  InitResult initialize();
  bool isReady() const;
  bool selfTestPassed() const;

 private:
  bool runSelfTest();

  uint8_t csPin_;
  uint8_t spiSckPin_;
  uint8_t spiMisoPin_;
  uint8_t spiMosiPin_;
  uint32_t spiFrequencyHz_;
  SPIClass* spiBus_;
  bool ready_ = false;
  bool selfTestPassed_ = false;
};

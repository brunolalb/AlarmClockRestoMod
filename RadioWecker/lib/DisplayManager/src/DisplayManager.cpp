#include "DisplayManager.h"

DisplayManager::DisplayManager(uint8_t clkPin, uint8_t dioPin)
    : display_(clkPin, dioPin) {}

void DisplayManager::begin(uint8_t brightness) {
  display_.init();
  display_.setBrightness(brightness);
  display_.colonOff();
}

void DisplayManager::showTimeHHMM(int timeValue) {
  display_.colonOn();
  display_.display(timeValue, false, true);
}

void DisplayManager::showRtcFailure() {
  display_.colonOff();
  display_.display("RTCF");
}

void DisplayManager::showSdFailure() {
  display_.colonOff();
  display_.display("SDFL");
}

void DisplayManager::showSdSelfTestResult(bool passed) {
  display_.colonOff();
  display_.display(passed ? "TSTP" : "TSTF");
}

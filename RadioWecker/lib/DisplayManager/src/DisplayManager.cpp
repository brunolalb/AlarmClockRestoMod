#include "DisplayManager.h"

DisplayManager::DisplayManager(uint8_t clkPin, uint8_t dioPin)
    : display_(clkPin, dioPin) {}

void DisplayManager::begin(uint8_t brightness) {
  display_.init();
  setBrightness(brightness);
  display_.colonOff();
}

void DisplayManager::setBrightness(uint8_t brightness) {
  brightness_ = brightness > 7 ? 7 : brightness;
  display_.setBrightness(brightness_);
}

uint8_t DisplayManager::brightness() const {
  return brightness_;
}

void DisplayManager::showTimeHHMM(int timeValue) {
  display_.colonOn();
  char hhmm[5];
  snprintf(hhmm, sizeof(hhmm), "%04d", timeValue);
  String text(hhmm);
  if (text.length() == 4 && text.charAt(0) == '0') {
    text.setCharAt(0, ' ');
  }
  display_.display(text, false, false);
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

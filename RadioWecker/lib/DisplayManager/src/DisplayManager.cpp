#include "DisplayManager.h"

DisplayManager::DisplayManager(uint8_t clkPin, uint8_t dioPin, SeparatorMode separatorMode)
    : display_(clkPin, dioPin),
      separatorMode_(separatorMode) {}

void DisplayManager::begin(uint8_t brightness) {
  display_.init();
  setBrightness(brightness);
  display_.clearScreen();
  applySeparatorMode(separatorMode_);
}

void DisplayManager::setBrightness(uint8_t brightness) {
  brightness_ = brightness > 7 ? 7 : brightness;
  display_.setBrightness(brightness_);
}

uint8_t DisplayManager::brightness() const {
  return brightness_;
}

void DisplayManager::showTimeHHMM(int timeValue) {
  applySeparatorMode(separatorMode_);
  char hhmm[5];
  snprintf(hhmm, sizeof(hhmm), "%04d", timeValue);
  String text(hhmm);
  if (text.length() == 4 && text.charAt(0) == '0') {
    text.setCharAt(0, ' ');
  }
  display_.display(text, false, false);
}

void DisplayManager::showRtcFailure() {
  applySeparatorMode(SeparatorMode::None);
  display_.display("RTCF");
}

void DisplayManager::showSdFailure() {
  applySeparatorMode(SeparatorMode::None);
  display_.display("SDFL");
}

void DisplayManager::showSdSelfTestResult(bool passed) {
  applySeparatorMode(SeparatorMode::None);
  display_.display(passed ? "TSTP" : "TSTF");
}

void DisplayManager::applySeparatorMode(SeparatorMode separatorMode) {
  switch (separatorMode) {
    case SeparatorMode::Dots:
      display_.setDp(0x02);
      break;
    case SeparatorMode::Colon:
      display_.setDp(0x00);
      display_.colonOn();
      break;
    case SeparatorMode::None:
    default:
      display_.setDp(0x00);
      display_.colonOff();
      break;
  }
}

#include "DisplayManager.h"

DisplayManager::DisplayManager( uint8_t clkPin,
                                uint8_t dioPin,
                                SeparatorMode separatorMode)
    : display_(dioPin, clkPin),
      separatorMode_(separatorMode) {}

bool DisplayManager::initialize(uint8_t brightness) {
  display_.begin();
  setBrightness(brightness);
  display_.clearDisplay();
  return true;
}

void DisplayManager::setBrightness(uint8_t brightness) {
  brightness_ = brightness > 7 ? 7 : brightness;
  display_.setupDisplay(true, brightness_);
}

uint8_t DisplayManager::brightness() const {
  return brightness_;
}

TM1637* DisplayManager::display() {
  return &display_;
}

void DisplayManager::showTimeHHMM(int timeValue) {
  char hhmm[9];
  snprintf(hhmm, sizeof(hhmm), "%04d    ", timeValue);
  if (hhmm[0] == '0') {
    hhmm[0] = ' ';
  }
  displayText(hhmm, separatorMode_);
}

void DisplayManager::showRtcFailure() {
  displayText("RTCF    ", SeparatorMode::None);
}

void DisplayManager::showSdFailure() {
  displayText("SDFL    ", SeparatorMode::None);
}

void DisplayManager::displayText(const char* text, SeparatorMode separatorMode) {
  const word dots = separatorMode == SeparatorMode::None ? 0 : (1 << 1);
  display_.setDisplayToString(text, dots);
}

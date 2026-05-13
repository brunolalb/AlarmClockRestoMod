#include "OnboardLedController.h"

HeartbeatLedController::HeartbeatLedController(unsigned long intervalMs)
    : intervalMs_(intervalMs), lastToggleMs_(0), state_(false) {}

bool HeartbeatLedController::update(unsigned long nowMs) {
  if (nowMs - lastToggleMs_ >= intervalMs_) {
    lastToggleMs_ = nowMs;
    state_ = !state_;
  }
  return state_;
}

ActivityPulseController::ActivityPulseController() : activeUntilMs_(0) {}

void ActivityPulseController::trigger(unsigned long nowMs, unsigned long durationMs) {
  activeUntilMs_ = nowMs + durationMs;
}

bool ActivityPulseController::isActive(unsigned long nowMs) const {
  return nowMs < activeUntilMs_;
}

OnboardLedController::OnboardLedController(uint8_t pin, unsigned long heartbeatIntervalMs)
    : pin_(pin), heartbeat_(heartbeatIntervalMs), activity_() {}

void OnboardLedController::begin() {
  pinMode(pin_, OUTPUT);
  digitalWrite(pin_, LOW);
}

void OnboardLedController::pulseActivity(unsigned long durationMs) {
  activity_.trigger(millis(), durationMs);
}

void OnboardLedController::update() {
  const unsigned long nowMs = millis();
  const bool ledOn = activity_.isActive(nowMs) ? true : heartbeat_.update(nowMs);
  digitalWrite(pin_, ledOn ? HIGH : LOW);
}

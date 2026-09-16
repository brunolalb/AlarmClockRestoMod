#include "OnboardLedController.h"
#include <timers.h>

OnboardLedController::OnboardLedController(uint8_t pin, uint32_t heartbeatIntervalMs)
    : pin_(pin), 
      heartbeatIntervalMs_(heartbeatIntervalMs){}

void OnboardLedController::timerCallback(TimerHandle_t timer) {
  static_cast<OnboardLedController*>(pvTimerGetTimerID(timer))->update();
}

bool OnboardLedController::initialize() {
  pinMode(pin_, OUTPUT);
  digitalWrite(pin_, LOW);

  xTimer = xTimerCreate(
    "HeartBeat", 
    pdMS_TO_TICKS(heartbeatIntervalMs_), 
    pdTRUE, // autoreload
    this, 
    timerCallback);
    
  if (xTimer) {
    xTimerStart(xTimer, 0);
  }
  return true;
}

void OnboardLedController::update() {
  digitalWrite(pin_, digitalRead(pin_) == LOW ? HIGH : LOW);
}

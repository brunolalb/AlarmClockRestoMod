#include "ClockController.h"

#include <I2C_RTC.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>

#define CLOCK_CONTROLLER_USE_DS3231 1
#if CLOCK_CONTROLLER_USE_DS3231
using RtcChip = DS3231;
#else
using RtcChip = DS1307;
#endif

ClockController* ClockController::activeInstance_ = nullptr;

namespace {
RtcChip rtc;
}

ClockController::ClockController(uint8_t rtcSqwPin,
                                 uint8_t i2cSdaPin,
                                 uint8_t i2cSclPin,
                                 uint32_t i2cFrequencyHz,
                                 const char* ntpServer,
                                 long gmtOffsetSeconds,
                                 int daylightOffsetSeconds,
                                 uint32_t ntpSyncIntervalMs,
                                 uint32_t ntpRetryIntervalMs)
    : rtcSqwPin_(rtcSqwPin),
      i2cSdaPin_(i2cSdaPin),
      i2cSclPin_(i2cSclPin),
      i2cFrequencyHz_(i2cFrequencyHz),
      ntpServer_(ntpServer),
      gmtOffsetSeconds_(gmtOffsetSeconds),
      daylightOffsetSeconds_(daylightOffsetSeconds),
      ntpSyncIntervalMs_(ntpSyncIntervalMs),
      ntpRetryIntervalMs_(ntpRetryIntervalMs) {}

bool ClockController::begin() {
  Wire.begin(i2cSdaPin_, i2cSclPin_, i2cFrequencyHz_);

  ready_ = rtc.begin() != 0;
  timeValid_ = false;

  if (!ready_) {
    return false;
  }

  if (!rtc.isRunning()) {
    rtc.startClock();
  }

  timeValid_ = initializeRtcTimeFromChip();

#if CLOCK_CONTROLLER_USE_DS3231
  rtc.enableSqwePin();
  rtc.setOutPin(SQW001Hz);
#else
  rtc.setOutPin(SQW001Hz);
#endif

  pinMode(rtcSqwPin_, INPUT_PULLUP);
  const int sqwInterrupt = digitalPinToInterrupt(rtcSqwPin_);
  if (sqwInterrupt != NOT_AN_INTERRUPT) {
    activeInstance_ = this;
    attachInterrupt(sqwInterrupt, handleRtcSecondTickISR, FALLING);
  }

  syncFromNtpIfNeeded();

  return ready_;
}

void ClockController::update() {
  if (!ready_) {
    return;
  }

  syncFromNtpIfNeeded();

  if (!timeValid_ || !secondTick_) {
    return;
  }

  noInterrupts();
  secondTick_ = false;
  interrupts();

  advanceSoftwareClockOneSecond();
}

bool ClockController::isReady() const {
  return ready_;
}

bool ClockController::isTimeValid() const {
  return timeValid_;
}

int ClockController::displayValueHHMM() const {
  return displayedHHMM_;
}

bool ClockController::isNtpSynchronized() const {
  return ntpSynchronized_;
}

bool ClockController::initializeClockFromTm(const struct tm& now) {
  if (now.tm_hour < 0 || now.tm_hour > 23 ||
      now.tm_min < 0 || now.tm_min > 59 ||
      now.tm_sec < 0 || now.tm_sec > 59) {
    return false;
  }

  hour_ = static_cast<uint8_t>(now.tm_hour);
  minute_ = static_cast<uint8_t>(now.tm_min);
  second_ = static_cast<uint8_t>(now.tm_sec);
  displayedHHMM_ = hour_ * 100 + minute_;
  return true;
}

bool ClockController::initializeClockFromTimeString(const String& timeStr) {
  if (timeStr.length() < 8) {
    return false;
  }

  struct tm now = {};
  now.tm_hour = timeStr.substring(0, 2).toInt();
  now.tm_min = timeStr.substring(3, 5).toInt();
  now.tm_sec = timeStr.substring(6, 8).toInt();
  return initializeClockFromTm(now);
}

bool ClockController::initializeRtcTimeFromChip() {
  return initializeClockFromTimeString(rtc.getTimeString());
}

void ClockController::advanceSoftwareClockOneSecond() {
  second_++;
  if (second_ >= 60) {
    second_ = 0;
    minute_++;
    if (minute_ >= 60) {
      minute_ = 0;
      hour_ = static_cast<uint8_t>((hour_ + 1) % 24);
    }
  }

  displayedHHMM_ = hour_ * 100 + minute_;
}

void ClockController::syncFromNtpIfNeeded() {
  const unsigned long nowMs = millis();
  if (static_cast<long>(nowMs - nextNtpSyncAttemptMs_) < 0) {
    return;
  }

  const bool synced = syncFromNtp();
  nextNtpSyncAttemptMs_ = nowMs + (synced ? ntpSyncIntervalMs_ : ntpRetryIntervalMs_);
}

bool ClockController::syncFromNtp() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  if (!ntpConfigured_) {
    configTime(gmtOffsetSeconds_, daylightOffsetSeconds_, ntpServer_);
    ntpConfigured_ = true;
  }

  struct tm now = {};
  if (!getLocalTime(&now, 1500)) {
    return false;
  }

  const bool initialized = initializeClockFromTm(now);
  if (initialized) {
    ntpSynchronized_ = true;
    Serial.println("NTP sync successful");
  }

  return initialized;
}

void ClockController::onRtcSecondTick() {
  secondTick_ = true;
}

void IRAM_ATTR ClockController::handleRtcSecondTickISR() {
  if (activeInstance_ != nullptr) {
    activeInstance_->onRtcSecondTick();
  }
}

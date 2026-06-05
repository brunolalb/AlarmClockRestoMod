#include "ClockController.h"

#include <RTClib.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>

ClockController* ClockController::activeInstance_ = nullptr;

namespace {
RTC_DS3231 rtc;
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
      ntpRetryIntervalMs_(ntpRetryIntervalMs),
      timezonePosix_("UTC0") {}

bool ClockController::begin() {
  Wire.begin(i2cSdaPin_, i2cSclPin_, i2cFrequencyHz_);

  ready_ = rtc.begin();
  timeValid_ = false;

  if (!ready_) {
    return false;
  }

  timeValid_ = initializeRtcTimeFromChip();
  rtc.writeSqwPinMode(DS3231_SquareWave1Hz);

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

void ClockController::applyTimeConfig(const String& timezonePosix, int16_t timeOffsetMinutes) {
  timezonePosix_ = timezonePosix;
  timeOffsetMinutes_ = timeOffsetMinutes;
  updateDisplayedValue();

  ntpConfigured_ = false;
  ntpSynchronized_ = false;
  nextNtpSyncAttemptMs_ = 0;
}

String ClockController::timezonePosix() const {
  return timezonePosix_;
}

int16_t ClockController::timeOffsetMinutes() const {
  return timeOffsetMinutes_;
}

bool ClockController::initializeClockFromDateTime(const DateTime& now) {
  if (!now.isValid()) {
    return false;
  }

  currentTime_ = now;
  updateDisplayedValue();
  return true;
}

bool ClockController::initializeRtcTimeFromChip() {
  const DateTime now = rtc.now();
  return initializeClockFromDateTime(now);
}

void ClockController::updateDisplayedValue() {
  int totalMinutes = static_cast<int>(currentTime_.hour()) * 60 +
                     static_cast<int>(currentTime_.minute()) +
                     static_cast<int>(timeOffsetMinutes_);
  const int minutesPerDay = 24 * 60;
  totalMinutes %= minutesPerDay;
  if (totalMinutes < 0) {
    totalMinutes += minutesPerDay;
  }

  const int displayHour = totalMinutes / 60;
  const int displayMinute = totalMinutes % 60;
  displayedHHMM_ = displayHour * 100 + displayMinute;
}

void ClockController::advanceSoftwareClockOneSecond() {
  currentTime_ = currentTime_ + TimeSpan(1);
  updateDisplayedValue();
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
    if (timezonePosix_.length() > 0) {
      configTzTime(timezonePosix_.c_str(), ntpServer_);
    } else {
      configTime(gmtOffsetSeconds_, daylightOffsetSeconds_, ntpServer_);
    }
    ntpConfigured_ = true;
  }

  struct tm now = {};
  if (!getLocalTime(&now, 1500)) {
    return false;
  }

  const DateTime ntpNow(now.tm_year + 1900,
                        now.tm_mon + 1,
                        now.tm_mday,
                        now.tm_hour,
                        now.tm_min,
                        now.tm_sec);

  rtc.adjust(ntpNow);

  const bool initialized = initializeClockFromDateTime(ntpNow);
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

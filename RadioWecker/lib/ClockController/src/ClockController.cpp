#include "ClockController.h"

#include <Coordinator.h>
#include <RTClib.h>
#include <WiFi.h>
#include <Wire.h>
#include <time.h>
#include <esp_sntp.h>

ClockController* ClockController::activeInstance_ = nullptr;

namespace {
RTC_DS3231 rtc;
}

ClockController::ClockController(uint8_t rtcSqwPin,
                                 uint8_t i2cSdaPin,
                                 uint8_t i2cSclPin,
                                 uint32_t i2cFrequencyHz)
    : hwConfig_{rtcSqwPin, i2cSdaPin, i2cSclPin, i2cFrequencyHz} {}

bool ClockController::initialize(const TimeConfig* default_config,
                                 Coordinator* coordinator) {
  config_ = *default_config;
  coordinator_ = coordinator;
  activeInstance_ = this;

  // first initialize the I2C RTC
  rtcReady_ = Wire.begin(hwConfig_.i2cSdaPin_,
                         hwConfig_.i2cSclPin_,
                         hwConfig_.i2cFrequencyHz_);
  if (!rtcReady_) {
    Serial.println("clock: I2C init failed");
  } else {
    rtcReady_ = rtc.begin();
    if (!rtcReady_) {
      Serial.println("clock: RTC not found");
    } else {
      // sync internal RTC with external RTC
      const DateTime now = rtc.now();
      timeval timeval_;
      timezone timezone_;
      
      timeval_.tv_sec = now.unixtime();
      timeval_.tv_usec = 0;

      timezone_.tz_minuteswest = 0;
      timezone_.tz_dsttime = 0;

      settimeofday(&timeval_, &timezone_);

      rtc.writeSqwPinMode(DS3231_SquareWave1Hz);

      // pinMode(hwConfig_.rtcSqwPin_, INPUT_PULLUP);
      // const int sqwInterrupt = digitalPinToInterrupt(hwConfig_.rtcSqwPin_);
      // if (sqwInterrupt != NOT_AN_INTERRUPT) {
      //   activeInstance_ = this;
      //   attachInterrupt(sqwInterrupt, handleRtcSecondTickISR, FALLING);
      // }
    }
  }

  // now initialize the NTP synchronization
  ntpLastSynchronized_ = {};
  messageQueue_ = xQueueCreate(4, sizeof(Message));
  sntp_set_time_sync_notification_cb(sntpSyncCallback);
  configTzTime(config_.timezonePosix.c_str(), config_.ntpServer.c_str());
  // add the offset to current time
  timeval now_time;
  gettimeofday(&now_time, nullptr);
  now_time.tv_sec += config_.timeOffsetMinutes * 60;
  settimeofday(&now_time, nullptr);
  sntp_set_sync_interval(config_.ntpSyncIntervalMs);

  nextMinuteTimer_ = xTimerCreate("NextMinuteTimer",
                                  pdMS_TO_TICKS(5000),
                                  pdFALSE, // no auto reload
                                  nullptr,
                                  onNextMinuteTimer);

  _updateTask = std::thread(&ClockController::updateTask, this);

  // start the timer for the first time
  xTimerStart(nextMinuteTimer_, 0);

  return true;
}

void ClockController::updateTask() {
  struct tm now = {};
  Message msg;

  while(1) {
    xQueueReceive(messageQueue_, &msg, portMAX_DELAY);
    switch (msg) {
      case Message::NTPSynced: {
        getLocalTime(&now, 0);
        Serial.printf("clock: NTP synced: %02d:%02d:%02d\n", now.tm_hour, now.tm_min, now.tm_sec);
        //stop the next minute timer 
        xTimerStop(nextMinuteTimer_, 0);
        // send the NextMinute message to the queue manually
        msg = Message::NextMinute;
        xQueueSend(messageQueue_, &msg, 0);
        // adjust the clock with the offset
        timeval tv;
        gettimeofday(&tv, nullptr);
        tv.tv_sec += config_.timeOffsetMinutes * 60;
        settimeofday(&tv, nullptr);

        // update the RTC
        syncExternalRtc();
        // update the last synchronized time
        ntpLastSynchronized_ = now;
        break;
      }

      case Message::NewConfig: {
        // adjust new timezone
        if (new_config_.timezonePosix != config_.timezonePosix) {
          configTzTime(new_config_.timezonePosix.c_str(), config_.ntpServer.c_str());
        }
  
        // adjust new offset
        if (new_config_.timeOffsetMinutes != config_.timeOffsetMinutes) {
          timeval tv;
          gettimeofday(&tv, nullptr);
          tv.tv_sec -= config_.timeOffsetMinutes * 60;
          tv.tv_sec += new_config_.timeOffsetMinutes * 60;
          settimeofday(&tv, nullptr);
        }

        // update the RTC
        syncExternalRtc();

        config_ = new_config_;

        // send the NextMinute msg so it's sent to the coordinator
        msg = Message::NextMinute;
        xQueueSend(messageQueue_, &msg, 0);
        break;
      }

      case Message::NextMinute: {
        getLocalTime(&now, 0);
        // update the displayed value
        displayedHHMM_ = now.tm_hour * 100 + now.tm_min;
        // send the new time to the coordinator
        if (coordinator_) {
          coordinator_->postEvent({Coordinator::EventType::NewTime, displayedHHMM_});
        }
        // schedule the timer
        //Serial.println("clock: scheduling timer in " + String(60 - now.tm_sec) + " s");
        xTimerChangePeriod(nextMinuteTimer_, pdMS_TO_TICKS((60 - now.tm_sec) * 1000), 0);
        xTimerStart(nextMinuteTimer_, 0);
        break;
      }
    }
  }
}

void ClockController::sntpSyncCallback(struct timeval* tv) {
  const Message msg = Message::NTPSynced;
  xQueueSend(activeInstance_->messageQueue_, &msg, 0);
}

void ClockController::onNextMinuteTimer(TimerHandle_t xTimer) {
  Message msg = Message::NextMinute;
  xQueueSend(activeInstance_->messageQueue_, &msg, 0);
}

void ClockController::syncExternalRtc() {
  struct tm now = {};
  if (!rtcReady_) {
    return;
  }
  // getLocalTime will get the time with the local timezone applied
  if (!getLocalTime(&now, 0)) {
    return;
  }
  // convert the local time to UTC
  time_t utcEpoch = mktime(&now);
  // convert back to the struct tm representation
  now = *gmtime(&utcEpoch);

  const DateTime ntpNow(now.tm_year + 1900,
                        now.tm_mon + 1,
                        now.tm_mday,
                        now.tm_hour,
                        now.tm_min,
                        now.tm_sec);

  Serial.println("clock: RTC sync to " + String(ntpNow.timestamp()));
  rtc.adjust(ntpNow);
}

/******* Getters ******* */
bool ClockController::isReady() const {
  return true;
}

bool ClockController::externalRTCReady() const {
  return rtcReady_;
}

struct tm ClockController::ntpLastSynchronized() const {
  return ntpLastSynchronized_;
}

String ClockController::timezonePosix() const {
  return config_.timezonePosix;
}

int16_t ClockController::timeOffsetMinutes() const {
  return config_.timeOffsetMinutes;
}


/******* Setters ******* */
void ClockController::applyTimeConfig(const String& timezonePosix, int16_t timeOffsetMinutes) {
  Message msg = Message::NewConfig;
  bool something_changed = false;
  Serial.print("clock: new config - timezone: " + timezonePosix + ", time offset: " + String(timeOffsetMinutes) + "\n");

  new_config_ = config_;

  if (timezonePosix != config_.timezonePosix) {
    Serial.println("clock: timezone: " + config_.timezonePosix + " -> " + timezonePosix);
    new_config_.timezonePosix = timezonePosix;
    something_changed = true;
  }
  if (timeOffsetMinutes != config_.timeOffsetMinutes) {
    Serial.println("clock: offset: " + String(config_.timeOffsetMinutes) + " -> " + String(timeOffsetMinutes));
    new_config_.timeOffsetMinutes = timeOffsetMinutes;
    // the task will update the time
    something_changed = true;
  }

  // signal that we should update the time
  if (something_changed) {
    xQueueSend(messageQueue_, &msg, 0);
  }
}

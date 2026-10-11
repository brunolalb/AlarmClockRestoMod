#include "AlarmController.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <time.h>

const char* AlarmController::ALARM_FILE = "/alarm_config.json";
AlarmController* AlarmController::activeInstance_ = nullptr;

AlarmController::AlarmController(SdController* sdController)
    : sdController_(sdController) {}

bool AlarmController::initialize(Coordinator* coordinator) {
  coordinator_ = coordinator;
  activeInstance_ = this;
  
  loadAlarmSettings(alarmSettings_, alarmCount_);

  // schedule the next alarm
  messageQueue_ = xQueueCreate(4, sizeof(Message));
  nextAlarmTimer_ = xTimerCreate("NextAlarmTimer",
                                 pdMS_TO_TICKS(portMAX_DELAY), // will be set later based on the next alarm
                                 pdFALSE, // no auto reload
                                 nullptr,
                                 onNextAlarmTimer);

  _monitorTask = std::thread(&AlarmController::monitorTask, this);

  // send the stop message to the timer task to initialize the next alarm
  sendMessage(Message::Stop);

  return true; 
}

void AlarmController::sendMessage(Message message) {
  if (messageQueue_) {
    xQueueSend(messageQueue_, &message, 0);
  }
}

void AlarmController::monitorTask() {
  Message msg;
  while(1) {

    // check for messages in the queue
    xQueueReceive(messageQueue_, &msg, pdMS_TO_TICKS(portMAX_DELAY));
    switch (msg) {
      case Message::Alarm:
      Serial.println("alarm: msg Alarm");
        // tell the coordinator
        if (coordinator_) {
          // todo: pass relevant alarm information
          coordinator_->postEvent({Coordinator::EventType::AlarmTriggered, 0});
        }
        break;
      case Message::Sleep: {
        Serial.println("alarm: msg Sleep");
        // set timer to sleep period
        uint32_t sleep_time_ms = currentAlarm_.snooze_min * 60 * 1000;
        Serial.println("alarm: scheduling next alarm in " + String(sleep_time_ms / 1000) + " s");
        xTimerChangePeriod(nextAlarmTimer_, pdMS_TO_TICKS(sleep_time_ms), 0);
        xTimerStart(nextAlarmTimer_, 0);
        break;
      }
      case Message::Stop: {
        Serial.println("alarm: msg Stop");
        xTimerStop(nextAlarmTimer_, 0); // just in case
        // load next alarm
        currentAlarm_ = getNextAlarm();
        if (currentAlarm_.enabled) {
          uint32_t timeUntilNextAlarm = timeUntilAlarm_ms(currentAlarm_);
          if (timeUntilNextAlarm != UINT32_MAX) {
            Serial.println("alarm: scheduling next alarm in " + String(timeUntilNextAlarm / 1000) + " s");
            xTimerChangePeriod(nextAlarmTimer_, pdMS_TO_TICKS(timeUntilNextAlarm), 0);
            xTimerStart(nextAlarmTimer_, 0);
          }
        }
        break;
      }
    }

  }
}

bool AlarmController::isValidSettings(const AlarmSettings& settings) const {
  if (settings.time_hour > 23) {
    Serial.println("alarm: invalid hour: " + String(settings.time_hour));
    return false;
  }
  if (settings.time_min > 59) {
    Serial.println("alarm: invalid minute: " + String(settings.time_min));
    return false;
  }
  if (settings.soundPath.isEmpty()) {
    return false;
  }

  return true;
}

bool AlarmController::compareAlarmSettings(const AlarmSettings* settings1, uint8_t count1, const AlarmSettings* settings2, uint8_t count2) const {
  if (count1 != count2) {
    Serial.println("alarm: different alarm count: " + String(count1) + " vs " + String(count2));
    return false;
  }
  for (uint8_t i = 0; i < count1; ++i) {
    if (settings1[i].enabled != settings2[i].enabled) {
      Serial.println("alarm: different enabled (i=" + String(i) + ")");
      return false;
    }
    for (uint8_t j = 0; j < 7; ++j) {
      if (settings1[i].enabled_days[j] != settings2[i].enabled_days[j]) {
        Serial.println("alarm: different enabled day (i=" + String(i) + ", j=" + String(j) + ")");
        return false;
      }
    }
    if (settings1[i].time_hour != settings2[i].time_hour) {
      Serial.println("alarm: different hour (i=" + String(i) + ")");
      return false;
    }
    if (settings1[i].time_min != settings2[i].time_min) {
      Serial.println("alarm: different minute (i=" + String(i) + ")");
      return false;
    }
    if (settings1[i].snooze_min != settings2[i].snooze_min) {
      Serial.println("alarm: different snooze minute (i=" + String(i) + ")");
      return false;
    }
    if (settings1[i].sound_in_SD != settings2[i].sound_in_SD) {
      Serial.println("alarm: different sound_in_SD (i=" + String(i) + ")");
      return false;
    }
    if (settings1[i].soundPath != settings2[i].soundPath) {
      Serial.println("alarm: different soundPath (i=" + String(i) + ")");
      return false;
    }
    if (settings1[i].soundStartAt != settings2[i].soundStartAt) {
      Serial.println("alarm: different soundStartAt (i=" + String(i) + ")");
      return false;
    }
    if (settings1[i].soundVolume != settings2[i].soundVolume) {
      Serial.println("alarm: different soundVolume (i=" + String(i) + ")");
      return false;
    }
  }
  return true;
}

uint32_t AlarmController::timeUntilAlarm_ms(const AlarmSettings& alarm) const {
  if (!alarm.enabled) {
    return UINT32_MAX; // no next alarm
  }

  time_t now;
  if (time(&now) == static_cast<time_t>(-1)) {
    return UINT32_MAX;
  }

  struct tm currentTime;
  if (localtime_r(&now, &currentTime) == nullptr) {
    return UINT32_MAX;
  }

  const uint8_t currentWeekday = (currentTime.tm_wday + 6) % 7;
  time_t soonestAlarm = static_cast<time_t>(-1);

  // Include offset 7 so an alarm enabled only for today's weekday can recur next week.
  for (uint8_t dayOffset = 0; dayOffset <= 7; ++dayOffset) {
    const uint8_t weekday = (currentWeekday + dayOffset) % 7;
    if (!alarm.enabled_days[weekday]) {
      continue;
    }

    struct tm candidate = currentTime;
    candidate.tm_mday += dayOffset;
    candidate.tm_hour = alarm.time_hour;
    candidate.tm_min = alarm.time_min;
    candidate.tm_sec = 0;
    candidate.tm_isdst = -1;

    const time_t candidateTime = mktime(&candidate);
    if (candidateTime > now &&
        (soonestAlarm == static_cast<time_t>(-1) || candidateTime < soonestAlarm)) {
      soonestAlarm = candidateTime;
    }
  }

  if (soonestAlarm == static_cast<time_t>(-1)) {
    return UINT32_MAX;
  }

  return static_cast<uint32_t>(difftime(soonestAlarm, now) * 1000.0);
}

AlarmController::AlarmSettings AlarmController::getNextAlarm() const {
  AlarmSettings nextAlarm{};
  nextAlarm.enabled = false;
  uint32_t shortestDelay = UINT32_MAX;

  for (uint8_t i = 0; i < alarmCount_; ++i) {
    const uint32_t delay = timeUntilAlarm_ms(alarmSettings_[i]);
    if (delay < shortestDelay) {
      shortestDelay = delay;
      nextAlarm = alarmSettings_[i];
    }
  }

  return nextAlarm;
}

/*********************************/
/********* File Handling *********/
/*********************************/

/******* LittleFS ********/

bool AlarmController::LittleFS_ensureInit() {
  return LittleFS.begin(false);
}

bool AlarmController::LittleFS_read(AlarmSettings* settings, uint8_t& count) {
  Serial.println("alarms: LittleFS_read");

  if (!LittleFS_ensureInit()) {
    Serial.println("alarms: failed to mount LittleFS");
    return false;
  }

  if (!LittleFS.exists(ALARM_FILE)) {
    Serial.println("alarms: no alarm file on LittleFS");
    return false;
  }

  File file = LittleFS.open(ALARM_FILE, FILE_READ);
  if (!file) {
    Serial.println("alarms: failed to open file on LittleFS");
    return false;
  }

  return parseAlarmSettingsDocument(file, settings, count);
}

bool AlarmController::LittleFS_save(const AlarmSettings* settings, uint8_t count) {
  Serial.println("alarms: LittleFS_save");
  if (!LittleFS_ensureInit()) {
    Serial.println("alarms: failed to mount LittleFS");
    return false;
  }

  if (LittleFS.exists(ALARM_FILE) && !LittleFS.remove(ALARM_FILE)) {
    Serial.println("alarms: failed to remove LittleFS");
    return false;
  }

  File file = LittleFS.open(ALARM_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("alarms: failed to open LittleFS");
    return false;
  }

  const bool ok = saveJSON(settings, count, file);

  file.close();
  return ok;
}

/******* SD Card ********/

bool AlarmController::SDCard_read(AlarmSettings* settings, uint8_t& count) {
  if (!sdController_ || !sdController_->isReady()) {
    Serial.println("alarm: no SD");
    return false;
  }

  if (!sdController_->exists(ALARM_FILE)) {
    Serial.println("alarm: no SD alarm file");
    return false;
  }

  File file = sdController_->open(ALARM_FILE, FILE_READ);
  if (!file) {
    Serial.println("alarm: failed to open file on SD");
    return false;
  }

  return parseAlarmSettingsDocument(file, settings, count);
}

bool AlarmController::SDCard_save(const AlarmSettings* settings, uint8_t count) {
  if (!sdController_ || !sdController_->isReady()) {
    Serial.println("alarms: no SD");
    return false;
  }

  if (sdController_->exists(ALARM_FILE) && !sdController_->remove(ALARM_FILE)) {
    Serial.println("alarms: failed to remove SD");
    return false;
  }

  File file = sdController_->open(ALARM_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("alarms: failed to open SD");
    return false;
  }

  const bool ok = saveJSON(settings, count, file);

  file.close();
  return ok;
}

/******* Generic ********/

bool AlarmController::loadAlarmSettings(const AlarmSettings* settings, uint8_t count) {
  AlarmSettings alarms_SD[MAX_ALARMS];
  uint8_t alarms_in_SD_count = 0;
  AlarmSettings alarms_LittleFS[MAX_ALARMS];
  uint8_t alarms_in_LittleFS_count = 0;

  bool alarms_in_SD = SDCard_read(alarms_SD, alarms_in_SD_count);
  bool alarms_in_LittleFS = LittleFS_read(alarms_LittleFS, alarms_in_LittleFS_count);

  bool all_good = false;

  if (!alarms_in_SD && !alarms_in_LittleFS) {
    Serial.println("alarms: no valid cfg, creating clean");
    all_good = saveAlarmSettings(alarms_SD, 0);
  } else {
    if (alarms_in_SD) {
      memcpy(alarmSettings_, alarms_SD, sizeof(alarms_SD));
      alarmCount_ = alarms_in_SD_count;
      // to prevent wear on the LittleFS, only write if necessary
      if (!alarms_in_LittleFS || !compareAlarmSettings(alarms_SD, alarms_in_SD_count, alarms_LittleFS, alarms_in_LittleFS_count)) {
        // if the config on the littleFS is different, save it
        all_good = LittleFS_save(alarms_SD, alarms_in_SD_count);
      } else {
        all_good = true;
      }
    } else {
      memcpy(alarmSettings_, alarms_LittleFS, sizeof(alarms_LittleFS));
      alarmCount_ = alarms_in_LittleFS_count;
      all_good = SDCard_save(alarms_LittleFS, alarms_in_LittleFS_count);
    }
  }

  if (all_good) {
    currentAlarm_ = getNextAlarm();
  } else {
    currentAlarm_.enabled = false;
  }

  return all_good; // should not reach here
}

bool AlarmController::saveAlarmSettings(const AlarmSettings* settings, uint8_t count) {
  const bool littleFsOk = LittleFS_save(settings, count);
  const bool sdOk = SDCard_save(settings, count);
  if (littleFsOk || sdOk) {
    memcpy(alarmSettings_, settings, count * sizeof(AlarmSettings));
    alarmCount_ = count;
  }

  // make the monitor aware that the alarm settings have changed
  sendMessage(Message::Stop);

  return littleFsOk || sdOk; //save in at least one media
}

bool AlarmController::parseAlarmSettingsDocument( File file,
                                                  AlarmSettings* settings,
                                                  uint8_t& count) const {
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    Serial.println("alarm: failed to deserialize");
    return false;
  }

  JsonVariantConst root = doc.as<JsonVariantConst>();

  JsonArrayConst alarms = root["alarms"].as<JsonArrayConst>();
  if (alarms.isNull() || alarms.size() > MAX_ALARMS) {
    Serial.println("alarm: invalid or too many alarms in JSON: " + (alarms.isNull() ? "null" : String(alarms.size())));
    return false;
  }

  uint8_t parsedCount = 0;
  uint8_t savedCount = 0;
  for (JsonVariantConst alarmVariant : alarms) {
    if (!parseJSON(alarmVariant, settings[savedCount])) {
      Serial.println("alarm: failed to parse alarm at index " + String(parsedCount));
    } else {
      savedCount++;
    }
    parsedCount++;
  }

  count = savedCount;
  return true;
}

/*********************************/
/********* JSON *********/
/*********************************/

bool AlarmController::parseJSON(JsonVariantConst alarmVariant, AlarmSettings& outSettings) const {
  // read one alarm settings from the json, validate and save the data to outSettings
  JsonObjectConst alarmObj = alarmVariant.as<JsonObjectConst>();
  if (alarmObj.isNull()) {
    Serial.println("alarm: alarm object is null");
    return false;
  }

  Serial.println("alarm: parsing JSON: " + String(alarmVariant.as<String>()));

  AlarmSettings newAlarm;
  newAlarm.enabled = alarmObj["enabled"] | false;
  for (uint8_t i = 0; i < 7; ++i) {
    newAlarm.enabled_days[i] = alarmObj["enabled_days"][i];
  }
  newAlarm.time_hour = alarmObj["time_hour"] | 0;
  newAlarm.time_min = alarmObj["time_min"] | 0;
  newAlarm.snooze_min = alarmObj["snooze_min"] | 0;
  newAlarm.sound_in_SD = alarmObj["sound_in_SD"] | false;
  newAlarm.soundPath = alarmObj["soundPath"] | "";
  newAlarm.soundStartAt = alarmObj["soundStartAt"] | 0;
  newAlarm.soundVolume = alarmObj["soundVolume"] | 5;

  // make sure the settings are valid
  if (!isValidSettings(newAlarm)) {
    return false;
  }

  outSettings = newAlarm;
  return true;
}

JsonDocument AlarmController::makeJSON(const AlarmSettings* settings, uint8_t count) {
  JsonDocument doc;
  JsonArray alarms = doc["alarms"].to<JsonArray>();
  for (uint8_t i = 0; i < count; ++i) {
    appendToJSON(settings[i], alarms);
  }
  return doc;
}

void AlarmController::appendToJSON(const AlarmSettings& settings, JsonArray& alarms) const {
  JsonObject alarmObj = alarms.add<JsonObject>();

  alarmObj["enabled"] = settings.enabled;
  for (uint8_t i = 0; i < 7; ++i) {
    alarmObj["enabled_days"][i] = settings.enabled_days[i];
  }
  alarmObj["time_hour"] = settings.time_hour;
  alarmObj["time_min"] = settings.time_min;
  alarmObj["snooze_min"] = settings.snooze_min;
  alarmObj["sound_in_SD"] = settings.sound_in_SD;
  alarmObj["soundPath"] = settings.soundPath;
  alarmObj["soundStartAt"] = settings.soundStartAt;
  alarmObj["soundVolume"] = settings.soundVolume;

}

bool AlarmController::saveJSON(const AlarmSettings* settings, uint8_t count, File file) {
  JsonDocument doc = makeJSON(settings, count);

  Serial.print("alarms: saving JSON: ");
  serializeJson(doc, Serial);

  return serializeJson(doc, file) > 0;
}

/*********************************/
/********* Alarms *********/
/*********************************/

void AlarmController::onNextAlarmTimer(TimerHandle_t xTimer) {
  activeInstance_->sendMessage(Message::Alarm);
}


/*********************************/
/********* API *********/
/*********************************/

uint8_t AlarmController::alarmCount() const {
  return alarmCount_;
}

/*********************************/
/********* API - Webserver *******/
/*********************************/

JsonDocument AlarmController::handleGetAlarmConfig() {
  JsonDocument doc;
  doc["maxAlarms"] = MAX_ALARMS;
  JsonArray alarms = doc["alarms"].to<JsonArray>();
  for (uint8_t i = 0; i < alarmCount_; ++i) {
    appendToJSON(alarmSettings_[i], alarms);
  }

  return doc;
}

JsonDocument AlarmController::handleSaveAlarmConfig(JsonDocument doc) {
  JsonDocument response;
  response["ok"] = false;
  response["error"] = "";
  response["count"] = 0;

  JsonArray alarms = doc["alarms"].as<JsonArray>();
  if (alarms.isNull() || alarms.size() > MAX_ALARMS) {
    response["error"] = "Invalid alarms array";
    return response;
  }

  // parse each alarm and save in new_alarms
  AlarmSettings new_alarms[MAX_ALARMS];
  uint8_t nextCount = 0;
  for (JsonVariant alarmVariant : alarms) {
    if (!parseJSON(alarmVariant, new_alarms[nextCount])) {
      response["error"] = "Invalid alarm entry (" + String(nextCount) + ")";
      return response;
    }
    nextCount++;
  }

  // save the new alarms
  if (!saveAlarmSettings(new_alarms, nextCount)) {
    response["error"] = "Failed to save alarms";
    return response;
  }

  // send the answer
  response["ok"] = true;
  response["count"] = alarmCount_;

  return response;
}

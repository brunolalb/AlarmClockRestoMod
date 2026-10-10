#include "AlarmController.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>

const char* AlarmController::ALARM_FILE = "/alarm_config.json";

AlarmController::AlarmController(SdController* sdController)
    : sdController_(sdController) {}

bool AlarmController::initialize() {
  
  loadAlarmSettings(alarmSettings_, alarmCount_);

  // todo: schedule the next alarm

  return true; 
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

/*********************************/
/********* File Handling *********/
/*********************************/

/******* LittleFS ********/

bool AlarmController::LittleFS_ensureInit() {
  return LittleFS.begin(false);
}

bool AlarmController::LittleFS_read(AlarmSettings* settings, uint8_t& count) {
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

  if (!alarms_in_SD && !alarms_in_LittleFS) {
    Serial.println("alarms: no valid cfg, creating clean");
    return saveAlarmSettings(alarms_SD, 0);
  } else {
    if (alarms_in_SD) {
      memcpy(alarmSettings_, alarms_SD, sizeof(alarms_SD));
      alarmCount_ = alarms_in_SD_count;
      // to prevent wear on the LittleFS, only write if necessary
      if (!alarms_in_LittleFS || !compareAlarmSettings(alarms_SD, alarms_in_SD_count, alarms_LittleFS, alarms_in_LittleFS_count)) {
        // if the config on the littleFS is different, save it
        return LittleFS_save(alarms_SD, alarms_in_SD_count);
      }
      return true;
    } else {
      memcpy(alarmSettings_, alarms_LittleFS, sizeof(alarms_LittleFS));
      alarmCount_ = alarms_in_LittleFS_count;
      return SDCard_save(alarms_LittleFS, alarms_in_LittleFS_count);
    }
  }
  return false; // should not reach here
}

bool AlarmController::saveAlarmSettings(const AlarmSettings* settings, uint8_t count) {
  const bool littleFsOk = LittleFS_save(settings, count);
  const bool sdOk = SDCard_save(settings, count);
  if (littleFsOk || sdOk) {
    memcpy(alarmSettings_, settings, count * sizeof(AlarmSettings));
    alarmCount_ = count;
  }
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

  JsonArrayConst days = alarmObj["enabled_days"].as<JsonArrayConst>();
  if (days.isNull() || days.size() != 7) {
    Serial.println("alarm: invalid or missing enabled_days");
    return false;
  }
  bool enabled_days[7];
  for (uint8_t i = 0; i < 7; ++i) {
    enabled_days[i] = days[i] | false;
  }

  AlarmSettings newAlarm;
  newAlarm.enabled = alarmObj["enabled"] | false;
  memcpy(newAlarm.enabled_days, enabled_days, 7 * sizeof(bool));
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

  JsonArray enabled_days = alarmObj["enabled_days"].to<JsonArray>();
  for (uint8_t i = 0; i < 7; ++i) {
    enabled_days.add(settings.enabled_days[i]);
  }

  alarmObj["enabled"] = settings.enabled;
  alarmObj["enabled_days"] = enabled_days;
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
/********* API *********/
/*********************************/

uint8_t AlarmController::alarmCount() const {
  return alarmCount_;
}




// todo: fix all this

void AlarmController::sendAlarmConfigJson(WebServer& webServer) {
  StaticJsonDocument<4096> doc;
  doc["maxAlarms"] = MAX_ALARMS;
  JsonArray alarms = doc.createNestedArray("alarms");
  for (uint8_t i = 0; i < alarmCount_; ++i) {
    appendToJSON(alarmSettings_[i], alarms);
  }

  String payload;
  serializeJson(doc, payload);
  webServer.send(200, "application/json", payload);
}

void AlarmController::handleGetAlarmConfig(WebServer& webServer) {
  sendAlarmConfigJson(webServer);
}

void AlarmController::handleSaveAlarmConfig(WebServer& webServer) {
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "application/json", "{\"ok\":false,\"error\":\"Method not allowed\"}");
    return;
  }

  if (!webServer.hasArg("plain")) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing JSON body\"}");
    return;
  }

  StaticJsonDocument<4096> doc;
  const DeserializationError err = deserializeJson(doc, webServer.arg("plain"));
  if (err) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
    return;
  }

  JsonArray alarms = doc["alarms"].as<JsonArray>();
  if (alarms.isNull() || alarms.size() > MAX_ALARMS) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid alarms array\"}");
    return;
  }

  AlarmSettings next[MAX_ALARMS];
  uint8_t nextCount = 0;
  for (JsonVariant alarmVariant : alarms) {
    if (!parseJSON(alarmVariant, next[nextCount])) {
      webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid alarm entry\"}");
      return;
    }
    nextCount++;
  }

  if (!saveAlarmSettings(next, nextCount)) {
    webServer.send(500, "application/json", "{\"ok\":false,\"error\":\"Failed to save alarm\"}");
    return;
  }

  alarmCount_ = nextCount;
  for (uint8_t i = 0; i < alarmCount_; ++i) {
    alarmSettings_[i] = next[i];
  }

  StaticJsonDocument<96> response;
  response["ok"] = true;
  response["count"] = alarmCount_;
  String payload;
  serializeJson(response, payload);
  webServer.send(200, "application/json", payload);
}

void AlarmController::handleListMusicFiles(WebServer& webServer) {
  DynamicJsonDocument doc(1024);
  JsonArray files = doc.to<JsonArray>();

  if (sdController_ && sdController_->isReady()) {
    File root = sdController_->open("/", FILE_READ);
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        if (!file.isDirectory()) {
          String name = String(file.name());
          String lower = name;
          lower.toLowerCase();
          if (lower.endsWith(".mp3") || lower.endsWith(".wav") || lower.endsWith(".ogg")) {
            files.add(name);
          }
        }
        file.close();
        file = root.openNextFile();
      }
      root.close();
    }
  }

  String payload;
  serializeJson(files, payload);
  webServer.send(200, "application/json", payload);
}

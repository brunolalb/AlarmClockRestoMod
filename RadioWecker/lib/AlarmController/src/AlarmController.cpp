#include "AlarmController.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>

const char* AlarmController::ALARM_FILE = "/alarm_config.json";

AlarmController::AlarmController(SdController& sdController)
    : sdController_(sdController) {}

bool AlarmController::initialize() {
  initialized_ = false;

  if (!loadAlarmSettings(alarmSettings_, alarmCount_)) {
    setDefaultAlarmSettings(alarmSettings_[0]);
    alarmCount_ = 1;
    saveCurrentAlarmSettings();
  }

  initialized_ = true;

  return true;
}

bool AlarmController::isInitialized() const {
  return initialized_;
}

uint8_t AlarmController::alarmCount() const {
  return alarmCount_;
}

void AlarmController::setDefaultAlarmSettings(AlarmSettings& settings) {
  settings.enabled = false;
  settings.time = "07:00";
  for (uint8_t i = 0; i < 7; ++i) {
    settings.days[i] = (i >= 1 && i <= 5);
  }
  settings.snoozeMin = 10;
  settings.soundType = "buzzer";
  settings.musicPath = "";
}

bool AlarmController::isValidTimeString(const String& timeValue) const {
  if (timeValue.length() != 5 || timeValue.charAt(2) != ':') {
    return false;
  }

  const char h0 = timeValue.charAt(0);
  const char h1 = timeValue.charAt(1);
  const char m0 = timeValue.charAt(3);
  const char m1 = timeValue.charAt(4);
  if (!isDigit(h0) || !isDigit(h1) || !isDigit(m0) || !isDigit(m1)) {
    return false;
  }

  const uint8_t hours = static_cast<uint8_t>((h0 - '0') * 10 + (h1 - '0'));
  const uint8_t minutes = static_cast<uint8_t>((m0 - '0') * 10 + (m1 - '0'));
  return hours < 24 && minutes < 60;
}

bool AlarmController::parseAlarmFromJson(JsonVariantConst alarmVariant, AlarmSettings& outSettings) const {
  // read one alarm settings from the json, validate and save the data to outSettings
  JsonObjectConst alarmObj = alarmVariant.as<JsonObjectConst>();
  if (alarmObj.isNull()) {
    return false;
  }

  const bool enabled = alarmObj["enabled"] | false;
  const String timeValue = alarmObj["time"] | "";
  const String soundType = alarmObj["soundType"] | "";
  const String musicPath = alarmObj["musicPath"] | "";
  const uint16_t snoozeMin = alarmObj["snoozeMin"] | 0;
  JsonArrayConst days = alarmObj["days"].as<JsonArrayConst>();

  // make sure the settings are valid
  if (!isValidTimeString(timeValue) ||
      !(soundType == "buzzer" || soundType == "music") ||
      snoozeMin < 1 || snoozeMin > 120 ||
      days.isNull() || days.size() != 7) {
    return false;
  }

  // at least one day must be selected
  bool hasAtLeastOneDay = false;
  for (uint8_t i = 0; i < 7; ++i) {
    outSettings.days[i] = days[i] | false;
    hasAtLeastOneDay = hasAtLeastOneDay || outSettings.days[i];
  }
  if (!hasAtLeastOneDay) {
    return false;
  }

  // if the sound type is music, a music path must be provided
  if (soundType == "music" && musicPath.isEmpty()) {
    return false;
  }

  outSettings.enabled = enabled;
  outSettings.time = timeValue;
  outSettings.snoozeMin = snoozeMin;
  outSettings.soundType = soundType;
  outSettings.musicPath = musicPath;
  return true;
}

bool AlarmController::parseAlarmSettingsDocument( File file,
                                                  AlarmSettings* settings,
                                                  uint8_t& count) const {
  StaticJsonDocument<4096> doc;
  const DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    return false;
  }

  JsonVariantConst root = doc.as<JsonVariantConst>();

  JsonArrayConst alarms = root["alarms"].as<JsonArrayConst>();
  if (alarms.isNull() || alarms.size() > MAX_ALARMS) {
    return false;
  }

  uint8_t parsedCount = 0;
  for (JsonVariantConst alarmVariant : alarms) {
    if (!parseAlarmFromJson(alarmVariant, settings[parsedCount])) {
      return false;
    }
    parsedCount++;
  }

  count = parsedCount;
  return true;
}

void AlarmController::appendAlarmToJsonArray(const AlarmSettings& settings, JsonArray& alarms) const {
  JsonObject alarmObj = alarms.createNestedObject();
  alarmObj["enabled"] = settings.enabled;
  alarmObj["time"] = settings.time;
  JsonArray days = alarmObj.createNestedArray("days");
  for (uint8_t i = 0; i < 7; ++i) {
    days.add(settings.days[i]);
  }
  alarmObj["snoozeMin"] = settings.snoozeMin;
  alarmObj["soundType"] = settings.soundType;
  alarmObj["musicPath"] = settings.musicPath;
}

StaticJsonDocument<4096> AlarmController::makeJSON(const AlarmSettings* settings, uint8_t count) {
  StaticJsonDocument<4096> doc;
  JsonArray alarms = doc.createNestedArray("alarms");
  for (uint8_t i = 0; i < count; ++i) {
    appendAlarmToJsonArray(settings[i], alarms);
  }
  return doc;
}

bool AlarmController::saveAlarmSettingsToSd(const AlarmSettings* settings, uint8_t count) {
  if (!sdController_.isReady()) {
    return false;
  }

  if (sdController_.exists(ALARM_FILE) && !sdController_.remove(ALARM_FILE)) {
    return false;
  }

  StaticJsonDocument<4096> doc = makeJSON(settings, count);

  File file = sdController_.open(ALARM_FILE, FILE_WRITE);
  if (!file) {
    return false;
  }

  const bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

bool AlarmController::saveAlarmSettingsToLittleFs(const AlarmSettings* settings, uint8_t count) {
  if (!LittleFS.begin(true)) {
    return false;
  }

  if (LittleFS.exists(ALARM_FILE) && !LittleFS.remove(ALARM_FILE)) {
    return false;
  }

  StaticJsonDocument<4096> doc = makeJSON(settings, count);

  File file = LittleFS.open(ALARM_FILE, FILE_WRITE);
  if (!file) {
    return false;
  }

  const bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

bool AlarmController::saveAlarmSettingsToAll(const AlarmSettings* settings, uint8_t count) {
  const bool littleFsOk = saveAlarmSettingsToLittleFs(settings, count);
  const bool sdOk = saveAlarmSettingsToSd(settings, count);
  return littleFsOk || sdOk; //save in at least one media
}

bool AlarmController::loadAlarmSettingsFromSd(AlarmSettings* settings, uint8_t& count) {
  if (!sdController_.isReady()) {
    return false;
  }

  if (!sdController_.exists(ALARM_FILE)) {
    return false;
  }

  File file = sdController_.open(ALARM_FILE, FILE_READ);
  if (!file) {
    return false;
  }

  return parseAlarmSettingsDocument(file, settings, count);
}

bool AlarmController::loadAlarmSettingsFromLittleFs(AlarmSettings* settings, uint8_t& count) {
  if (!LittleFS.begin(true)) {
    return false;
  }

  if (!LittleFS.exists(ALARM_FILE)) {
    return false;
  }

  File file = LittleFS.open(ALARM_FILE, FILE_READ);
  if (!file) {
    return false;
  }

  return parseAlarmSettingsDocument(file, settings, count);
}

bool AlarmController::loadAlarmSettings(AlarmSettings* settings, uint8_t& count) {
  if (loadAlarmSettingsFromSd(settings, count)) {
    saveAlarmSettingsToLittleFs(settings, count);
    return true;
  }

  if (loadAlarmSettingsFromLittleFs(settings, count)) {
    saveAlarmSettingsToSd(settings, count);
    return true;
  }

  Serial.println("alarms: failed to load settings from both SD and LittleFS");
  return false;
}

bool AlarmController::saveCurrentAlarmSettings() {
  return saveAlarmSettingsToAll(alarmSettings_, alarmCount_);
}

void AlarmController::sendAlarmConfigJson(WebServer& webServer) {
  StaticJsonDocument<4096> doc;
  doc["maxAlarms"] = MAX_ALARMS;
  JsonArray alarms = doc.createNestedArray("alarms");
  for (uint8_t i = 0; i < alarmCount_; ++i) {
    appendAlarmToJsonArray(alarmSettings_[i], alarms);
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
    if (!parseAlarmFromJson(alarmVariant, next[nextCount])) {
      webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid alarm entry\"}");
      return;
    }
    nextCount++;
  }

  if (!saveAlarmSettingsToAll(next, nextCount)) {
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

  if (sdController_.isReady()) {
    File root = sdController_.open("/", FILE_READ);
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

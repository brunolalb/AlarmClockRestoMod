#include "GeneralConfigController.h"

#include <ArduinoJson.h>
#include <ClockController.h>
#include <DisplayManager.h>
#include <LittleFS.h>
#include <SD.h>
#include <SdController.h>
#include <WebServer.h>

GeneralConfigController::GeneralConfigController(ClockController& clockController,
                                                 SdController& sdController,
                                                 DisplayManager& displayManager,
                                                 const char* defaultTimezonePosix,
                                                 int16_t defaultTimeOffsetMinutes,
                                                 uint8_t defaultBrightness)
    : clockController_(clockController),
      sdController_(sdController),
      displayManager_(displayManager),
      timezonePosix_(defaultTimezonePosix),
      timeOffsetMinutes_(defaultTimeOffsetMinutes),
      brightness_(defaultBrightness),
      defaultTimezonePosix_(defaultTimezonePosix),
      defaultTimeOffsetMinutes_(defaultTimeOffsetMinutes),
      defaultBrightness_(defaultBrightness) {}

bool GeneralConfigController::ensureInternalFsMounted() {
  if (internalFsMounted_) {
    return true;
  }

  internalFsMounted_ = LittleFS.begin(true);
  return internalFsMounted_;
}

bool GeneralConfigController::isValidConfig(const String& timezone, int offsetMinutes, int brightness) const {
  return timezone.length() > 0 && offsetMinutes >= -720 && offsetMinutes <= 840 && brightness >= 0 && brightness <= 7;
}

bool GeneralConfigController::readConfigFromJsonFile(const String& jsonPayload, ConfigData& outConfig) const {
  StaticJsonDocument<384> doc;
  const DeserializationError err = deserializeJson(doc, jsonPayload);
  if (err) {
    return false;
  }

  const String timezone = doc["timezone"] | defaultTimezonePosix_;
  const int offsetMinutes = doc["timeOffsetMinutes"] | defaultTimeOffsetMinutes_;
  const int brightness = doc["brightness"] | defaultBrightness_;

  if (!isValidConfig(timezone, offsetMinutes, brightness)) {
    return false;
  }

  outConfig.timezonePosix = timezone;
  outConfig.timeOffsetMinutes = static_cast<int16_t>(offsetMinutes);
  outConfig.brightness = static_cast<uint8_t>(brightness);
  return true;
}

bool GeneralConfigController::readFromLittleFs(ConfigData& outConfig) {
  if (!ensureInternalFsMounted()) {
    return false;
  }

  if (!LittleFS.exists(GENERAL_CONFIG_FILE)) {
    return false;
  }

  File file = LittleFS.open(GENERAL_CONFIG_FILE, FILE_READ);
  if (!file) {
    return false;
  }

  const String payload = file.readString();
  file.close();
  return readConfigFromJsonFile(payload, outConfig);
}

bool GeneralConfigController::readFromSdCard(ConfigData& outConfig) {
  if (!sdController_.isReady()) {
    return false;
  }

  if (!SD.exists(GENERAL_CONFIG_FILE)) {
    return false;
  }

  File file = SD.open(GENERAL_CONFIG_FILE, FILE_READ);
  if (!file) {
    return false;
  }

  const String payload = file.readString();
  file.close();
  return readConfigFromJsonFile(payload, outConfig);
}

void GeneralConfigController::loadFromStorage() {
  timezonePosix_ = defaultTimezonePosix_;
  timeOffsetMinutes_ = defaultTimeOffsetMinutes_;
  brightness_ = defaultBrightness_;

  ConfigData config;
  if (readFromSdCard(config)) {
    timezonePosix_ = config.timezonePosix;
    timeOffsetMinutes_ = config.timeOffsetMinutes;
    brightness_ = config.brightness;
    saveToLittleFs();
    return;
  }

  if (readFromLittleFs(config)) {
    timezonePosix_ = config.timezonePosix;
    timeOffsetMinutes_ = config.timeOffsetMinutes;
    brightness_ = config.brightness;
    saveToSdCard();
    return;
  }

  saveToAllStorages();
}

void GeneralConfigController::applyToClock() {
  clockController_.applyTimeConfig(timezonePosix_, timeOffsetMinutes_);
}

void GeneralConfigController::applyToDisplay() {
  displayManager_.setBrightness(brightness_);
}

bool GeneralConfigController::saveToLittleFs() {
  if (!ensureInternalFsMounted()) {
    return false;
  }

  if (LittleFS.exists(GENERAL_CONFIG_FILE) && !LittleFS.remove(GENERAL_CONFIG_FILE)) {
    return false;
  }

  StaticJsonDocument<384> doc;
  doc["timezone"] = timezonePosix_;
  doc["timeOffsetMinutes"] = timeOffsetMinutes_;
  doc["brightness"] = brightness_;

  File file = LittleFS.open(GENERAL_CONFIG_FILE, FILE_WRITE);
  if (!file) {
    return false;
  }

  const bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

bool GeneralConfigController::saveToSdCard() {
  if (!sdController_.isReady()) {
    return false;
  }

  if (SD.exists(GENERAL_CONFIG_FILE) && !SD.remove(GENERAL_CONFIG_FILE)) {
    return false;
  }

  StaticJsonDocument<384> doc;
  doc["timezone"] = timezonePosix_;
  doc["timeOffsetMinutes"] = timeOffsetMinutes_;
  doc["brightness"] = brightness_;

  File file = SD.open(GENERAL_CONFIG_FILE, FILE_WRITE);
  if (!file) {
    return false;
  }

  const bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

bool GeneralConfigController::saveToAllStorages() {
  const bool littleFsOk = saveToLittleFs();
  const bool sdOk = sdController_.isReady() ? saveToSdCard() : true;
  return littleFsOk && sdOk;
}

void GeneralConfigController::handleGetConfig(WebServer& webServer) {
  StaticJsonDocument<384> doc;
  doc["timezone"] = timezonePosix_;
  doc["timeOffsetMinutes"] = timeOffsetMinutes_;
  doc["brightness"] = brightness_;

  String payload;
  serializeJson(doc, payload);
  webServer.send(200, "application/json", payload);
}

void GeneralConfigController::handleSaveConfig(WebServer& webServer) {
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "application/json", "{\"ok\":false,\"error\":\"Method not allowed\"}");
    return;
  }

  if (!webServer.hasArg("plain")) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing JSON body\"}");
    return;
  }

  StaticJsonDocument<384> doc;
  const DeserializationError err = deserializeJson(doc, webServer.arg("plain"));
  if (err) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
    return;
  }

  const String timezone = doc["timezone"] | "";
  const int offsetMinutes = doc["timeOffsetMinutes"] | 0;
  const int brightness = doc["brightness"] | -1;

  if (!isValidConfig(timezone, offsetMinutes, brightness)) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid configuration\"}");
    return;
  }

  timezonePosix_ = timezone;
  timeOffsetMinutes_ = static_cast<int16_t>(offsetMinutes);
  brightness_ = static_cast<uint8_t>(brightness);

  applyToClock();
  applyToDisplay();

  if (!saveToAllStorages()) {
    webServer.send(500, "application/json", "{\"ok\":false,\"error\":\"Failed to persist configuration\"}");
    return;
  }

  StaticJsonDocument<96> response;
  response["ok"] = true;
  String payload;
  serializeJson(response, payload);
  webServer.send(200, "application/json", payload);
}

uint8_t GeneralConfigController::brightness() const {
  return brightness_;
}

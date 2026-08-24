#include "GeneralConfigController.h"

#include <ArduinoJson.h>
#include <DisplayManager.h>
#include <LittleFS.h>
#include <SdController.h>
#include <WebServer.h>

namespace {
int fromHexNibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

String encryptHexXor(const String& plainText, const String& key) {
  if (key.length() == 0) {
    return "";
  }

  String out;
  out.reserve(plainText.length() * 2);
  static const char* kHex = "0123456789ABCDEF";

  for (size_t i = 0; i < plainText.length(); ++i) {
    const uint8_t x = static_cast<uint8_t>(plainText.charAt(i)) ^ static_cast<uint8_t>(key.charAt(i % key.length()));
    out += kHex[(x >> 4) & 0x0F];
    out += kHex[x & 0x0F];
  }

  return out;
}

String decryptHexXor(const String& cipherHex, const String& key) {
  if (cipherHex.length() == 0 || (cipherHex.length() % 2) != 0 || key.length() == 0) {
    return "";
  }

  String out;
  out.reserve(cipherHex.length() / 2);
  for (size_t i = 0; i < cipherHex.length(); i += 2) {
    const int hi = fromHexNibble(cipherHex.charAt(i));
    const int lo = fromHexNibble(cipherHex.charAt(i + 1));
    if (hi < 0 || lo < 0) {
      return "";
    }

    const uint8_t enc = static_cast<uint8_t>((hi << 4) | lo);
    const uint8_t dec = enc ^ static_cast<uint8_t>(key.charAt((i / 2) % key.length()));
    out += static_cast<char>(dec);
  }

  return out;
}
}

GeneralConfigController::GeneralConfigController(SdController& sdController,
                                                 DisplayManager& displayManager,
                                                 const char* defaultTimezonePosix,
                                                 int16_t defaultTimeOffsetMinutes,
                                                 uint8_t defaultBrightness)
    : sdController_(sdController),
      displayManager_(displayManager),
      timezonePosix_(defaultTimezonePosix),
      timeOffsetMinutes_(defaultTimeOffsetMinutes),
      brightness_(defaultBrightness),
      ftpUsername_(DEFAULT_FTP_USERNAME),
      ftpPassword_(DEFAULT_FTP_PASSWORD),
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

bool GeneralConfigController::isValidConfig(const String& timezone,
                                            int offsetMinutes,
                                            int brightness,
                                            const String& ftpUsername,
                                            const String& ftpPassword) const {
  return timezone.length() > 0 && offsetMinutes >= -720 && offsetMinutes <= 840 && brightness >= 0 && brightness <= 7 &&
         ftpUsername.length() > 0 && ftpUsername.length() <= 32 && ftpPassword.length() >= 4 && ftpPassword.length() <= 32;
}

String GeneralConfigController::encryptPassword(const String& plainText) const {
  return "v2:" + encryptHexXor(plainText, buildDevicePasswordKey());
}

String GeneralConfigController::decryptPassword(const String& cipherHex) const {
  if (cipherHex.length() == 0) {
    return "";
  }

  if (cipherHex.startsWith("v2:")) {
    return decryptHexXor(cipherHex.substring(3), buildDevicePasswordKey());
  }

  return "";
}

String GeneralConfigController::buildDevicePasswordKey() const {
  const uint64_t efuseMac = ESP.getEfuseMac();
  char key[17];
  snprintf(key, sizeof(key), "%08lX%08lX",
           static_cast<unsigned long>(efuseMac >> 32),
           static_cast<unsigned long>(efuseMac & 0xFFFFFFFFULL));
  return String(key);
}

bool GeneralConfigController::readConfigFromJsonFile(const String& jsonPayload, ConfigData& outConfig) const {
  StaticJsonDocument<512> doc;
  const DeserializationError err = deserializeJson(doc, jsonPayload);
  if (err) {
    return false;
  }

  const String hostname = doc["hostname"] | "";
  const String timezone = doc["timezone"] | defaultTimezonePosix_;
  const int offsetMinutes = doc["timeOffsetMinutes"] | defaultTimeOffsetMinutes_;
  const int brightness = doc["brightness"] | defaultBrightness_;
  const String ftpUsername = doc["ftpUsername"] | DEFAULT_FTP_USERNAME;
  const String ftpPasswordEncrypted = doc["ftpPasswordEnc"] | "";
  String ftpPassword = decryptPassword(ftpPasswordEncrypted);
  if (ftpPassword.length() == 0) {
    ftpPassword = doc["ftpPassword"] | DEFAULT_FTP_PASSWORD;
  }

  if (!isValidConfig(timezone, offsetMinutes, brightness, ftpUsername, ftpPassword)) {
    return false;
  }

  outConfig.hostname = hostname;
  outConfig.timezonePosix = timezone;
  outConfig.timeOffsetMinutes = static_cast<int16_t>(offsetMinutes);
  outConfig.brightness = static_cast<uint8_t>(brightness);
  outConfig.ftpUsername = ftpUsername;
  outConfig.ftpPassword = ftpPassword;
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

  if (!sdController_.exists(GENERAL_CONFIG_FILE)) {
    return false;
  }

  File file = sdController_.open(GENERAL_CONFIG_FILE, FILE_READ);
  if (!file) {
    return false;
  }

  const String payload = file.readString();
  file.close();
  return readConfigFromJsonFile(payload, outConfig);
}

bool GeneralConfigController::initialize() {
  timezonePosix_ = defaultTimezonePosix_;
  timeOffsetMinutes_ = defaultTimeOffsetMinutes_;
  brightness_ = defaultBrightness_;
  ftpUsername_ = DEFAULT_FTP_USERNAME;
  ftpPassword_ = DEFAULT_FTP_PASSWORD;

  ConfigData config;
  if (readFromSdCard(config)) {
    hostname_ = config.hostname;
    timezonePosix_ = config.timezonePosix;
    timeOffsetMinutes_ = config.timeOffsetMinutes;
    brightness_ = config.brightness;
    ftpUsername_ = config.ftpUsername;
    ftpPassword_ = config.ftpPassword;

    return saveToLittleFs(); // internal storage must not fail
  }

  if (readFromLittleFs(config)) {
    hostname_ = config.hostname;
    timezonePosix_ = config.timezonePosix;
    timeOffsetMinutes_ = config.timeOffsetMinutes;
    brightness_ = config.brightness;
    ftpUsername_ = config.ftpUsername;
    ftpPassword_ = config.ftpPassword;

    saveToSdCard();

    return true; // it doesn't matter if saving to SD card fails, we still have a valid config in LittleFS
  }

  return saveToAllStorages();
}

void GeneralConfigController::applyToDisplay() {
  displayManager_.setBrightness(brightness_);
}

bool GeneralConfigController::saveToLittleFs() {
  if (!ensureInternalFsMounted()) {
    Serial.println("general config: failed to mount LittleFS");
    return false;
  }

  if (LittleFS.exists(GENERAL_CONFIG_FILE) && !LittleFS.remove(GENERAL_CONFIG_FILE)) {
    Serial.println("general config: failed to remove existing config file from LittleFS");
    return false;
  }

  StaticJsonDocument<512> doc;
  doc["hostname"] = hostname_;
  doc["timezone"] = timezonePosix_;
  doc["timeOffsetMinutes"] = timeOffsetMinutes_;
  doc["brightness"] = brightness_;
  doc["ftpUsername"] = ftpUsername_;
  doc["ftpPasswordEnc"] = encryptPassword(ftpPassword_);

  File file = LittleFS.open(GENERAL_CONFIG_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("general config: failed to open config file for writing to LittleFS");
    return false;
  }

  const bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

bool GeneralConfigController::saveToSdCard() {
  if (!sdController_.isReady()) {
    Serial.println("general config: SD card not ready, cannot save config");
    return false;
  }

  if (sdController_.exists(GENERAL_CONFIG_FILE) && !sdController_.remove(GENERAL_CONFIG_FILE)) {
    Serial.println("general config: failed to remove existing config file from SD card");
    return false;
  }

  StaticJsonDocument<512> doc;
  doc["hostname"] = hostname_;
  doc["timezone"] = timezonePosix_;
  doc["timeOffsetMinutes"] = timeOffsetMinutes_;
  doc["brightness"] = brightness_;
  doc["ftpUsername"] = ftpUsername_;
  doc["ftpPasswordEnc"] = encryptPassword(ftpPassword_);

  File file = sdController_.open(GENERAL_CONFIG_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("general config: failed to open config file for writing to SD card");
    return false;
  }

  const bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

bool GeneralConfigController::saveToAllStorages() {
  const bool littleFsOk = saveToLittleFs();
  const bool sdOk = sdController_.isReady() ? saveToSdCard() : true;
  return littleFsOk || sdOk; // at least one storage must succeed, otherwise we have no valid config anywhere
}

void GeneralConfigController::handleGetConfig(WebServer& webServer) {
  StaticJsonDocument<512> doc;
  doc["hostname"] = hostname_;
  doc["timezone"] = timezonePosix_;
  doc["timeOffsetMinutes"] = timeOffsetMinutes_;
  doc["brightness"] = brightness_;
  doc["ftpUsername"] = ftpUsername_;
  doc["ftpPassword"] = ftpPassword_;

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

  StaticJsonDocument<512> doc;
  const DeserializationError err = deserializeJson(doc, webServer.arg("plain"));
  if (err) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
    return;
  }

  const String timezone = doc["timezone"] | "";
  const int offsetMinutes = doc["timeOffsetMinutes"] | 0;
  const int brightness = doc["brightness"] | -1;
  const String hostname = doc["hostname"] | "";
  const String ftpUsername = doc["ftpUsername"] | DEFAULT_FTP_USERNAME;
  const String ftpPassword = doc["ftpPassword"] | "";

  if (!isValidConfig(timezone, offsetMinutes, brightness, ftpUsername, ftpPassword)) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid configuration\"}");
    return;
  }

  timezonePosix_ = timezone;
  timeOffsetMinutes_ = static_cast<int16_t>(offsetMinutes);
  brightness_ = static_cast<uint8_t>(brightness);
  hostname_ = hostname;
  ftpUsername_ = ftpUsername;
  ftpPassword_ = ftpPassword;

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

const String& GeneralConfigController::hostname() const {
  return hostname_;
}

uint8_t GeneralConfigController::brightness() const {
  return brightness_;
}

const String& GeneralConfigController::timezonePosix() const {
  return timezonePosix_;
}

int16_t GeneralConfigController::timeOffsetMinutes() const {
  return timeOffsetMinutes_;
}

const String& GeneralConfigController::ftpUsername() const {
  return ftpUsername_;
}

const String& GeneralConfigController::ftpPassword() const {
  return ftpPassword_;
}

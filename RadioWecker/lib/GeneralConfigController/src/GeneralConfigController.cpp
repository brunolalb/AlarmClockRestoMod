#include "GeneralConfigController.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SdController.h>

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

GeneralConfigController::GeneralConfigController(SdController& sdController)
    : sdController_(sdController) {}

bool GeneralConfigController::initialize(const ConfigData *default_config) {
  if (!default_config) {
    Serial.println("general config: no default config provided");
    return false;
  }

  // copy the default config
  ConfigData new_config;
  memcpy(&new_config, default_config, sizeof(ConfigData));

  if (readFromSdCard(&new_config)) {
    if (saveToLittleFs(&new_config)) {
      memcpy(&config_, &new_config, sizeof(ConfigData));
      return true;
    } else {
      return false; // internal storage must not fail
    }
  }

  if (readFromLittleFs(&new_config)) {
    saveToSdCard(&new_config);

    memcpy(&config_, &new_config, sizeof(ConfigData));

    return true; // it doesn't matter if saving to SD card fails, we still have a valid config in LittleFS
  }

  return saveToAllStorages(&new_config);
}

bool GeneralConfigController::ensureInternalFsMounted() {
  if (internalFsMounted_) {
    return true;
  }

  internalFsMounted_ = LittleFS.begin(true);
  return internalFsMounted_;
}

bool GeneralConfigController::isValidConfig(const ConfigData *config) const {
  return  config->timezonePosix.length() > 0 &&
          config->timeOffsetMinutes >= -720 && config->timeOffsetMinutes <= 840 &&
          config->brightness >= 0 && config->brightness <= 7 &&
          !config->ftpUsername.isEmpty() && config->ftpUsername.length() <= 32 &&
          !config->ftpPassword.isEmpty() && config->ftpPassword.length() <= 32;
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

bool GeneralConfigController::readConfigFromJsonFile(const String& jsonPayload, ConfigData* config) const {
  // reads the config from a JSON payload, updates the internal config_ if successful
  StaticJsonDocument<512> doc;
  const DeserializationError err = deserializeJson(doc, jsonPayload);
  if (err) {
    return false;
  }

  ConfigData new_config = {
    .hostname = doc["hostname"] | "",
    .timezonePosix = doc["timezone"] | "",
    .timeOffsetMinutes = static_cast<int16_t>(doc["timeOffsetMinutes"] | 0),
    .brightness = static_cast<uint8_t>(doc["brightness"] | 0),
    .ftpUsername = doc["ftpUsername"] | "",
    .ftpPassword = ""
  };
  // password comes encrypted from the JSON, we need to decrypt it
  String ftp_psw_crypt = doc["ftpPassword"] | "";
  String ftpPassword = decryptPassword(ftp_psw_crypt);
  if (ftpPassword.isEmpty()) {
    ftpPassword = config_.ftpPassword; // fallback to the current password if decryption fails
  }
  new_config.ftpPassword = ftpPassword;

  if (!isValidConfig(&new_config)) {
    return false;
  }

  // copy new config to the parameter
  memcpy(config, &new_config, sizeof(ConfigData));

  return true;
}

bool GeneralConfigController::readFromLittleFs(ConfigData* config) {
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
  return readConfigFromJsonFile(payload, config);
}

bool GeneralConfigController::readFromSdCard(ConfigData* config) {
  // reads config from SD card, saves it in the internal config_
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
  return readConfigFromJsonFile(payload, config);
}

bool GeneralConfigController::saveToLittleFs(const ConfigData* new_config) {
  if (!ensureInternalFsMounted()) {
    Serial.println("general config: failed to mount LittleFS");
    return false;
  }

  if (LittleFS.exists(GENERAL_CONFIG_FILE) && !LittleFS.remove(GENERAL_CONFIG_FILE)) {
    Serial.println("general config: failed to remove existing config file from LittleFS");
    return false;
  }

  StaticJsonDocument<512> doc;
  doc["hostname"] = new_config->hostname;
  doc["timezone"] = new_config->timezonePosix;
  doc["timeOffsetMinutes"] = new_config->timeOffsetMinutes;
  doc["brightness"] = new_config->brightness;
  doc["ftpUsername"] = new_config->ftpUsername;
  doc["ftpPasswordEnc"] = encryptPassword(new_config->ftpPassword);

  File file = LittleFS.open(GENERAL_CONFIG_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("general config: failed to open config file for writing to LittleFS");
    return false;
  }

  const bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

bool GeneralConfigController::saveToSdCard(const ConfigData* new_config) {
  if (!sdController_.isReady()) {
    Serial.println("general config: SD card not ready, cannot save config");
    return false;
  }

  if (sdController_.exists(GENERAL_CONFIG_FILE) && !sdController_.remove(GENERAL_CONFIG_FILE)) {
    Serial.println("general config: failed to remove existing config file from SD card");
    return false;
  }

  StaticJsonDocument<512> doc;
  doc["hostname"] = new_config->hostname;
  doc["timezone"] = new_config->timezonePosix;
  doc["timeOffsetMinutes"] = new_config->timeOffsetMinutes;
  doc["brightness"] = new_config->brightness;
  doc["ftpUsername"] = new_config->ftpUsername;
  doc["ftpPasswordEnc"] = encryptPassword(new_config->ftpPassword);

  File file = sdController_.open(GENERAL_CONFIG_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("general config: failed to open config file for writing to SD card");
    return false;
  }

  const bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

bool GeneralConfigController::saveToAllStorages(const ConfigData* new_config) {
  const bool littleFsOk = saveToLittleFs(new_config);
  const bool sdOk = saveToSdCard(new_config);
  if (littleFsOk || sdOk) {
    memcpy(&config_, new_config, sizeof(ConfigData)); // update the internal config only if both storages succeeded
  }
  return littleFsOk || sdOk; // at least one storage must succeed, otherwise we have no valid config anywhere
}

void GeneralConfigController::configToJson(JsonDocument& doc) {
  // fills the provided JsonDocument with the current configuration values
  doc["hostname"] = config_.hostname;
  doc["timezone"] = config_.timezonePosix;
  doc["timeOffsetMinutes"] = config_.timeOffsetMinutes;
  doc["brightness"] = config_.brightness;
  doc["ftpUsername"] = config_.ftpUsername;
  doc["ftpPassword"] = config_.ftpPassword;
}

String GeneralConfigController::jsonToConfig(const JsonDocument& doc) {
  // saves the config to the internal storage and SD card, returns an empty string on success, or an error message on failure

  ConfigData new_config = {
    .hostname = doc["hostname"] | "",
    .timezonePosix = doc["timezone"] | "",
    .timeOffsetMinutes = static_cast<int16_t>(doc["timeOffsetMinutes"] | 0),
    .brightness = static_cast<uint8_t>(doc["brightness"] | 0),
    .ftpUsername = doc["ftpUsername"] | "",
    .ftpPassword = doc["ftpPassword"] | ""
  };

  if (!isValidConfig(&new_config)) {
    return "Invalid configuration";
  }

  if (!saveToAllStorages(&new_config)) {
    return "Failed to save configuration";
  }

  return "";
}

const String& GeneralConfigController::hostname() const {
  return config_.hostname;
}

uint8_t GeneralConfigController::brightness() const {
  return config_.brightness;
}

const String& GeneralConfigController::timezonePosix() const {
  return config_.timezonePosix;
}

int16_t GeneralConfigController::timeOffsetMinutes() const {
  return config_.timeOffsetMinutes;
}

const String& GeneralConfigController::ftpUsername() const {
  return config_.ftpUsername;
}

const String& GeneralConfigController::ftpPassword() const {
  return config_.ftpPassword;
}

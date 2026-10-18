#include "GeneralConfigController.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SdController.h>

GeneralConfigController::GeneralConfigController(SdController* sdController)
    : sdController_(sdController) {}

bool GeneralConfigController::initialize(const ConfigData *default_config, Coordinator* coordinator) {
  assert(coordinator != nullptr);
  assert(default_config != nullptr);
  
  coordinator_ = coordinator;

  // copy the default config
  ConfigData new_config = *default_config;

  // SD Card config has higher priority
  ConfigData config_SD;
  bool config_SD_ok = SdCard_read(&config_SD);
  ConfigData config_LittleFS;
  bool config_LittleFS_ok = LittleFS_read(&config_LittleFS);

  if (!config_SD_ok && !config_LittleFS_ok) {
    Serial.println("cfg: no valid cfg, using default");
    return saveToAllStorages(&new_config);
  } else {
    // internal storage (LittleFS) has priority over SD card
    if (config_LittleFS_ok) {
      config_ = config_LittleFS;
      // to prevent wear on the SD card, only write if necessary
      if (!config_SD_ok || !compareConfig(&config_LittleFS, &config_SD)) {
        // if the config on the SD card is different, save it
        return SdCard_save(&config_LittleFS);
      }
      return true;
    } else {
      // SD Card has a valid config, copy to internal
      config_ = config_SD;
      // to prevent wear on the LittleFS, only write if necessary
      if (!config_LittleFS_ok || !compareConfig(&config_SD, &config_LittleFS)) {
        // if the config on the littleFS is different, save it
        return LittleFS_save(&config_SD);
      }
      return true;
    }
  }

  return false; // no way to get here
}

bool GeneralConfigController::isValidConfig(const ConfigData *config) const {
  if (config->timezonePosix.length() == 0) {
    Serial.print("cfg: invalid timezonePosix: ");
    Serial.println(config->timezonePosix);
    return false;
  }
  if (config->timeOffsetMinutes < -720 || config->timeOffsetMinutes > 840) {
    Serial.print("cfg: invalid timeOffsetMinutes: ");
    Serial.println(config->timeOffsetMinutes);
    return false;
  }
  if (config->brightness < 0 || config->brightness > 7) {
    Serial.print("cfg: invalid brightness: ");
    Serial.println(config->brightness);
    return false;
  }
  if (config->ftpUsername.isEmpty() || config->ftpUsername.length() > 32) {
    Serial.print("cfg: invalid ftpUsername: ");
    Serial.println(config->ftpUsername);
    return false;
  }
  if (config->ftpPassword.isEmpty() || config->ftpPassword.length() > 32) {
    Serial.print("cfg: invalid ftpPassword: ");
    Serial.println(config->ftpPassword);
    return false;
  }
  return true;
}

bool GeneralConfigController::compareConfig(const ConfigData* config1, const ConfigData* config2) const {
  if (config1->hostname != config2->hostname) {
    Serial.print("cfg: hostname mismatch: ");
    Serial.print(config1->hostname);
    Serial.print(" != ");
    Serial.println(config2->hostname);
    return false;
  }
  if (config1->timezonePosix != config2->timezonePosix) {
    Serial.print("cfg: timezonePosix mismatch: ");
    Serial.print(config1->timezonePosix);
    Serial.print(" != ");
    Serial.println(config2->timezonePosix);
    return false;
  }
  if (config1->timeOffsetMinutes != config2->timeOffsetMinutes) {
    Serial.print("cfg: timeOffsetMinutes mismatch: ");
    Serial.print(config1->timeOffsetMinutes);
    Serial.print(" != ");
    Serial.println(config2->timeOffsetMinutes);
    return false;
  }
  if (config1->brightness != config2->brightness) {
    Serial.print("cfg: brightness mismatch: ");
    Serial.print(config1->brightness);
    Serial.print(" != ");
    Serial.println(config2->brightness);
    return false;
  }
  if (config1->ftpUsername != config2->ftpUsername) {
    Serial.print("cfg: ftpUsername mismatch: ");
    Serial.print(config1->ftpUsername);
    Serial.print(" != ");
    Serial.println(config2->ftpUsername);
    return false;
  }
  if (config1->ftpPassword != config2->ftpPassword) {
    Serial.print("cfg: ftpPassword mismatch: ");
    Serial.print(config1->ftpPassword);
    Serial.print(" != ");
    Serial.println(config2->ftpPassword);
    return false;
  }
  return true;
}


/*********************************/
/********* File Handling *********/
/*********************************/

/******* LittleFS ********/

bool GeneralConfigController::LittleFS_ensureInit() {
  return LittleFS.begin(true);
}

bool GeneralConfigController::LittleFS_read(ConfigData* config) {
  Serial.println("config: LittleFS_read");
  if (!LittleFS_ensureInit()) {
    Serial.println("cfg: failed to mount LittleFS");
    return false;
  }

  if (!LittleFS.exists(GENERAL_CONFIG_FILE)) {
    Serial.println("cfg: no config file on LittleFS");
    return false;
  }

  File file = LittleFS.open(GENERAL_CONFIG_FILE, FILE_READ);
  if (!file) {
    Serial.println("cfg: failed to open config file on LittleFS");
    return false;
  }

  const String payload = file.readString();
  file.close();
  return parseJson(payload, config);
}

bool GeneralConfigController::LittleFS_save(const ConfigData* new_config) {
  Serial.println("config: saving to LittleFS");
  if (!LittleFS_ensureInit()) {
    Serial.println("config: failed to mount LittleFS");
    return false;
  }

  if (LittleFS.exists(GENERAL_CONFIG_FILE) && !LittleFS.remove(GENERAL_CONFIG_FILE)) {
    Serial.println("config: failed to remove LittleFS");
    return false;
  }

  File file = LittleFS.open(GENERAL_CONFIG_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("config: failed to open LittleFS");
    return false;
  }

  const bool ok = saveJSON(new_config, file);
  file.close();
  return ok;
}

/******* SD Card ********/

bool GeneralConfigController::SdCard_read(ConfigData* config) {
  // reads config from SD card, saves it in the internal config_
  if (!sdController_ || !sdController_->isReady()) {
    Serial.println("cfg: no SD");
    return false;
  }

  // if the file doesnt exist, nothing to read
  if (!sdController_->exists(GENERAL_CONFIG_FILE)) {
    Serial.println("cfg: no file on SD");
    return false;
  }

  // open the file from the SD card
  File file = sdController_->open(GENERAL_CONFIG_FILE, FILE_READ);
  if (!file) {
    Serial.println("cfg: failed to open file on SD");
    return false;
  }

  // read the json from the file
  const String payload = file.readString();
  file.close();

  // parse the json
  return parseJson(payload, config);
}

bool GeneralConfigController::SdCard_save(const ConfigData* new_config) {
  if (!sdController_ || !sdController_->isReady()) {
    Serial.println("config: no SD");
    return false;
  }

  if (sdController_->exists(GENERAL_CONFIG_FILE) && !sdController_->remove(GENERAL_CONFIG_FILE)) {
    Serial.println("config: failed to remove SD");
    return false;
  }

  File file = sdController_->open(GENERAL_CONFIG_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("config: failed to open SD");
    return false;
  }

  const bool ok = saveJSON(new_config, file);
  file.close();
  return ok;
}

/******* Generic ********/

bool GeneralConfigController::saveToAllStorages(const ConfigData* new_config) {
  const bool littleFsOk = LittleFS_save(new_config);
  const bool sdOk = SdCard_save(new_config);
  if (littleFsOk || sdOk) {
    config_ = *new_config;
  }
  return littleFsOk || sdOk; // at least one storage must succeed, otherwise we have no valid config anywhere
}


/*********************************/
/************ JSON ***************/
/*********************************/

bool GeneralConfigController::deserialize(const String& payload, ConfigData* config) const {
  // from string payload (json) to ConfigData structure
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.println("cfg: failed to parse JSON: " + String(err.c_str()));
    return false;
  }

  ConfigData new_config = {
    .hostname = doc["hostname"] | config_.hostname,
    .timezonePosix = doc["timezone"] | config_.timezonePosix,
    .timeOffsetMinutes = static_cast<int16_t>(doc["timeOffsetMinutes"] | config_.timeOffsetMinutes),
    .brightness = static_cast<uint8_t>(doc["brightness"] | config_.brightness),
    .ftpUsername = doc["ftpUsername"] | config_.ftpUsername,
    .ftpPassword = doc["ftpPassword"] | config_.ftpPassword
  };

  *config = new_config;

  return true;
}

bool GeneralConfigController::parseJson(const String& jsonPayload, ConfigData* config) const {
  // reads the config from a String payload (JSON), updates the internal config_ if successful
  ConfigData new_config;

  if (!deserialize(jsonPayload, &new_config)) {
    return false;
  }

  if (!isValidConfig(&new_config)) {
    Serial.println("cfg: invalid config");
    return false;
  }

  // copy new config to the parameter
  *config = new_config;

  return true;
}

JsonDocument GeneralConfigController::makeJSON(const ConfigData* config) const {
  JsonDocument doc;
  doc["hostname"] = config->hostname;
  doc["timezone"] = config->timezonePosix;
  doc["timeOffsetMinutes"] = config->timeOffsetMinutes;
  doc["brightness"] = config->brightness;
  doc["ftpUsername"] = config->ftpUsername;
  doc["ftpPassword"] = config->ftpPassword;

  return doc;
}

bool GeneralConfigController::saveJSON(const ConfigData* config, File file) {
  JsonDocument doc = makeJSON(config);

  Serial.print("config: writing JSON: ");
  serializeJson(doc, Serial);

  const bool ok = serializeJson(doc, file) > 0;
  return ok;
}

/***************************/
/******** API **************/
/***************************/

JsonDocument GeneralConfigController::json() {
  // fills the provided JsonDocument with the current configuration values
  JsonDocument doc;
  doc["hostname"] = config_.hostname;
  doc["timezone"] = config_.timezonePosix;
  doc["timeOffsetMinutes"] = config_.timeOffsetMinutes;
  doc["brightness"] = config_.brightness;
  doc["ftpUsername"] = config_.ftpUsername;
  doc["ftpPassword"] = config_.ftpPassword;

  return doc;
}

String GeneralConfigController::jsonToConfig(const String& jsonPayload) {
  // saves the config to the internal storage and SD card, returns an empty string on success, or an error message on failure
  ConfigData new_config;

  Serial.println(jsonPayload);

  if (!deserialize(jsonPayload, &new_config)) {
    return "Failed to parse JSON";
  }

  if (compareConfig(&new_config, &config_)) {
    Serial.println("config: no changes detected");
    return "";
  }

  if (!isValidConfig(&new_config)) {
    Serial.println("config: invalid cfg");
    return "Invalid configuration";
  }

  if (!saveToAllStorages(&new_config)) {
    Serial.println("config: failed to save");
    return "Failed to save configuration";
  }

  if (coordinator_) {
    coordinator_->postEvent({Coordinator::EventType::NewConfig, 0});
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

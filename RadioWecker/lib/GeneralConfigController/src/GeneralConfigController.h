#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include <Coordinator.h>

class SdController;

class GeneralConfigController {
 public:
  struct ConfigData {
    String hostname;
    String timezonePosix;
    int16_t timeOffsetMinutes;
    uint8_t brightness;
    String ftpUsername;
    String ftpPassword;
  };

  explicit GeneralConfigController(SdController* sdController);

  bool initialize(const ConfigData *default_config,
                  Coordinator* coordinator);

  JsonDocument json();
  String jsonToConfig(const String& jsonPayload);

  uint8_t brightness() const;
  const String& hostname() const;
  const String& timezonePosix() const;
  int16_t timeOffsetMinutes() const;
  const String& ftpUsername() const;
  const String& ftpPassword() const;

 private:
  bool LittleFS_ensureInit();
  bool LittleFS_read(ConfigData* config);
  bool LittleFS_save(const ConfigData* new_config);
  bool SdCard_read(ConfigData* config);
  bool SdCard_save(const ConfigData* new_config);
  bool saveToAllStorages(const ConfigData* new_config);
  
  bool deserialize(const String& payload, ConfigData* config) const;
  bool parseJson(const String& jsonPayload, ConfigData* config) const;
  
  bool isValidConfig(const ConfigData *config) const;
  bool compareConfig(const ConfigData* config1, const ConfigData* config2) const;
  String buildDevicePasswordKey() const;
  String encryptPassword(const String& plainText) const;
  String decryptPassword(const String& cipherHex) const;

  static constexpr const char* GENERAL_CONFIG_FILE = "/general_config.json";

  SdController* sdController_;
  Coordinator* coordinator_ = nullptr;

  ConfigData config_;

  bool internalFsMounted_ = false;
};

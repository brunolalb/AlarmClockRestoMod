#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

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

  explicit GeneralConfigController(SdController& sdController);

  bool initialize(const ConfigData *default_config);

  void configToJson(JsonDocument& doc);
  String jsonToConfig(const JsonDocument& doc);

  uint8_t brightness() const;
  const String& hostname() const;
  const String& timezonePosix() const;
  int16_t timeOffsetMinutes() const;
  const String& ftpUsername() const;
  const String& ftpPassword() const;

 private:
  bool ensureInternalFsMounted();
  bool readFromLittleFs(ConfigData* config);
  bool readFromSdCard(ConfigData* config);
  bool readConfigFromJsonFile(const String& jsonPayload, ConfigData* config) const;
  bool saveToAllStorages(const ConfigData* new_config);
  bool saveToLittleFs(const ConfigData* new_config);
  bool saveToSdCard(const ConfigData* new_config);
  bool isValidConfig(const ConfigData *config) const;
  String buildDevicePasswordKey() const;
  String encryptPassword(const String& plainText) const;
  String decryptPassword(const String& cipherHex) const;

  static constexpr const char* GENERAL_CONFIG_FILE = "/general_config.json";

  SdController& sdController_;

  ConfigData config_;

  bool internalFsMounted_ = false;
};

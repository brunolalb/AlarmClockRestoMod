#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class SdController;

class GeneralConfigController {
 public:
  explicit GeneralConfigController( SdController& sdController,
                                    const char* defaultTimezonePosix,
                                    int16_t defaultTimeOffsetMinutes,
                                    uint8_t defaultBrightness);

  bool initialize();

  void configToJson(JsonDocument& doc);
  String jsonToConfig(const JsonDocument& doc);

  uint8_t brightness() const;
  const String& hostname() const;
  const String& timezonePosix() const;
  int16_t timeOffsetMinutes() const;
  const String& ftpUsername() const;
  const String& ftpPassword() const;

 private:
  struct ConfigData {
    String hostname;
    String timezonePosix;
    int16_t timeOffsetMinutes;
    uint8_t brightness;
    String ftpUsername;
    String ftpPassword;
  };

  bool ensureInternalFsMounted();
  bool readFromLittleFs(ConfigData& outConfig);
  bool readFromSdCard(ConfigData& outConfig);
  bool readConfigFromJsonFile(const String& jsonPayload, ConfigData& outConfig) const;
  String buildDevicePasswordKey() const;
  String encryptPassword(const String& plainText) const;
  String decryptPassword(const String& cipherHex) const;
  bool saveToAllStorages();
  bool saveToLittleFs();
  bool saveToSdCard();
  bool isValidConfig(const String& timezone,
                     int offsetMinutes,
                     int brightness,
                     const String& ftpUsername,
                     const String& ftpPassword) const;

  static constexpr const char* GENERAL_CONFIG_FILE = "/general_config.json";

  SdController& sdController_;

  String hostname_;
  String timezonePosix_;
  int16_t timeOffsetMinutes_;
  uint8_t brightness_;
  String ftpUsername_;
  String ftpPassword_;

  const char* defaultTimezonePosix_;
  int16_t defaultTimeOffsetMinutes_;
  uint8_t defaultBrightness_;
  static constexpr const char* DEFAULT_FTP_USERNAME = "radiowecker";
  static constexpr const char* DEFAULT_FTP_PASSWORD = "radiowecker";

  bool internalFsMounted_ = false;
};

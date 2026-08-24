#pragma once

#include <Arduino.h>

class DisplayManager;
class SdController;
class WebServer;

class GeneralConfigController {
 public:
  explicit GeneralConfigController( SdController& sdController,
                                    DisplayManager& displayManager,
                                    const char* defaultTimezonePosix,
                                    int16_t defaultTimeOffsetMinutes,
                                    uint8_t defaultBrightness);

  bool initialize();
  void applyToDisplay();

  void handleGetConfig(WebServer& webServer);
  void handleSaveConfig(WebServer& webServer);

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
  DisplayManager& displayManager_;

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

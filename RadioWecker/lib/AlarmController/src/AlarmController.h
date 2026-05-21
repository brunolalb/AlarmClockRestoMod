#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class WebServer;

#include <SdController.h>

class AlarmController {
 public:
  explicit AlarmController(SdController& sdController);

  void begin();
  void handleGetAlarmConfig(WebServer& webServer);
  void handleSaveAlarmConfig(WebServer& webServer);
  void handleListMusicFiles(WebServer& webServer);
  bool isInitialized() const;
  uint8_t alarmCount() const;

 private:
  static constexpr uint8_t MAX_ALARMS = 10;
  static const char* ALARM_FILE;

  struct AlarmSettings {
    String time;
    bool days[7];
    uint16_t snoozeMin;
    String soundType;
    String musicPath;
  };

  void setDefaultAlarmSettings(AlarmSettings& settings);
  bool isValidTimeString(const String& timeValue) const;
  bool parseAlarmFromJson(JsonVariantConst alarmVariant, AlarmSettings& outSettings) const;
  void appendAlarmToJsonArray(const AlarmSettings& settings, JsonArray& alarms) const;
  bool parseAlarmSettingsDocument(JsonVariantConst root, AlarmSettings* settings, uint8_t& count) const;

  bool saveAlarmSettingsToSd(const AlarmSettings* settings, uint8_t count);
  bool saveAlarmSettingsToLittleFs(const AlarmSettings* settings, uint8_t count);
  bool saveAlarmSettingsToAll(const AlarmSettings* settings, uint8_t count);
  bool loadAlarmSettingsFromSd(AlarmSettings* settings, uint8_t& count);
  bool loadAlarmSettingsFromLittleFs(AlarmSettings* settings, uint8_t& count);
  bool loadAlarmSettings(AlarmSettings* settings, uint8_t& count);
  bool saveCurrentAlarmSettings();

  void sendAlarmConfigJson(WebServer& webServer);

  SdController& sdController_;

  AlarmSettings alarmSettings_[MAX_ALARMS];
  uint8_t alarmCount_ = 0;
  bool initialized_ = false;
};

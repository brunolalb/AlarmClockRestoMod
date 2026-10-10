#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class WebServer;

#include <SdController.h>

class AlarmController {
 public:
  explicit AlarmController(SdController* sdController);

  bool initialize();
  void handleGetAlarmConfig(WebServer& webServer);
  void handleSaveAlarmConfig(WebServer& webServer);
  void handleListMusicFiles(WebServer& webServer);
  uint8_t alarmCount() const;

 private:
  static constexpr uint8_t MAX_ALARMS = 10;
  static const char* ALARM_FILE;
  
  struct AlarmSettings {
    bool enabled;
    bool enabled_days[7];
    uint8_t time_hour;
    uint8_t time_min;
    uint8_t snooze_min;
    bool sound_in_SD;
    String soundPath;
    uint16_t soundStartAt;
    uint16_t soundVolume;
  };

  bool isValidSettings(const AlarmSettings& settings) const;
  bool compareAlarmSettings(const AlarmSettings* settings1, uint8_t count1, const AlarmSettings* settings2, uint8_t count2) const;
  /*********************************/
  /********* File Handling *********/
  /*********************************/
  /******* LittleFS ********/
  bool LittleFS_ensureInit();
  bool LittleFS_read(AlarmSettings* settings, uint8_t& count);
  bool LittleFS_save(const AlarmSettings* settings, uint8_t count);
  /******* SD Card ********/
  bool SDCard_read(AlarmSettings* settings, uint8_t& count);
  bool SDCard_save(const AlarmSettings* settings, uint8_t count);
  /******* Generic ********/
  bool loadAlarmSettings(const AlarmSettings* settings, uint8_t count);
  bool saveAlarmSettings(const AlarmSettings* settings, uint8_t count);
  bool parseAlarmSettingsDocument(File file, AlarmSettings* settings, uint8_t& count) const;
  /*********************************/
  /********* JSON *********/
  /*********************************/
  bool parseJSON(JsonVariantConst alarmVariant, AlarmSettings& outSettings) const;
  JsonDocument makeJSON(const AlarmSettings* settings, uint8_t count);
  void appendToJSON(const AlarmSettings& settings, JsonArray& alarms) const;
  bool saveJSON(const AlarmSettings* settings, uint8_t count, File file);


  

  void sendAlarmConfigJson(WebServer& webServer);

  SdController* sdController_;

  AlarmSettings alarmSettings_[MAX_ALARMS];
  uint8_t alarmCount_ = 0;
};

#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include <AlarmController.h>

class ClockController;
class DisplayManager;

class WebServerController {
 public:
  explicit WebServerController(AlarmController& alarmController,
                               ClockController& clockController,
                               DisplayManager& displayManager,
                               uint16_t port = 80);

  void begin(bool enableWebServer);
  void update();
  bool isStarted() const;
  WebServer& server();

 private:
  bool ensureInternalFsMounted();
  void serveFile(const char* path, const char* notFoundMessage);
  void handleIndexPage();
  void handleAlarmPage();
  void handleConfigPage();
  void handleGetGeneralConfig();
  void handleSaveGeneralConfig();
  void applyGeneralConfig(const String& timezonePosix, int16_t timeOffsetMinutes, uint8_t brightness);
  void loadGeneralConfig();
  bool saveGeneralConfig();
  void setupRoutes();

  static constexpr const char* GENERAL_CONFIG_FILE = "/general_config.json";

  AlarmController& alarmController_;
  ClockController& clockController_;
  DisplayManager& displayManager_;
  WebServer webServer_;
  uint16_t port_;
  bool started_ = false;
  bool internalFsMounted_ = false;
  String timezonePosix_ = "UTC0";
  int16_t timeOffsetMinutes_ = 0;
  uint8_t brightness_ = 7;
};

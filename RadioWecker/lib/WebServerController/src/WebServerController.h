#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include <AlarmController.h>
#include <ClockController.h>
#include <GeneralConfigController.h>
#include <SdController.h>

class WebServerController {
 public:
  explicit WebServerController(AlarmController& alarmController,
                               ClockController& clockController,
                               SdController& sdController,
                               GeneralConfigController& generalConfigController,
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
  void handleStatusPage();
  void handleGetStatus();
  void setupRoutes();

  AlarmController& alarmController_;
  ClockController& clockController_;
  SdController& sdController_;
  GeneralConfigController& generalConfigController_;
  WebServer webServer_;
  uint16_t port_;
  bool started_ = false;
  bool internalFsMounted_ = false;
};

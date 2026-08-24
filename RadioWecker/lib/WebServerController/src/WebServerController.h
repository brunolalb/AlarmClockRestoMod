#pragma once

#include <Arduino.h>
#include <ESP32FtpServer.h>
#include <WebServer.h>

#include <AlarmController.h>
#include <ClockController.h>
#include <GeneralConfigController.h>
#include <SdController.h>
#include <SoundController.h>

class WebServerController {
 public:
  explicit WebServerController(AlarmController& alarmController,
                               ClockController& clockController,
                               SdController& sdController,
                               SoundController& soundController,
                               GeneralConfigController& generalConfigController,
                               uint16_t port = 80);

  bool initialize(bool wifi_is_connected);
  void update();
  bool isStarted() const;
  WebServer& server();

 private:
  bool ensureInternalFsMounted();
  void serveFile(const char* path, const char* notFoundMessage, const char* contentType = "text/html");
  void handleIndexPage();
  void handleAlarmPage();
  void handleConfigPage();
  void handleUploadPage();
  void handleSoundPage();
  void handleStatusPage();
  void handleGetStatus();
  void handleReboot();
  void handleSaveConfig();
  void setupRoutes();
  bool beginFtpServer();

  AlarmController& alarmController_;
  ClockController& clockController_;
  SdController& sdController_;
  SoundController& soundController_;
  GeneralConfigController& generalConfigController_;
  FtpServer ftpServer_;
  WebServer webServer_;
  uint16_t port_;
  bool started_ = false;
  bool internalFsMounted_ = false;
  bool ftpStarted_ = false;
};

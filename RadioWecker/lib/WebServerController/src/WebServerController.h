#pragma once

#include <Arduino.h>
#include <ESP32FtpServer.h>
#include <WebServer.h>
#include <thread>

#include <DisplayManager.h>
#include <AlarmController.h>
#include <ClockController.h>
#include <GeneralConfigController.h>
#include <SdController.h>
#include <SoundController.h>

class WebServerController {
 public:
  struct WebserverConfig {
    uint16_t port;
    String ftpUsername;
    String ftpPassword;
  };

  explicit WebServerController(AlarmController* alarmController,
                               ClockController* clockController,
                               SdController* sdController,
                               SoundController* soundController,
                               DisplayManager* displayManager,
                               GeneralConfigController* generalConfigController);

  bool initialize(const WebserverConfig* default_config);
  bool isReady() const { return ready_; }
  WebServer& server();
  bool reconfigure(const WebserverConfig* new_config);

 private:
  void updateTask();
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
  void handleGetConfig();
  void handleSaveConfig();
  void setupRoutes();
  bool beginFtpServer();

  AlarmController* alarmController_;
  ClockController* clockController_;
  SdController* sdController_;
  SoundController* soundController_;
  DisplayManager* displayManager_;
  GeneralConfigController* generalConfigController_;
  FtpServer ftpServer_;
  WebServer webServer_;

  WebserverConfig config_;

  bool ready_ = false;
  bool internalFsMounted_ = false;
  bool ftpStarted_ = false;
  std::thread _updateTask;
};

#pragma once

#include <Arduino.h>

#include <AlarmController.h>
#include <ClockController.h>
#include <SdController.h>

class WebServerController;

class CLIController {
 public:
  CLIController(ClockController& clockController,
                SdController& sdController,
                AlarmController& alarmController,
                WebServerController& webServerController);

  void begin();
  void update();

 private:
  void handleCommand(const String& rawCommand);
  void printHelp() const;
  void printIp() const;
  void printWifi() const;
  void printModuleStatus() const;

  ClockController& clockController_;
  SdController& sdController_;
  AlarmController& alarmController_;
  WebServerController& webServerController_;
  String inputBuffer_;
};

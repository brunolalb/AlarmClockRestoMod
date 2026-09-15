#pragma once

#include <Arduino.h>

#include <AlarmController.h>
#include <ButtonReader.h>
#include <ClockController.h>
#include <SdController.h>

class WebServerController;

class CLIController {
 public:
  CLIController(ClockController* clockController,
                SdController* sdController,
                AlarmController* alarmController,
                WebServerController* webServerController,
                ButtonReader* buttonReader);

  bool initialize();
  void update();

 private:
  void handleCommand(const String& rawCommand);
  void printHelp() const;
  void printWifi() const;
  void printModuleStatus() const;
  void printButtonStates() const;

  ClockController* clockController_;
  SdController* sdController_;
  AlarmController* alarmController_;
  WebServerController* webServerController_;
  ButtonReader* buttonReader_;
  String inputBuffer_;
};

#pragma once

#include <Arduino.h>

#include <AlarmController.h>
#include <ClockController.h>
#include <SdController.h>

class SerialController {
 public:
  SerialController(ClockController& clockController,
                   SdController& sdController,
                   AlarmController& alarmController,
                   const bool& webServerStarted);

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
  const bool& webServerStarted_;
  String inputBuffer_;
};

#include "SerialController.h"

#include <WiFi.h>

namespace {
String toCommandKey(const String& source) {
  String key = source;
  key.trim();
  key.toLowerCase();
  return key;
}
}

SerialController::SerialController(ClockController& clockController,
                                   SdController& sdController,
                                   AlarmController& alarmController,
                                   const bool& webServerStarted)
    : clockController_(clockController),
      sdController_(sdController),
      alarmController_(alarmController),
      webServerStarted_(webServerStarted) {}

void SerialController::begin() {
  Serial.println("Serial controller ready. Type 'help' for commands.");
}

void SerialController::update() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') {
      continue;
    }

    if (c == '\n') {
      handleCommand(inputBuffer_);
      inputBuffer_ = "";
      continue;
    }

    if (inputBuffer_.length() < 96) {
      inputBuffer_ += c;
    }
  }
}

void SerialController::handleCommand(const String& rawCommand) {
  const String command = toCommandKey(rawCommand);
  if (command.length() == 0) {
    return;
  }

  if (command == "help" || command == "?") {
    printHelp();
    return;
  }

  if (command == "ip") {
    printIp();
    return;
  }

  if (command == "wifi") {
    printWifi();
    return;
  }

  if (command == "status" || command == "modules") {
    printModuleStatus();
    return;
  }

  Serial.print("Unknown command: ");
  Serial.println(rawCommand);
  Serial.println("Type 'help' for available commands.");
}

void SerialController::printHelp() const {
  Serial.println("Commands:");
  Serial.println("  help      - Show this help");
  Serial.println("  ip        - Print local IP address");
  Serial.println("  wifi      - Print WiFi connection details");
  Serial.println("  status    - Print system/module state");
  Serial.println("  modules   - Alias for status");
}

void SerialController::printIp() const {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("IP: not connected");
    return;
  }

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void SerialController::printWifi() const {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi: disconnected");
    return;
  }

  Serial.print("WiFi SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("WiFi RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
  printIp();
}

void SerialController::printModuleStatus() const {
  Serial.println("Module status:");

  Serial.print("  wifi: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected");

  Serial.print("  webserver: ");
  Serial.println(webServerStarted_ ? "running" : "stopped");

  Serial.print("  clock: ");
  if (!clockController_.isReady()) {
    Serial.println("not ready");
  } else if (!clockController_.isTimeValid()) {
    Serial.println("ready, time invalid");
  } else {
    Serial.println("ready, time valid");
  }

  Serial.print("  sd: ");
  if (!sdController_.isReady()) {
    Serial.println("not ready");
  } else if (!sdController_.selfTestPassed()) {
    Serial.println("ready, self-test failed");
  } else {
    Serial.println("ready, self-test passed");
  }

  Serial.print("  alarm: ");
  if (!alarmController_.isInitialized()) {
    Serial.println("not initialized");
  } else {
    Serial.print("initialized, configured alarms=");
    Serial.println(alarmController_.alarmCount());
  }
}

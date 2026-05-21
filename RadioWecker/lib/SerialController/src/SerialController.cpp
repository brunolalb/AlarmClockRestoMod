#include "SerialController.h"

#include <WiFi.h>

#include <WebServerController.h>

namespace {
String toCommandKey(const String& source) {
  String key = source;
  key.trim();
  key.toLowerCase();
  return key;
}

String formatBytes(uint64_t bytes) {
  const double kib = 1024.0;
  const double mib = kib * 1024.0;
  const double gib = mib * 1024.0;

  if (bytes >= static_cast<uint64_t>(gib)) {
    return String(bytes / gib, 2) + " GiB";
  }

  if (bytes >= static_cast<uint64_t>(mib)) {
    return String(bytes / mib, 2) + " MiB";
  }

  if (bytes >= static_cast<uint64_t>(kib)) {
    return String(bytes / kib, 2) + " KiB";
  }

  return String(static_cast<unsigned long>(bytes)) + " B";
}
}

SerialController::SerialController(ClockController& clockController,
                                   SdController& sdController,
                                   AlarmController& alarmController,
                                   WebServerController& webServerController)
    : clockController_(clockController),
      sdController_(sdController),
      alarmController_(alarmController),
      webServerController_(webServerController) {}

void SerialController::begin() {
  const String output = "Serial controller ready. Type 'help' for commands.\n";
  Serial.print(output);
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

  String output;
  output.reserve(96);
  output += "Unknown command: ";
  output += rawCommand;
  output += "\nType 'help' for available commands.\n";
  Serial.print(output);
}

void SerialController::printHelp() const {
  String output;
  output.reserve(192);
  output =
      "Commands:\n"
      "  help      - Show this help\n"
      "  ip        - Print local IP address\n"
      "  wifi      - Print WiFi connection details\n"
      "  status    - Print system/module state\n"
      "  modules   - Alias for status\n";
  Serial.print(output);
}

void SerialController::printIp() const {
  String output;
  output.reserve(48);

  if (WiFi.status() != WL_CONNECTED) {
    output = "IP: not connected\n";
  } else {
    output = "IP: ";
    output += WiFi.localIP().toString();
    output += '\n';
  }

  Serial.print(output);
}

void SerialController::printWifi() const {
  String output;
  output.reserve(128);

  if (WiFi.status() != WL_CONNECTED) {
    output = "WiFi: disconnected\n";
    Serial.print(output);
    return;
  }

  output += "WiFi SSID: ";
  output += WiFi.SSID();
  output += '\n';
  output += "WiFi RSSI: ";
  output += String(WiFi.RSSI());
  output += " dBm\n";
  output += "IP: ";
  output += WiFi.localIP().toString();
  output += '\n';

  Serial.print(output);
}

void SerialController::printModuleStatus() const {
  String output;
  output.reserve(320);

  output += "Module status:\n";

  output += "  wifi: ";
  output += (WiFi.status() == WL_CONNECTED ? "connected" : "disconnected");
  output += '\n';

  output += "  webserver: ";
  output += (webServerController_.isStarted() ? "running" : "stopped");
  output += '\n';

  output += "  clock: ";
  if (!clockController_.isReady()) {
    output += "not ready";
  } else if (!clockController_.isTimeValid()) {
    output += "ready, time invalid";
  } else {
    output += "ready, time valid";
  }
  output += ", ntp ";
  output += (clockController_.isNtpSynchronized() ? "sync" : "not sync");
  output += '\n';

  output += "  sd: ";
  if (!sdController_.isReady()) {
    output += "not ready";
  } else {
    output += "ready, free ";
    output += formatBytes(sdController_.availableBytes());
    output += "/";
    output += formatBytes(sdController_.totalBytes());
  }
  output += '\n';

  output += "  alarm: ";
  if (!alarmController_.isInitialized()) {
    output += "not initialized";
  } else {
    output += "initialized, configured alarms=";
    output += String(alarmController_.alarmCount());
  }
  output += '\n';

  Serial.print(output);
}

#include "WebServerController.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>

namespace {
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

WebServerController::WebServerController( AlarmController& alarmController,
                                          ClockController& clockController,
                                          SdController& sdController,
                                          SoundController& soundController,
                                          DisplayManager& displayManager,
                                          GeneralConfigController& generalConfigController,
                                          uint16_t port)
    : alarmController_(alarmController),
      clockController_(clockController),
      sdController_(sdController),
      soundController_(soundController),
      displayManager_(displayManager),
      generalConfigController_(generalConfigController),
      webServer_(port),
      port_(port) {}

bool WebServerController::beginFtpServer() {
  if (ftpStarted_) {
    return true;
  }

  if (!sdController_.isReady()) {
    Serial.println("webserver: FTP not started (SD is not ready)");
    return false;
  }

  ftpServer_.begin(generalConfigController_.ftpUsername().c_str(), generalConfigController_.ftpPassword().c_str());
  ftpStarted_ = true;
  Serial.print("webserver: FTP on port 21 (user: ");
  Serial.print(generalConfigController_.ftpUsername());
  Serial.println(")");

  return true;
}

bool WebServerController::initialize(bool wifi_is_connected) {
  if (!wifi_is_connected) {
    Serial.println("web server: not started: WiFi is not connected");
    return false;
  }

  if (!started_) {
    setupRoutes();
    webServer_.begin();
    started_ = true;
  }

  beginFtpServer();

  Serial.print("web server: running on port ");
  Serial.println(port_);
  return true;
}

void WebServerController::update() {
  if (started_) {
    webServer_.handleClient();
    if (ftpStarted_) {
      ftpServer_.handleFTP();
    }
  }
}

bool WebServerController::isStarted() const {
  return started_;
}

WebServer& WebServerController::server() {
  return webServer_;
}

bool WebServerController::ensureInternalFsMounted() {
  if (internalFsMounted_) {
    return true;
  }

  internalFsMounted_ = LittleFS.begin(true);
  return internalFsMounted_;
}

void WebServerController::serveFile(const char* path, const char* notFoundMessage, const char* contentType) {
  if (!ensureInternalFsMounted()) {
    webServer_.send(500, "text/plain", "LittleFS mount failed");
    return;
  }

  File page = LittleFS.open(path, FILE_READ);
  if (!page) {
    webServer_.send(404, "text/plain", notFoundMessage);
    return;
  }

  webServer_.streamFile(page, contentType);
  page.close();
}

void WebServerController::handleIndexPage() {
  serveFile("/index.html", "index.html not found");
}

void WebServerController::handleAlarmPage() {
  serveFile("/alarm.html", "alarm.html not found");
}

void WebServerController::handleConfigPage() {
  serveFile("/config.html", "config.html not found");
}

void WebServerController::handleUploadPage() {
  serveFile("/upload.html", "upload.html not found");
}

void WebServerController::handleSoundPage() {
  serveFile("/sound.html", "sound.html not found");
}

void WebServerController::handleStatusPage() {
  serveFile("/status.html", "status.html not found");
}

void WebServerController::handleGetStatus() {
  StaticJsonDocument<1024> doc;

  doc["wifiConnected"] = WiFi.status() == WL_CONNECTED;
  doc["wifiSsid"] = WiFi.status() == WL_CONNECTED ? WiFi.SSID() : "";
  doc["wifiRssi"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
  doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";

  doc["webserver"] = isStarted() ? "running" : "stopped";

  String clockStatus;
  if (!clockController_.isReady()) {
    clockStatus = "not ready";
  } else if (!clockController_.isTimeValid()) {
    clockStatus = "ready, time invalid";
  } else {
    clockStatus = "ready, time valid";
  }
  doc["clock"] = clockStatus;
  doc["ntp"] = clockController_.isNtpSynchronized() ? "sync" : "not sync";

  if (!sdController_.isReady()) {
    doc["sd"] = "not ready";
    doc["sdFree"] = "0 B";
    doc["sdTotal"] = "0 B";
  } else {
    doc["sd"] = "ready";
    doc["sdFree"] = formatBytes(sdController_.availableBytes());
    doc["sdTotal"] = formatBytes(sdController_.totalBytes());
  }

  if (!alarmController_.isInitialized()) {
    doc["alarm"] = "not initialized";
    doc["alarmCount"] = 0;
  } else {
    doc["alarm"] = "initialized";
    doc["alarmCount"] = alarmController_.alarmCount();
  }

  doc["soundReady"] = soundController_.isReady();
  doc["soundPlaying"] = soundController_.isPlaying();
  doc["soundTrack"] = soundController_.currentTrack();

  String payload;
  serializeJson(doc, payload);
  webServer_.send(200, "application/json", payload);
}

void WebServerController::handleReboot() {
  if (webServer_.method() != HTTP_POST) {
    webServer_.send(405, "application/json", "{\"ok\":false,\"error\":\"Method not allowed\"}");
    return;
  }

  webServer_.send(200, "application/json", "{\"ok\":true,\"message\":\"Rebooting\"}");
  delay(200);
  ESP.restart();
}

void WebServerController::handleGetConfig() {
  StaticJsonDocument<512> doc;
  generalConfigController_.configToJson(doc);

  String payload;
  serializeJson(doc, payload);
  webServer_.send(200, "application/json", payload);
}

void WebServerController::handleSaveConfig() {
  if (!webServer_.hasArg("plain")) {
    webServer_.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing JSON body\"}");
    return;
  }

  StaticJsonDocument<512> doc;
  const DeserializationError err = deserializeJson(doc, webServer_.arg("plain"));
  if (err) {
    webServer_.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
    return;
  }

  String answer = generalConfigController_.jsonToConfig(doc);
  if (!answer.isEmpty()) {
    webServer_.send(400, "application/json", "{\"ok\":false,\"error\":\"" + answer + "\"}");
    return;
  }

  clockController_.applyTimeConfig( generalConfigController_.timezonePosix(),
                                    generalConfigController_.timeOffsetMinutes());
  displayManager_.setBrightness(generalConfigController_.brightness());

  webServer_.send(200, "application/json", "{\"ok\":true}");
}

void WebServerController::setupRoutes() {
  webServer_.on("/", HTTP_GET, [this]() { handleIndexPage(); });
  webServer_.on("/alarm", HTTP_GET, [this]() { handleAlarmPage(); });
  webServer_.on("/config", HTTP_GET, [this]() { handleConfigPage(); });
  webServer_.on("/upload", HTTP_GET, [this]() { handleUploadPage(); });
  webServer_.on("/sound", HTTP_GET, [this]() { handleSoundPage(); });
  webServer_.on("/status", HTTP_GET, [this]() { handleStatusPage(); });
  webServer_.on("/common.css", HTTP_GET,
                [this]() { serveFile("/common.css", "common.css not found", "text/css"); });
  webServer_.on("/api/status", HTTP_GET, [this]() { handleGetStatus(); });
  webServer_.on("/api/reboot", HTTP_POST, [this]() { handleReboot(); });
  webServer_.on("/api/config", HTTP_GET, [this]() { handleGetConfig(); });
  webServer_.on("/api/config", HTTP_POST, [this]() { handleSaveConfig(); });

  // Alarms related
  webServer_.on("/api/alarm", HTTP_GET, [this]() { alarmController_.handleGetAlarmConfig(webServer_); });
  webServer_.on("/api/alarm", HTTP_POST, [this]() { alarmController_.handleSaveAlarmConfig(webServer_); });

  // sound controller related
  webServer_.on("/api/sound/status", HTTP_GET, [this]() {
    soundController_.handleWebServerCommand(webServer_, SoundController::WebServerCommand::GetStatus);
  });
  webServer_.on("/api/sound/play", HTTP_POST, [this]() {
    soundController_.handleWebServerCommand(webServer_, SoundController::WebServerCommand::Play);
  });
  webServer_.on("/api/sound/radio", HTTP_POST, [this]() {
    soundController_.handleWebServerCommand(webServer_, SoundController::WebServerCommand::PlayRadio);
  });
  webServer_.on("/api/sound/pause", HTTP_POST, [this]() {
    soundController_.handleWebServerCommand(webServer_, SoundController::WebServerCommand::PauseToggle);
  });
  webServer_.on("/api/sound/next", HTTP_POST, [this]() {
    soundController_.handleWebServerCommand(webServer_, SoundController::WebServerCommand::Next);
  });
  webServer_.on("/api/sound/prev", HTTP_POST, [this]() {
    soundController_.handleWebServerCommand(webServer_, SoundController::WebServerCommand::Previous);
  });
  webServer_.on("/api/sound/stop", HTTP_POST, [this]() {
    soundController_.handleWebServerCommand(webServer_, SoundController::WebServerCommand::Stop);
  });
  webServer_.on("/api/sound/volume", HTTP_POST, [this]() {
    soundController_.handleWebServerCommand(webServer_, SoundController::WebServerCommand::SetVolume);
  });

  // sd card related
  webServer_.on("/api/sdcard/listMusicFiles", HTTP_GET, [this]() {
    sdController_.handleListFiles(webServer_,
                                  SoundController::kSupportedFileExtensions,
                                  SoundController::kSupportedFileExtensionCount);
  });
  webServer_.on("/api/sdcard/mkdir", HTTP_POST, [this]() { sdController_.handleCreateFolder(webServer_); });
  webServer_.on("/api/sdcard/delete", HTTP_POST, [this]() { sdController_.handleDeletePath(webServer_); });
  webServer_.on("/api/sdcard/upload", HTTP_POST,
                [this]() { sdController_.handleUploadCompleted(webServer_); },
                [this]() { sdController_.handleUploadFile(webServer_); });

  // not found handler
  webServer_.onNotFound([this]() {
    webServer_.send(404, "application/json", "{\"ok\":false,\"error\":\"Not found\"}");
  });
}

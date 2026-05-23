#include "WebServerController.h"

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <SD.h>
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

WebServerController::WebServerController(AlarmController& alarmController,
                                         ClockController& clockController,
                                         SdController& sdController,
                                         SoundController& soundController,
                                         GeneralConfigController& generalConfigController,
                                         uint16_t port)
    : alarmController_(alarmController),
      clockController_(clockController),
      sdController_(sdController),
      soundController_(soundController),
      generalConfigController_(generalConfigController),
      webServer_(port),
      port_(port) {}

void WebServerController::beginFtpServer() {
  if (ftpStarted_) {
    return;
  }

  if (!sdController_.isReady()) {
    Serial.println("FTP server not started: SD is not ready");
    return;
  }

  ftpServer_.begin(generalConfigController_.ftpUsername().c_str(), generalConfigController_.ftpPassword().c_str());
  ftpStarted_ = true;
  if (ftpStarted_) {
    Serial.print("FTP server running on port 21 (user: ");
    Serial.print(generalConfigController_.ftpUsername());
    Serial.println(")");
  } else {
    Serial.println("FTP server failed to start");
  }
}

void WebServerController::begin(bool enableWebServer) {
  if (!enableWebServer || started_) {
    return;
  }

  setupRoutes();
  webServer_.begin();
  beginFtpServer();
  started_ = true;

  Serial.print("Web server running on port ");
  Serial.println(port_);
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

void WebServerController::handleUploadFile() {
  if (!sdController_.isReady()) {
    return;
  }

  HTTPUpload& upload = webServer_.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    filename.replace("\\", "/");
    const int slash = filename.lastIndexOf('/');
    if (slash >= 0) {
      filename = filename.substring(slash + 1);
    }

    filename.trim();
    if (filename.length() == 0 || filename.indexOf("..") >= 0) {
      currentUploadFilePath_ = "";
      return;
    }

    currentUploadFilePath_ = "/" + filename;
    if (SD.exists(currentUploadFilePath_)) {
      SD.remove(currentUploadFilePath_);
    }
    currentUploadFile_ = SD.open(currentUploadFilePath_, FILE_WRITE);
    if (!currentUploadFile_) {
      currentUploadFilePath_ = "";
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (currentUploadFile_) {
      currentUploadFile_.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (currentUploadFile_) {
      currentUploadFile_.close();
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (currentUploadFile_) {
      currentUploadFile_.close();
    }
    if (currentUploadFilePath_.length() > 0 && SD.exists(currentUploadFilePath_)) {
      SD.remove(currentUploadFilePath_);
    }
    currentUploadFilePath_ = "";
  }
}

void WebServerController::handleUploadCompleted() {
  if (webServer_.method() != HTTP_POST) {
    webServer_.send(405, "application/json", "{\"ok\":false,\"error\":\"Method not allowed\"}");
    return;
  }

  if (!sdController_.isReady()) {
    webServer_.send(503, "application/json", "{\"ok\":false,\"error\":\"SD not ready\"}");
    return;
  }

  if (currentUploadFile_) {
    currentUploadFile_.close();
  }

  const bool ok = currentUploadFilePath_.length() > 1 && SD.exists(currentUploadFilePath_);
  String savedPath = currentUploadFilePath_;
  currentUploadFilePath_ = "";

  if (!ok) {
    webServer_.send(500, "application/json", "{\"ok\":false,\"error\":\"Upload failed\"}");
    return;
  }

  StaticJsonDocument<192> response;
  response["ok"] = true;
  response["path"] = savedPath;
  String payload;
  serializeJson(response, payload);
  webServer_.send(200, "application/json", payload);
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
  webServer_.on("/api/config", HTTP_GET, [this]() { generalConfigController_.handleGetConfig(webServer_); });
  webServer_.on("/api/config", HTTP_POST, [this]() { generalConfigController_.handleSaveConfig(webServer_); });
  webServer_.on("/api/alarm", HTTP_GET, [this]() { alarmController_.handleGetAlarmConfig(webServer_); });
  webServer_.on("/api/alarm", HTTP_POST, [this]() { alarmController_.handleSaveAlarmConfig(webServer_); });
  webServer_.on("/api/music", HTTP_GET, [this]() { soundController_.handleListMusicFiles(webServer_); });
  webServer_.on("/api/sound/status", HTTP_GET, [this]() { soundController_.handleGetStatus(webServer_); });
  webServer_.on("/api/sound/play", HTTP_POST, [this]() { soundController_.handlePlay(webServer_); });
  webServer_.on("/api/sound/radio", HTTP_POST, [this]() { soundController_.handlePlayRadio(webServer_); });
  webServer_.on("/api/sound/stop", HTTP_POST, [this]() { soundController_.handleStop(webServer_); });
  webServer_.on("/api/sound/volume", HTTP_POST, [this]() { soundController_.handleSetVolume(webServer_); });
  webServer_.on("/api/upload", HTTP_POST,
                [this]() { handleUploadCompleted(); },
                [this]() { handleUploadFile(); });
  webServer_.onNotFound([this]() {
    webServer_.send(404, "application/json", "{\"ok\":false,\"error\":\"Not found\"}");
  });
}

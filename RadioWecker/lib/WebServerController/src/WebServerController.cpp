#include "WebServerController.h"

#include <ArduinoJson.h>
#include <ClockController.h>
#include <DisplayManager.h>
#include <LittleFS.h>

namespace {
constexpr const char* kDefaultTimezonePosix = "UTC0";
constexpr int16_t kDefaultTimeOffsetMinutes = 0;
constexpr uint8_t kDefaultBrightness = 7;
}

WebServerController::WebServerController(AlarmController& alarmController,
                                         ClockController& clockController,
                                         DisplayManager& displayManager,
                                         uint16_t port)
    : alarmController_(alarmController),
      clockController_(clockController),
      displayManager_(displayManager),
      webServer_(port),
      port_(port),
      timezonePosix_(kDefaultTimezonePosix),
      timeOffsetMinutes_(kDefaultTimeOffsetMinutes),
      brightness_(kDefaultBrightness) {}

void WebServerController::begin(bool enableWebServer) {
  loadGeneralConfig();

  if (!enableWebServer || started_) {
    return;
  }

  setupRoutes();
  webServer_.begin();
  started_ = true;

  Serial.print("Web server running on port ");
  Serial.println(port_);
}

void WebServerController::update() {
  if (started_) {
    webServer_.handleClient();
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

void WebServerController::serveFile(const char* path, const char* notFoundMessage) {
  if (!ensureInternalFsMounted()) {
    webServer_.send(500, "text/plain", "LittleFS mount failed");
    return;
  }

  File page = LittleFS.open(path, FILE_READ);
  if (!page) {
    webServer_.send(404, "text/plain", notFoundMessage);
    return;
  }

  webServer_.streamFile(page, "text/html");
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

void WebServerController::applyGeneralConfig(const String& timezonePosix, int16_t timeOffsetMinutes, uint8_t brightness) {
  timezonePosix_ = timezonePosix;
  timeOffsetMinutes_ = timeOffsetMinutes;
  brightness_ = brightness > 7 ? 7 : brightness;

  clockController_.applyTimeConfig(timezonePosix_, timeOffsetMinutes_);
  displayManager_.setBrightness(brightness_);
}

void WebServerController::loadGeneralConfig() {
  applyGeneralConfig(kDefaultTimezonePosix, kDefaultTimeOffsetMinutes, kDefaultBrightness);

  if (!ensureInternalFsMounted()) {
    return;
  }

  if (!LittleFS.exists(GENERAL_CONFIG_FILE)) {
    saveGeneralConfig();
    return;
  }

  File file = LittleFS.open(GENERAL_CONFIG_FILE, FILE_READ);
  if (!file) {
    return;
  }

  StaticJsonDocument<384> doc;
  const DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    return;
  }

  const String timezone = doc["timezone"] | kDefaultTimezonePosix;
  const int offset = doc["timeOffsetMinutes"] | kDefaultTimeOffsetMinutes;
  const int brightness = doc["brightness"] | kDefaultBrightness;

  if (timezone.length() == 0 || offset < -720 || offset > 840 || brightness < 0 || brightness > 7) {
    return;
  }

  applyGeneralConfig(timezone, static_cast<int16_t>(offset), static_cast<uint8_t>(brightness));
}

bool WebServerController::saveGeneralConfig() {
  if (!ensureInternalFsMounted()) {
    return false;
  }

  if (LittleFS.exists(GENERAL_CONFIG_FILE) && !LittleFS.remove(GENERAL_CONFIG_FILE)) {
    return false;
  }

  StaticJsonDocument<384> doc;
  doc["timezone"] = timezonePosix_;
  doc["timeOffsetMinutes"] = timeOffsetMinutes_;
  doc["brightness"] = brightness_;

  File file = LittleFS.open(GENERAL_CONFIG_FILE, FILE_WRITE);
  if (!file) {
    return false;
  }

  const bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

void WebServerController::handleGetGeneralConfig() {
  StaticJsonDocument<384> doc;
  doc["timezone"] = timezonePosix_;
  doc["timeOffsetMinutes"] = timeOffsetMinutes_;
  doc["brightness"] = brightness_;

  String payload;
  serializeJson(doc, payload);
  webServer_.send(200, "application/json", payload);
}

void WebServerController::handleSaveGeneralConfig() {
  if (webServer_.method() != HTTP_POST) {
    webServer_.send(405, "application/json", "{\"ok\":false,\"error\":\"Method not allowed\"}");
    return;
  }

  if (!webServer_.hasArg("plain")) {
    webServer_.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing JSON body\"}");
    return;
  }

  StaticJsonDocument<384> doc;
  const DeserializationError err = deserializeJson(doc, webServer_.arg("plain"));
  if (err) {
    webServer_.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
    return;
  }

  const String timezone = doc["timezone"] | "";
  const int offset = doc["timeOffsetMinutes"] | 0;
  const int brightness = doc["brightness"] | -1;

  if (timezone.length() == 0 || offset < -720 || offset > 840 || brightness < 0 || brightness > 7) {
    webServer_.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid configuration\"}");
    return;
  }

  applyGeneralConfig(timezone, static_cast<int16_t>(offset), static_cast<uint8_t>(brightness));
  if (!saveGeneralConfig()) {
    webServer_.send(500, "application/json", "{\"ok\":false,\"error\":\"Failed to persist configuration\"}");
    return;
  }

  StaticJsonDocument<96> response;
  response["ok"] = true;
  String payload;
  serializeJson(response, payload);
  webServer_.send(200, "application/json", payload);
}

void WebServerController::setupRoutes() {
  webServer_.on("/", HTTP_GET, [this]() { handleIndexPage(); });
  webServer_.on("/alarm", HTTP_GET, [this]() { handleAlarmPage(); });
  webServer_.on("/config", HTTP_GET, [this]() { handleConfigPage(); });
  webServer_.on("/api/config", HTTP_GET, [this]() { handleGetGeneralConfig(); });
  webServer_.on("/api/config", HTTP_POST, [this]() { handleSaveGeneralConfig(); });
  webServer_.on("/api/alarm", HTTP_GET, [this]() { alarmController_.handleGetAlarmConfig(webServer_); });
  webServer_.on("/api/alarm", HTTP_POST, [this]() { alarmController_.handleSaveAlarmConfig(webServer_); });
  webServer_.on("/api/music", HTTP_GET, [this]() { alarmController_.handleListMusicFiles(webServer_); });
  webServer_.onNotFound([this]() {
    webServer_.send(404, "application/json", "{\"ok\":false,\"error\":\"Not found\"}");
  });
}

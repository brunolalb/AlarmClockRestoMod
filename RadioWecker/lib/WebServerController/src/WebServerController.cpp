#include "WebServerController.h"

#include <LittleFS.h>

WebServerController::WebServerController(AlarmController& alarmController, uint16_t port)
    : alarmController_(alarmController), webServer_(port), port_(port) {}

void WebServerController::begin(bool enableWebServer) {
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

void WebServerController::setupRoutes() {
  webServer_.on("/", HTTP_GET, [this]() { handleIndexPage(); });
  webServer_.on("/alarm", HTTP_GET, [this]() { handleAlarmPage(); });
  webServer_.on("/api/alarm", HTTP_GET, [this]() { alarmController_.handleGetAlarmConfig(webServer_); });
  webServer_.on("/api/alarm", HTTP_POST, [this]() { alarmController_.handleSaveAlarmConfig(webServer_); });
  webServer_.on("/api/music", HTTP_GET, [this]() { alarmController_.handleListMusicFiles(webServer_); });
  webServer_.onNotFound([this]() {
    webServer_.send(404, "application/json", "{\"ok\":false,\"error\":\"Not found\"}");
  });
}

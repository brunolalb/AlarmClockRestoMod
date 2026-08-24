#include "WiFiController.h"

#include <WiFi.h>


WiFiController::WiFiController( const uint16_t configPortalTimeoutS,
                                const char *hostname)
    : configPortalTimeoutS_(configPortalTimeoutS),
      hostname_(hostname) {}

bool WiFiController::initialize(const char *hostname) {
  if (hostname != nullptr) {
    hostname_ = hostname;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  wifiManager_.setConfigPortalBlocking(false);
  wifiManager_.setConfigPortalTimeout(configPortalTimeoutS_);
  wifiManager_.setHostname(hostname_);

  const bool connected = wifiManager_.autoConnect("RadioWecker-Setup");
  if (connected) {
    Serial.println("WiFi: connected");
    Serial.println(WiFi.localIP());
    connected_ = true;
  } else {
    Serial.println("WiFi: setup started");
  }

  wasConnected_ = connected;
  return true;
}

bool WiFiController::update() {
  // returns true if it's connected
  wifiManager_.process();
  const bool connected = WiFi.status() == WL_CONNECTED;

  if (connected && !wasConnected_) {
    Serial.println("WiFi: reconnected");
    Serial.println(WiFi.localIP());
  } else if (!connected && wasConnected_) {
    Serial.println("WiFi: disconnected");
  }
  connected_ = connected;

  if (!connected) {
    const unsigned long nowMs = millis();
    if (static_cast<long>(nowMs - nextReconnectAttemptMs_) >= 0) {
      WiFi.reconnect();
      nextReconnectAttemptMs_ = nowMs + 10000UL;
    }
  }

  wasConnected_ = connected;
  return connected;
}

bool WiFiController::connected() const {
  return connected_;
}
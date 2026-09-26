#include "WiFiController.h"

#include <WiFi.h>


WiFiController::WiFiController()
  {}

bool WiFiController::initialize(const WifiConfig *default_config) {
  memcpy(&config_, default_config, sizeof(WifiConfig));

  WiFi.mode(WIFI_STA);
  wifiManager_.setConfigPortalBlocking(false);
  wifiManager_.setConfigPortalTimeout(config_.config_portal_timeout_sec);
  wifiManager_.setHostname(config_.hostname.c_str());
  wifiManager_.setWiFiAutoReconnect(true);

  connected_ = wifiManager_.autoConnect((config_.hostname + "-Setup").c_str());
  if (connected_) {
    Serial.println("WiFi: connected");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi: setup started");
  }
  wasConnected_ = connected_;

  _updateTask = std::thread(&WiFiController::updateTask, this);
  return true;
}

void WiFiController::updateTask() {

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  while (1) {
    wifiManager_.process();

    connected_ = (WiFi.status() == WL_CONNECTED);
    if (connected_ && !wasConnected_) {
      Serial.println("WiFi: reconnected");
      Serial.println(WiFi.localIP());
    } else if (!connected_ && wasConnected_) {
      Serial.println("WiFi: disconnected");
    }

    // if (!connected_) {
    //   const unsigned long nowMs = millis();
    //   if (static_cast<long>(nowMs - nextReconnectAttemptMs_) >= 0) {
    //     WiFi.reconnect();
    //     nextReconnectAttemptMs_ = nowMs + 10000UL;
    //   }
    // }

    wasConnected_ = connected_;

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

bool WiFiController::connected() const {
  return connected_;
}

#include "WiFiController.h"

#include <WiFi.h>
#include <esp_pthread.h>


WiFiController::WiFiController()
  {}

bool WiFiController::initialize(const WifiConfig *default_config) {
  
  config_ = *default_config;

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

  esp_pthread_cfg_t cfg = esp_pthread_get_default_config();
  size_t stackSize_orig = cfg.stack_size;
  int core_orig = cfg.pin_to_core;
  cfg.stack_size = 10*1024;
  //cfg.pin_to_core = 1;
  esp_pthread_set_cfg(&cfg);

  _updateTask = std::thread(&WiFiController::updateTask, this);

  cfg.stack_size = stackSize_orig;
  //cfg.pin_to_core = core_orig;
  esp_pthread_set_cfg(&cfg);

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

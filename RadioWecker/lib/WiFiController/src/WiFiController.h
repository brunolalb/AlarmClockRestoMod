#pragma once

#include <Arduino.h>
#include <WiFiManager.h>


class WiFiController {
 public:
  struct WifiConfig {
    String hostname;
    uint16_t config_portal_timeout_sec;
  };
  WiFiController();

  bool initialize(const WifiConfig *default_config);
  bool update();
  bool connected() const;

 private:
  WiFiManager wifiManager_;

  WifiConfig config_;

  bool connected_ = false;
  bool wasConnected_ = false;
  unsigned long nextReconnectAttemptMs_ = 0;
};
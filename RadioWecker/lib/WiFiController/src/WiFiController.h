#pragma once

#include <Arduino.h>
#include <WiFiManager.h>


class WiFiController {
 public:
  explicit WiFiController(const uint16_t configPortalTimeoutS,
                          const char *hostname);

  bool initialize(String hostname = "");
  bool update();
  bool connected() const;

 private:
  WiFiManager wifiManager_;
  bool connected_ = false;
  bool wasConnected_ = false;
  unsigned long nextReconnectAttemptMs_ = 0;
  const uint16_t configPortalTimeoutS_;
  const char *hostname_;
};
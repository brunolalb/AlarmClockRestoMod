#pragma once

#include <Arduino.h>

class ClockController;
class DisplayManager;
class WebServer;

class GeneralConfigController {
 public:
  explicit GeneralConfigController(ClockController& clockController,
                                   DisplayManager& displayManager,
                                   const char* defaultTimezonePosix,
                                   int16_t defaultTimeOffsetMinutes,
                                   uint8_t defaultBrightness);

  void loadFromLittleFs();
  void applyToClock();
  void applyToDisplay();

  void handleGetConfig(WebServer& webServer);
  void handleSaveConfig(WebServer& webServer);

  uint8_t brightness() const;

 private:
  bool ensureInternalFsMounted();
  bool saveToLittleFs();
  bool isValidConfig(const String& timezone, int offsetMinutes, int brightness) const;

  static constexpr const char* GENERAL_CONFIG_FILE = "/general_config.json";

  ClockController& clockController_;
  DisplayManager& displayManager_;

  String timezonePosix_;
  int16_t timeOffsetMinutes_;
  uint8_t brightness_;

  const char* defaultTimezonePosix_;
  int16_t defaultTimeOffsetMinutes_;
  uint8_t defaultBrightness_;

  bool internalFsMounted_ = false;
};

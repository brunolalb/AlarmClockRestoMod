#pragma once
#include <Arduino.h>

#include <HardwareConfig.h>
#include <DisplayManager.h>

// Wifi
static constexpr uint16_t WIFI_CONFIG_PORTAL_TIMEOUT_S = 300;
static constexpr const char* WIFI_DEFAULT_HOSTNAME = "RadioWecker";
static constexpr const char* DEFAULT_FTP_USERNAME = "radiowecker";
static constexpr const char* DEFAULT_FTP_PASSWORD = "radiowecker";

// Display - TM1637
static constexpr uint8_t DISPLAY_BRIGHTNESS_DEFAULT = 1;
static constexpr DisplayManager::SeparatorMode DISPLAY_SEPARATOR_MODE_DEFAULT = DisplayManager::SeparatorMode::Dots;

// RTC - DS3231
static constexpr const char* RTC_NTP_SERVER = "pool.ntp.org";
static constexpr long RTC_NTP_GMT_OFFSET_SECONDS = 0;
static constexpr int RTC_NTP_DAYLIGHT_OFFSET_SECONDS = 0;
static constexpr uint32_t RTC_NTP_SYNC_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;
static constexpr uint32_t RTC_NTP_RETRY_INTERVAL_MS = 60UL * 1000UL;
static constexpr const char* RTC_TIMEZONE_POSIX_DEFAULT = "UTC0";
static constexpr int16_t RTC_TIME_OFFSET_MINUTES_DEFAULT = 0;

// Radio Dial - read via MPR121
static constexpr uint8_t RADIO_BUTTON_DIAL0_CHANNEL = 0; ///E0
static constexpr uint8_t RADIO_BUTTON_DIAL1_CHANNEL = 1; ///E1
static constexpr uint8_t RADIO_BUTTON_DIAL2_CHANNEL = 2; ///E2
static constexpr uint8_t RADIO_BUTTON_DIAL3_CHANNEL = 3; ///E3

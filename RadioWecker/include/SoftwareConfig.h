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

// Display Buttons - read via TM1637
static constexpr uint8_t DISPLAY_BUTTON_SLOW_GRID = 1;
static constexpr uint8_t DISPLAY_BUTTON_SLOW_KEY = 1;
static constexpr uint8_t DISPLAY_BUTTON_FAST_GRID = 1;
static constexpr uint8_t DISPLAY_BUTTON_FAST_KEY = 2;
static constexpr uint8_t DISPLAY_BUTTON_SLEEP_TOP_GRID = 2;
static constexpr uint8_t DISPLAY_BUTTON_SLEEP_TOP_KEY = 2;
static constexpr uint8_t DISPLAY_BUTTON_SIGNAL_GRID = 3;
static constexpr uint8_t DISPLAY_BUTTON_SIGNAL_KEY = 1;
static constexpr uint8_t DISPLAY_BUTTON_TIME_GRID = 3;
static constexpr uint8_t DISPLAY_BUTTON_TIME_KEY = 2;
static constexpr uint8_t DISPLAY_BUTTON_SLEEP_FRONT_GRID = 4;
static constexpr uint8_t DISPLAY_BUTTON_SLEEP_FRONT_KEY = 1;
static constexpr uint8_t DISPLAY_BUTTON_ILLUM_GRID = 4;
static constexpr uint8_t DISPLAY_BUTTON_ILLUM_KEY = 2;

// RTC - DS3231
static constexpr const char* RTC_NTP_SERVER = "pool.ntp.org";
static constexpr long RTC_NTP_GMT_OFFSET_SECONDS = 0;
static constexpr int RTC_NTP_DAYLIGHT_OFFSET_SECONDS = 0;
static constexpr uint32_t RTC_NTP_SYNC_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;
static constexpr uint32_t RTC_NTP_RETRY_INTERVAL_MS = 60UL * 1000UL;
static constexpr const char* RTC_TIMEZONE_POSIX_DEFAULT = "UTC0";
static constexpr int16_t RTC_TIME_OFFSET_MINUTES_DEFAULT = 0;

// Radio Buttons - read via MCP23017
static constexpr uint8_t RADIO_BUTTON_OFF_CHANNEL = 0; //GPA0
static constexpr uint8_t RADIO_BUTTON_ON_CHANNEL = 1; //GPA1
static constexpr uint8_t RADIO_BUTTON_AUTOM_CHANNEL = 2; //GPA2
static constexpr uint8_t RADIO_BUTTON_ALARM_CHANNEL = 3; //GPA3
static constexpr uint8_t RADIO_BUTTON_MW_CHANNEL = 4; //GPA4
static constexpr uint8_t RADIO_BUTTON_FM_CHANNEL = 5; //GPA5
static constexpr uint8_t RADIO_BUTTON_AFC_CHANNEL = 6; //GPA6

// Radio Dial - read via MPR121
static constexpr uint8_t RADIO_BUTTON_DIAL0_CHANNEL = 0; ///E0
static constexpr uint8_t RADIO_BUTTON_DIAL1_CHANNEL = 1; ///E1
static constexpr uint8_t RADIO_BUTTON_DIAL2_CHANNEL = 2; ///E2
static constexpr uint8_t RADIO_BUTTON_DIAL3_CHANNEL = 3; ///E3

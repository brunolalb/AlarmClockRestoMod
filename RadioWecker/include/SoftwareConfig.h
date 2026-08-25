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

// Audio
// - Shutdown mode:
// GND - Shutdown
// OPEN - Stereo Average
#define AUDIO_SHUTDOWN_MODE_OFF()  pinMode(AUDIO_SD, OUTPUT) && digitalWrite(AUDIO_SD, LOW)
#define AUDIO_SHUTDOWN_MODE_ON()   pinMode(AUDIO_SD, OPEN_DRAIN)
// - Gain (MUX - S1S2S3):
// 3dB - 000
// 6dB - 100
// 9dB - 010
// 12dB - 110
// 15dB - 001
static constexpr uint8_t AUDIO_GAIN_3DB = 0b000;
static constexpr uint8_t AUDIO_GAIN_6DB = 0b100;
static constexpr uint8_t AUDIO_GAIN_9DB = 0b010;
static constexpr uint8_t AUDIO_GAIN_12DB = 0b110;
static constexpr uint8_t AUDIO_GAIN_15DB = 0b001;

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

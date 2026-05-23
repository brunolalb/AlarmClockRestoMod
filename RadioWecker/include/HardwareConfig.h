#pragma once

#include <Arduino.h>
#include <DisplayManager.h>

/* Arduino Nano S3
                 ___
D13 - GPIO48 ---|   |--- D12 - GPIO47
3V3          ---|   |--- D11 - GPIO38
B0 - GPIO46  ---|   |--- D10 - GPIO21
A0 - GPIO1   ---|   |--- D9 - GPIO18
A1 - GPIO2   ---|   |--- D8 - GPIO17
A2 - GPIO3   ---|   |--- D7 - GPIO10
A3 - GPIO4   ---|   |--- D6 - GPIO9
A4 - GPIO11  ---|   |--- D5 - GPIO8
A5 - GPIO12  ---|   |--- D4 - GPIO7
A6 - GPIO13  ---|   |--- D3 - GPIO6
A7 - GPIO14  ---|   |--- D2 - GPIO5
VBUS         ---|   |--- GND
B1 - GPIO0   ---|   |--- RST
GND          ---|   |--- D0 - GPIO44
VIN          ---|___|--- D1 - GPIO43

*/

// on board LED
static constexpr uint8_t ONBOARD_LED_PIN = LED_RED;//46;

// Display
static constexpr uint8_t DISPLAY_CLK_PIN = D2;
static constexpr uint8_t DISPLAY_DIO_PIN = D3;
static constexpr uint8_t DISPLAY_BRIGHTNESS_DEFAULT = 1;
static constexpr DisplayManager::SeparatorMode DISPLAY_SEPARATOR_MODE_DEFAULT = DisplayManager::SeparatorMode::Dots;

// SD Card
static constexpr uint8_t SD_SPI_CS_PIN = D10;
static constexpr uint8_t SD_SPI_SCK_PIN = D13;
static constexpr uint8_t SD_SPI_MISO_PIN = D12;
static constexpr uint8_t SD_SPI_MOSI_PIN = D11;
static constexpr uint32_t SD_SPI_FREQUENCY_HZ = 10000000;

// RTC
static constexpr uint8_t RTC_I2C_SDA_PIN = A4;
static constexpr uint8_t RTC_I2C_SCL_PIN = A5;
static constexpr uint8_t RTC_SQW_PIN = A6;
static constexpr uint32_t RTC_I2C_FREQUENCY_HZ = 100000;
static constexpr const char* RTC_NTP_SERVER = "pool.ntp.org";
static constexpr long RTC_NTP_GMT_OFFSET_SECONDS = 0;
static constexpr int RTC_NTP_DAYLIGHT_OFFSET_SECONDS = 0;
static constexpr uint32_t RTC_NTP_SYNC_INTERVAL_MS = 6UL * 60UL * 60UL * 1000UL;
static constexpr uint32_t RTC_NTP_RETRY_INTERVAL_MS = 60UL * 1000UL;
static constexpr const char* RTC_TIMEZONE_POSIX_DEFAULT = "UTC0";
static constexpr int16_t RTC_TIME_OFFSET_MINUTES_DEFAULT = 0;

// I2S - MAX9357A
static constexpr uint8_t I2S_LRCLK_PIN = 3; //A2;
static constexpr uint8_t I2S_BCLK_PIN = 2; //A1;
static constexpr uint8_t I2S_DATA_PIN = 1; //A0;
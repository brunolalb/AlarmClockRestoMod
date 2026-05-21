#pragma once

#include <Arduino.h>

// on board LED
#if defined(LED_BUILTIN)
static constexpr uint8_t ONBOARD_LED_PIN = LED_BUILTIN;
#else
static constexpr uint8_t ONBOARD_LED_PIN = 13;
#endif


// Display
static constexpr uint8_t DISPLAY_CLK_PIN = D2;
static constexpr uint8_t DISPLAY_DIO_PIN = D3;

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


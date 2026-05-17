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
static constexpr uint8_t SD_CS_PIN = D10;

// RTC
static constexpr uint8_t RTC_SQW_PIN = A6;


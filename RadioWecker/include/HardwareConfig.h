#pragma once

#include <Arduino.h>

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

// shared I2C
static constexpr uint8_t I2C_SDA_PIN = A4;
static constexpr uint8_t I2C_SCL_PIN = A5;
static constexpr uint32_t I2C_FREQ_HZ = 100000;

// Display - TM1637
static constexpr uint8_t DISPLAY_CLK_PIN = D4;
static constexpr uint8_t DISPLAY_DIO_PIN = D3;

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

// todo: verify that the channels are correct
static constexpr uint8_t DISPLAY_BUTTON_SLOW_CHANNEL = 0;
static constexpr uint8_t DISPLAY_BUTTON_FAST_CHANNEL = 1;
static constexpr uint8_t DISPLAY_BUTTON_SLEEP_TOP_CHANNEL = 2;
static constexpr uint8_t DISPLAY_BUTTON_SIGNAL_CHANNEL = 3; 
static constexpr uint8_t DISPLAY_BUTTON_TIME_CHANNEL = 4;
static constexpr uint8_t DISPLAY_BUTTON_SLEEP_FRONT_CHANNEL = 5;
static constexpr uint8_t DISPLAY_BUTTON_ILLUM_CHANNEL = 6;

// SD Card
static constexpr uint8_t SD_SPI_CS_PIN = D10;
static constexpr uint8_t SD_SPI_SCK_PIN = D13;
static constexpr uint8_t SD_SPI_MISO_PIN = D12;
static constexpr uint8_t SD_SPI_MOSI_PIN = D11;
static constexpr uint32_t SD_SPI_FREQUENCY_HZ = 10000000;

// RTC
static constexpr uint8_t RTC_I2C_SDA_PIN = I2C_SDA_PIN;
static constexpr uint8_t RTC_I2C_SCL_PIN = I2C_SCL_PIN;
static constexpr uint32_t RTC_I2C_FREQUENCY_HZ = I2C_FREQ_HZ;
static constexpr uint8_t RTC_SQW_PIN = D2;

// Audio - I2S - MAX9357A
static constexpr uint8_t I2S_LRCLK_PIN = 3; //A2;
static constexpr uint8_t I2S_BCLK_PIN = 2; //A1;
static constexpr uint8_t I2S_DATA_PIN = 1; //A0;
static constexpr uint8_t AUDIO_SD = A3;
static constexpr uint8_t AUDIO_GAIN_MUX_S1 = D5; // gain is controlled by HEF4051BT
static constexpr uint8_t AUDIO_GAIN_MUX_S2 = D6;
static constexpr uint8_t AUDIO_GAIN_MUX_S3 = D7;
static constexpr uint8_t AUDIO_VOLUME_POT = A7; // volume potentiometer is 100k
// Audio control
#define AUDIO_SHUTDOWN_MODE_OFF()  do { pinMode(AUDIO_SD, OUTPUT); digitalWrite(AUDIO_SD, LOW); } while (0)
#define AUDIO_SHUTDOWN_MODE_ON()   pinMode(AUDIO_SD, OPEN_DRAIN)
static constexpr uint8_t AUDIO_GAIN_3DB = 0b000;
static constexpr uint8_t AUDIO_GAIN_6DB = 0b100;
static constexpr uint8_t AUDIO_GAIN_9DB = 0b010;
static constexpr uint8_t AUDIO_GAIN_12DB = 0b110;
static constexpr uint8_t AUDIO_GAIN_15DB = 0b001;

// Light
static constexpr uint8_t LIGHT_PWM_PIN = D9;
static constexpr uint16_t LIGHT_PWM_FREQUENCY = 10000; // Hz

// Radio Dial - MPR121
static constexpr uint8_t RADIO_DIAL_I2C_SDA_PIN = I2C_SDA_PIN;
static constexpr uint8_t RADIO_DIAL_I2C_SCL_PIN = I2C_SCL_PIN;
static constexpr uint32_t RADIO_DIAL_I2C_FREQUENCY_HZ = I2C_FREQ_HZ;

// Radio Buttons - MCP23017
static constexpr uint8_t RADIO_BUTTONS_I2C_SDA_PIN = I2C_SDA_PIN;
static constexpr uint8_t RADIO_BUTTONS_I2C_SCL_PIN = I2C_SCL_PIN;
static constexpr uint32_t RADIO_BUTTONS_I2C_FREQUENCY_HZ = I2C_FREQ_HZ;
static constexpr uint8_t RADIO_BUTTONS_I2C_ADDRESS = 0x20;

static constexpr uint8_t RADIO_BUTTON_OFF_CHANNEL = 0; //GPA0
static constexpr uint8_t RADIO_BUTTON_ON_CHANNEL = 1; //GPA1
static constexpr uint8_t RADIO_BUTTON_AUTOM_CHANNEL = 2; //GPA2
static constexpr uint8_t RADIO_BUTTON_ALARM_CHANNEL = 3; //GPA3
static constexpr uint8_t RADIO_BUTTON_MW_CHANNEL = 4; //GPA4
static constexpr uint8_t RADIO_BUTTON_FM_CHANNEL = 5; //GPA5
static constexpr uint8_t RADIO_BUTTON_AFC_CHANNEL = 6; //GPA6

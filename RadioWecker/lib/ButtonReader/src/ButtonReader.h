#pragma once

#include <Arduino.h>
#include <Adafruit_MCP23X17.h>
#include <TM1637.h>
#include <TM16xxButtons.h>

class ButtonReader {
 public:
  struct HardwareConfig {
    uint8_t i2cSdaPin;
    uint8_t i2cSclPin;
    uint32_t i2cFrequencyHz;
    uint8_t i2cAddress;
  };
  struct ButtonsChannels{
    uint8_t RADIO_OFF;
    uint8_t RADIO_ON;
    uint8_t RADIO_AUTOM;
    uint8_t RADIO_ALARM;
    uint8_t RADIO_MW;
    uint8_t RADIO_FM;
    uint8_t RADIO_AFC;
    uint8_t DISPLAY_SLOW;
    uint8_t DISPLAY_FAST;
    uint8_t DISPLAY_SLEEP_TOP;
    uint8_t DISPLAY_SIGNAL;
    uint8_t DISPLAY_TIME;
    uint8_t DISPLAY_SLEEP_FRONT;
    uint8_t DISPLAY_ILLUM;
  };
  enum class Buttons : uint8_t {
    RADIO_OFF = 0,
    RADIO_ON,
    RADIO_AUTOM,
    RADIO_ALARM,
    RADIO_MW,
    RADIO_FM,
    RADIO_AFC,
    DISPLAY_SLOW,
    DISPLAY_FAST,
    DISPLAY_SLEEP_TOP,
    DISPLAY_SIGNAL,
    DISPLAY_TIME,
    DISPLAY_SLEEP_FRONT,
    DISPLAY_ILLUM,
  };

  ButtonReader(const HardwareConfig* hwConfig,
               const ButtonsChannels* buttonsChannels);
  bool initialize(TM1637* display);
  void update();

 private:
    void read_buttons();

    HardwareConfig hwConfig_;
    ButtonsChannels buttonsChannels_;
    TM1637* display_;
    TM16xxButtons* displayButtons_ = nullptr;
    Adafruit_MCP23X17 mcp_;
    ButtonsChannels buttonsStates_;

};
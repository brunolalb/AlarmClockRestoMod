#pragma once

#include <Arduino.h>
#include <Adafruit_MCP23X17.h>
#include <TM1637.h>

#include <Coordinator.h>
#include <thread>

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
  union ButtonsStates {
    struct {
      bool RADIO_OFF : 1;
      bool RADIO_ON : 1;
      bool RADIO_AUTOM : 1;
      bool RADIO_ALARM : 1;
      bool RADIO_MW : 1;
      bool RADIO_FM : 1;
      bool RADIO_AFC : 1;
      bool DISPLAY_SLOW : 1;
      bool DISPLAY_FAST : 1;
      bool DISPLAY_SLEEP_TOP : 1;
      bool DISPLAY_SIGNAL : 1;
      bool DISPLAY_TIME : 1;
      bool DISPLAY_SLEEP_FRONT : 1;
      bool DISPLAY_ILLUM : 1;
      bool RESERVED1 : 1;
      bool RESERVED2 : 1;
    } states;
    uint16_t raw;
  };
  static_assert(sizeof(ButtonsStates) == sizeof(uint16_t), "ButtonsStates must fit in 16 bits");
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
  enum class UpdateInterval : uint32_t {
    FAST = 50,
    NORMAL = 500,
  };

  ButtonReader(const HardwareConfig* hwConfig,
               const ButtonsChannels* buttonsChannels);
  bool initialize(TM1637* display,
                  Coordinator* coordinator);
  void setupUpdateInterval(UpdateInterval interval);
  const ButtonsStates& states() const;

 private:
    void updateTask();
    void read_buttons();

    HardwareConfig hwConfig_;
    ButtonsChannels buttonsChannels_;
    TM1637* display_;
    Adafruit_MCP23X17 mcp_;
    Coordinator* coordinator_;
    ButtonsStates buttonsStates_{};

    std::thread _updateTask;
    uint32_t updateIntervalMs_;

};
#include "ButtonReader.h"

ButtonReader::ButtonReader(const HardwareConfig* hwConfig,
                           const ButtonsChannels* buttonsChannels)
    : hwConfig_(*hwConfig),
      buttonsChannels_(*buttonsChannels) {}

bool ButtonReader::initialize(TM1637* display) {
  display_ = display;
  displayButtons_ = new TM16xxButtons(display_);

  Wire.begin(hwConfig_.i2cSdaPin,
             hwConfig_.i2cSclPin,
             hwConfig_.i2cFrequencyHz);

  if (!mcp_.begin_I2C(hwConfig_.i2cAddress, &Wire)) {
    Serial.println("buttons: MCP23017 initialization failed");
    return false;
  }

  mcp_.pinMode(buttonsChannels_.RADIO_OFF, INPUT);
  mcp_.pinMode(buttonsChannels_.RADIO_ON, INPUT);
  mcp_.pinMode(buttonsChannels_.RADIO_AUTOM, INPUT);
  mcp_.pinMode(buttonsChannels_.RADIO_ALARM, INPUT);
  mcp_.pinMode(buttonsChannels_.RADIO_MW, INPUT);
  mcp_.pinMode(buttonsChannels_.RADIO_FM, INPUT);
  mcp_.pinMode(buttonsChannels_.RADIO_AFC, INPUT);

  return true;
}

void ButtonReader::update() {
  read_buttons();
}

void ButtonReader::read_buttons() {
  buttonsStates_.RADIO_OFF = mcp_.digitalRead(buttonsChannels_.RADIO_OFF);
  buttonsStates_.RADIO_ON = mcp_.digitalRead(buttonsChannels_.RADIO_ON);
  buttonsStates_.RADIO_AUTOM = mcp_.digitalRead(buttonsChannels_.RADIO_AUTOM);
  buttonsStates_.RADIO_ALARM = mcp_.digitalRead(buttonsChannels_.RADIO_ALARM);
  buttonsStates_.RADIO_MW = mcp_.digitalRead(buttonsChannels_.RADIO_MW);
  buttonsStates_.RADIO_FM = mcp_.digitalRead(buttonsChannels_.RADIO_FM);
  buttonsStates_.RADIO_AFC = mcp_.digitalRead(buttonsChannels_.RADIO_AFC);

  buttonsStates_.DISPLAY_SLOW = displayButtons_->isPressed(buttonsChannels_.DISPLAY_SLOW);
  buttonsStates_.DISPLAY_FAST = displayButtons_->isPressed(buttonsChannels_.DISPLAY_FAST);
  buttonsStates_.DISPLAY_SLEEP_TOP = displayButtons_->isPressed(buttonsChannels_.DISPLAY_SLEEP_TOP);
  buttonsStates_.DISPLAY_SIGNAL = displayButtons_->isPressed(buttonsChannels_.DISPLAY_SIGNAL);
  buttonsStates_.DISPLAY_TIME = displayButtons_->isPressed(buttonsChannels_.DISPLAY_TIME);
  buttonsStates_.DISPLAY_SLEEP_FRONT = displayButtons_->isPressed(buttonsChannels_.DISPLAY_SLEEP_FRONT);
  buttonsStates_.DISPLAY_ILLUM = displayButtons_->isPressed(buttonsChannels_.DISPLAY_ILLUM);

  Serial.println("Button states:");
  Serial.print("RADIO_OFF: "); Serial.println(buttonsStates_.RADIO_OFF);
  Serial.print("RADIO_ON: "); Serial.println(buttonsStates_.RADIO_ON);
  Serial.print("RADIO_AUTOM: "); Serial.println(buttonsStates_.RADIO_AUTOM);
  Serial.print("RADIO_ALARM: "); Serial.println(buttonsStates_.RADIO_ALARM);
  Serial.print("RADIO_MW: "); Serial.println(buttonsStates_.RADIO_MW);
  Serial.print("RADIO_FM: "); Serial.println(buttonsStates_.RADIO_FM);
  Serial.print("RADIO_AFC: "); Serial.println(buttonsStates_.RADIO_AFC);
  Serial.print("DISPLAY_SLOW: "); Serial.println(buttonsStates_.DISPLAY_SLOW);
  Serial.print("DISPLAY_FAST: "); Serial.println(buttonsStates_.DISPLAY_FAST);
  Serial.print("DISPLAY_SLEEP_TOP: "); Serial.println(buttonsStates_.DISPLAY_SLEEP_TOP);
  Serial.print("DISPLAY_SIGNAL: "); Serial.println(buttonsStates_.DISPLAY_SIGNAL);
  Serial.print("DISPLAY_TIME: "); Serial.println(buttonsStates_.DISPLAY_TIME);
  Serial.print("DISPLAY_SLEEP_FRONT: "); Serial.println(buttonsStates_.DISPLAY_SLEEP_FRONT);
  Serial.print("DISPLAY_ILLUM: "); Serial.println(buttonsStates_.DISPLAY_ILLUM);
}

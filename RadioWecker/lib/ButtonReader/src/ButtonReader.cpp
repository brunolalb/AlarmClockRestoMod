#include "ButtonReader.h"

#define BUTTONREADER_DEBUG

ButtonReader::ButtonReader(const HardwareConfig* hwConfig,
                           const ButtonsChannels* buttonsChannels)
    : hwConfig_(*hwConfig),
      buttonsChannels_(*buttonsChannels) {}

bool ButtonReader::initialize(TM1637* display) {
  display_ = display;

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
  static uint32_t lastDisplayUpdateMs = 0;

  const uint32_t now = millis();
  if (now - lastDisplayUpdateMs >= 200) {
    read_buttons();
    lastDisplayUpdateMs = now;
  }
}

const ButtonReader::ButtonsStates& ButtonReader::states() const {
  return buttonsStates_;
}

void ButtonReader::read_buttons() {
  ButtonsStates newStates = {};
  static uint32_t lastDisplayButtonsState = 0;
  uint32_t displayButtonsState = display_ ? display_->getButtons() : 0;

  newStates.RADIO_OFF = mcp_.digitalRead(buttonsChannels_.RADIO_OFF);
  newStates.RADIO_ON = mcp_.digitalRead(buttonsChannels_.RADIO_ON);
  newStates.RADIO_AUTOM = mcp_.digitalRead(buttonsChannels_.RADIO_AUTOM);
  newStates.RADIO_ALARM = mcp_.digitalRead(buttonsChannels_.RADIO_ALARM);
  newStates.RADIO_MW = mcp_.digitalRead(buttonsChannels_.RADIO_MW);
  newStates.RADIO_FM = mcp_.digitalRead(buttonsChannels_.RADIO_FM);
  newStates.RADIO_AFC = mcp_.digitalRead(buttonsChannels_.RADIO_AFC);

  newStates.DISPLAY_SLOW = (displayButtonsState & (1 << (buttonsChannels_.DISPLAY_SLOW))) != 0;
  newStates.DISPLAY_FAST = (displayButtonsState & (1 << (buttonsChannels_.DISPLAY_FAST))) != 0;
  newStates.DISPLAY_SLEEP_TOP = (displayButtonsState & (1 << (buttonsChannels_.DISPLAY_SLEEP_TOP))) != 0;
  newStates.DISPLAY_SIGNAL = (displayButtonsState & (1 << (buttonsChannels_.DISPLAY_SIGNAL))) != 0;
  newStates.DISPLAY_TIME = (displayButtonsState & (1 << (buttonsChannels_.DISPLAY_TIME))) != 0;
  newStates.DISPLAY_SLEEP_FRONT = (displayButtonsState & (1 << (buttonsChannels_.DISPLAY_SLEEP_FRONT))) != 0;
  newStates.DISPLAY_ILLUM = (displayButtonsState & (1 << (buttonsChannels_.DISPLAY_ILLUM))) != 0;

#ifdef BUTTONREADER_DEBUG
  if (buttonsStates_.RADIO_OFF != newStates.RADIO_OFF) {
    Serial.println("RADIO_OFF: " + String(newStates.RADIO_OFF));
  }
  if (buttonsStates_.RADIO_ON != newStates.RADIO_ON) {
    Serial.println("RADIO_ON: " + String(newStates.RADIO_ON));
  }
  if (buttonsStates_.RADIO_AUTOM != newStates.RADIO_AUTOM) {
    Serial.println("RADIO_AUTOM: " + String(newStates.RADIO_AUTOM));
  }
  if (buttonsStates_.RADIO_ALARM != newStates.RADIO_ALARM) {
    Serial.println("RADIO_ALARM: " + String(newStates.RADIO_ALARM));
  }
  if (buttonsStates_.RADIO_MW != newStates.RADIO_MW) {
    Serial.println("RADIO_MW: " + String(newStates.RADIO_MW));
  }
  if (buttonsStates_.RADIO_FM != newStates.RADIO_FM) {
    Serial.println("RADIO_FM: " + String(newStates.RADIO_FM));
  }
  if (buttonsStates_.RADIO_AFC != newStates.RADIO_AFC) {
    Serial.println("RADIO_AFC: " + String(newStates.RADIO_AFC));
  }
  if (lastDisplayButtonsState != displayButtonsState) {
    Serial.print("Display btn: ");
    Serial.print(lastDisplayButtonsState);
    Serial.print(" -> ");
    Serial.println(displayButtonsState);
  }
  if (buttonsStates_.DISPLAY_SLOW != newStates.DISPLAY_SLOW) {
    Serial.println("DISPLAY_SLOW: " + String(newStates.DISPLAY_SLOW));
  }
  if (buttonsStates_.DISPLAY_FAST != newStates.DISPLAY_FAST) {
    Serial.println("DISPLAY_FAST: " + String(newStates.DISPLAY_FAST));
  }
  if (buttonsStates_.DISPLAY_SLEEP_TOP != newStates.DISPLAY_SLEEP_TOP) {
    Serial.println("DISPLAY_SLEEP_TOP: " + String(newStates.DISPLAY_SLEEP_TOP));
  }
  if (buttonsStates_.DISPLAY_SIGNAL != newStates.DISPLAY_SIGNAL) {
    Serial.println("DISPLAY_SIGNAL: " + String(newStates.DISPLAY_SIGNAL));
  }
  if (buttonsStates_.DISPLAY_TIME != newStates.DISPLAY_TIME) {
    Serial.println("DISPLAY_TIME: " + String(newStates.DISPLAY_TIME));
  }
  if (buttonsStates_.DISPLAY_SLEEP_FRONT != newStates.DISPLAY_SLEEP_FRONT) {
    Serial.println("DISPLAY_SLEEP_FRONT: " + String(newStates.DISPLAY_SLEEP_FRONT));
  }
  if (buttonsStates_.DISPLAY_ILLUM != newStates.DISPLAY_ILLUM) {
    Serial.println("DISPLAY_ILLUM: " + String(newStates.DISPLAY_ILLUM));
  }
  #endif

  buttonsStates_ = newStates;
  lastDisplayButtonsState = displayButtonsState;
}

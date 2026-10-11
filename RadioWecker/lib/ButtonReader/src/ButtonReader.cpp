#include "ButtonReader.h"

#define BUTTONREADER_DEBUG

ButtonReader::ButtonReader(const HardwareConfig* hwConfig,
                           const ButtonsChannels* buttonsChannels)
    : hwConfig_(*hwConfig),
      buttonsChannels_(*buttonsChannels) {}

bool ButtonReader::initialize(TM1637* display,
                              Coordinator* coordinator) {
  display_ = display;
  coordinator_ = coordinator;

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

  updateIntervalMs_ = 500; // set the update interval for the button reading task
  _updateTask = std::thread(&ButtonReader::updateTask, this);

  return true;
}

void ButtonReader::updateTask() {
  auto now = std::chrono::steady_clock::now();
  while (1) {
    read_buttons();

    std::this_thread::sleep_until(now + std::chrono::milliseconds(updateIntervalMs_));
    now = std::chrono::steady_clock::now();
  }
}

void ButtonReader::setupUpdateInterval(UpdateInterval interval) {
  updateIntervalMs_ = static_cast<uint32_t>(interval);
}

const ButtonReader::ButtonsStates& ButtonReader::states() const {
  return buttonsStates_;
}

void ButtonReader::read_buttons() {
  ButtonsStates newStates = {};
  static uint32_t lastDisplayButtonsState = 0;
  uint32_t displayButtonsState = display_ ? display_->getButtons() : 0;

  newStates.states.RADIO_OFF = mcp_.digitalRead(buttonsChannels_.RADIO_OFF);
  newStates.states.RADIO_ON = mcp_.digitalRead(buttonsChannels_.RADIO_ON);
  newStates.states.RADIO_AUTOM = mcp_.digitalRead(buttonsChannels_.RADIO_AUTOM);
  newStates.states.RADIO_ALARM = mcp_.digitalRead(buttonsChannels_.RADIO_ALARM);
  newStates.states.RADIO_MW = mcp_.digitalRead(buttonsChannels_.RADIO_MW);
  newStates.states.RADIO_FM = mcp_.digitalRead(buttonsChannels_.RADIO_FM);
  newStates.states.RADIO_AFC = mcp_.digitalRead(buttonsChannels_.RADIO_AFC);

  newStates.states.DISPLAY_SLOW = (displayButtonsState & (1 << (buttonsChannels_.DISPLAY_SLOW))) != 0;
  newStates.states.DISPLAY_FAST = (displayButtonsState & (1 << (buttonsChannels_.DISPLAY_FAST))) != 0;
  newStates.states.DISPLAY_SLEEP_TOP = (displayButtonsState & (1 << (buttonsChannels_.DISPLAY_SLEEP_TOP))) != 0;
  newStates.states.DISPLAY_SIGNAL = (displayButtonsState & (1 << (buttonsChannels_.DISPLAY_SIGNAL))) != 0;
  newStates.states.DISPLAY_TIME = (displayButtonsState & (1 << (buttonsChannels_.DISPLAY_TIME))) != 0;
  newStates.states.DISPLAY_SLEEP_FRONT = (displayButtonsState & (1 << (buttonsChannels_.DISPLAY_SLEEP_FRONT))) != 0;
  newStates.states.DISPLAY_ILLUM = (displayButtonsState & (1 << (buttonsChannels_.DISPLAY_ILLUM))) != 0;

#ifdef BUTTONREADER_DEBUG
  if (buttonsStates_.states.RADIO_OFF != newStates.states.RADIO_OFF) {
    Serial.println("RADIO_OFF: " + String(newStates.states.RADIO_OFF));
  }
  if (buttonsStates_.states.RADIO_ON != newStates.states.RADIO_ON) {
    Serial.println("RADIO_ON: " + String(newStates.states.RADIO_ON));
  }
  if (buttonsStates_.states.RADIO_AUTOM != newStates.states.RADIO_AUTOM) {
    Serial.println("RADIO_AUTOM: " + String(newStates.states.RADIO_AUTOM));
  }
  if (buttonsStates_.states.RADIO_ALARM != newStates.states.RADIO_ALARM) {
    Serial.println("RADIO_ALARM: " + String(newStates.states.RADIO_ALARM));
  }
  if (buttonsStates_.states.RADIO_MW != newStates.states.RADIO_MW) {
    Serial.println("RADIO_MW: " + String(newStates.states.RADIO_MW));
  }
  if (buttonsStates_.states.RADIO_FM != newStates.states.RADIO_FM) {
    Serial.println("RADIO_FM: " + String(newStates.states.RADIO_FM));
  }
  if (buttonsStates_.states.RADIO_AFC != newStates.states.RADIO_AFC) {
    Serial.println("RADIO_AFC: " + String(newStates.states.RADIO_AFC));
  }
  if (lastDisplayButtonsState != displayButtonsState) {
    Serial.print("Display btn: ");
    Serial.print(lastDisplayButtonsState);
    Serial.print(" -> ");
    Serial.println(displayButtonsState);
  }
  if (buttonsStates_.states.DISPLAY_SLOW != newStates.states.DISPLAY_SLOW) {
    Serial.println("DISPLAY_SLOW: " + String(newStates.states.DISPLAY_SLOW));
  }
  if (buttonsStates_.states.DISPLAY_FAST != newStates.states.DISPLAY_FAST) {
    Serial.println("DISPLAY_FAST: " + String(newStates.states.DISPLAY_FAST));
  }
  if (buttonsStates_.states.DISPLAY_SLEEP_TOP != newStates.states.DISPLAY_SLEEP_TOP) {
    Serial.println("DISPLAY_SLEEP_TOP: " + String(newStates.states.DISPLAY_SLEEP_TOP));
  }
  if (buttonsStates_.states.DISPLAY_SIGNAL != newStates.states.DISPLAY_SIGNAL) {
    Serial.println("DISPLAY_SIGNAL: " + String(newStates.states.DISPLAY_SIGNAL));
  }
  if (buttonsStates_.states.DISPLAY_TIME != newStates.states.DISPLAY_TIME) {
    Serial.println("DISPLAY_TIME: " + String(newStates.states.DISPLAY_TIME));
  }
  if (buttonsStates_.states.DISPLAY_SLEEP_FRONT != newStates.states.DISPLAY_SLEEP_FRONT) {
    Serial.println("DISPLAY_SLEEP_FRONT: " + String(newStates.states.DISPLAY_SLEEP_FRONT));
  }
  if (buttonsStates_.states.DISPLAY_ILLUM != newStates.states.DISPLAY_ILLUM) {
    Serial.println("DISPLAY_ILLUM: " + String(newStates.states.DISPLAY_ILLUM));
  }
  #endif

  if (buttonsStates_.raw != newStates.raw) {
    Serial.printf("btns: state changed: 0x%04X -> 0x%04X\n", buttonsStates_.raw, newStates.raw);
    coordinator_->postEvent({Coordinator::EventType::ButtonStateChanged, newStates.raw});
  }

  buttonsStates_ = newStates;
  lastDisplayButtonsState = displayButtonsState;
}

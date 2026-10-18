#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

class DisplayManager;
class ClockController;
class GeneralConfigController;
class WebServerController;
class AlarmController;
class ButtonReader;

class Coordinator {
 public:
  enum class State : uint8_t { Startup, Idle, Alarm };

  enum class EventType : uint8_t {
    _ENTER = 0,
    _EXIT,
    StartupDone = 10,
    NewTime,
    NewConfig,
    AlarmTriggered,
    ButtonStateChanged,
  };

  struct Event {
    EventType type;
    int32_t value;
  };

  Coordinator(DisplayManager *display,
              ClockController *clockController,
              GeneralConfigController *generalConfigController,
              WebServerController *webServerController,
              AlarmController *alarmController,
              ButtonReader *buttonReader);

  bool begin();

  // Safe to call from any task.
  bool postEvent(const Event &event, TickType_t timeout = 0);
  // Safe to call from an ISR.
  bool postEventFromISR(const Event &event);

  State state() const { return state_; }

 private:
  static void taskEntry(void *arg);
  void run();
  void handleEvent();
  void transitionTo(State next);

  bool basic_event_handler(const Event &event);

  void state_startup(const Event &event);
  void state_idle(const Event &event);
  void state_alarm(const Event &event);

  volatile State state_;
  QueueHandle_t queue_;
  TaskHandle_t task_;
  DisplayManager *display_;
  ClockController *clockController_;
  GeneralConfigController *generalConfigController_;
  WebServerController *webServerController_;
  AlarmController *alarmController_;
  ButtonReader *buttonReader_;
};

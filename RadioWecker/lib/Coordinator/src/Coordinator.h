#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <DisplayManager.h>

class Coordinator {
 public:
  enum class State : uint8_t { Startup, Idle, Music };

  enum class EventType : uint8_t {
    _ENTER,
    _EXIT,
    StartupDone,
    MusicStart,
    MusicStop,
  };

  struct Event {
    EventType type;
    int32_t value;
  };

  Coordinator(DisplayManager *display);

  bool begin(uint32_t queueLength = 16, uint32_t stackBytes = 4096, UBaseType_t priority = 1);

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

  void state_startup(const Event &event);
  void state_idle(const Event &event);
  void state_music(const Event &event);

  volatile State state_;
  QueueHandle_t queue_;
  TaskHandle_t task_;
  DisplayManager *display_;
};

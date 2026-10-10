#include "Coordinator.h"

#include <DisplayManager.h>
#include <ClockController.h>
#include <GeneralConfigController.h>
#include <WebServerController.h>

#include <SoftwareConfig.h>

Coordinator::Coordinator(DisplayManager *display,
                         ClockController *clockController,
                         GeneralConfigController *generalConfigController,
                         WebServerController *webServerController) : 
    state_(State::Startup), 
    queue_(nullptr), 
    task_(nullptr),
    display_(display),
    clockController_(clockController),
    generalConfigController_(generalConfigController),
    webServerController_(webServerController) {}


bool Coordinator::begin(uint32_t queueLength, uint32_t stackBytes, UBaseType_t priority) {
  queue_ = xQueueCreate(queueLength, sizeof(Event));
  if (!queue_) {
    Serial.println("Coordinator: failed to create queue");
    return false;
  }
  if (xTaskCreate(taskEntry, "Coordinator", stackBytes, this, priority, &task_) != pdPASS) {
    Serial.println("Coordinator: failed to create task");
    return false;
  }
  return true;
}

bool Coordinator::postEvent(const Event &event, TickType_t timeout) {
  return queue_ && xQueueSend(queue_, &event, timeout) == pdTRUE;
}

bool Coordinator::postEventFromISR(const Event &event) {
  if (!queue_) return false;
  BaseType_t woken = pdFALSE;
  bool ok = xQueueSendFromISR(queue_, &event, &woken) == pdTRUE;
  if (woken) portYIELD_FROM_ISR();
  return ok;
}

void Coordinator::taskEntry(void *arg) {
  static_cast<Coordinator *>(arg)->run();
}

void Coordinator::run() {
  postEvent({EventType::_ENTER, 0});
  for (;;) {
    handleEvent();
    // small delay to prevent task starvation
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void Coordinator::handleEvent() {
  Event event;
  if (xQueueReceive(queue_, &event, portMAX_DELAY) == pdTRUE) {
    switch (state_) {
      case State::Startup: state_startup(event); break;
      case State::Idle:    state_idle(event);    break;
      case State::Alarm:   state_alarm(event);   break;
    }
  }
}

void Coordinator::transitionTo(State next) {
  if (next == state_) return;
  postEvent({EventType::_EXIT, 0});
  handleEvent();
  state_ = next;
  postEvent({EventType::_ENTER, 0});
}

bool Coordinator::basic_event_handler(const Event &event) {
    switch (event.type) {
    case EventType::NewTime:
        // Handle new time event
        display_->showTimeHHMM(event.value);
        break;
    case EventType::NewConfig: {
        // Handle new config event
        clockController_->applyTimeConfig(generalConfigController_->timezonePosix(),
                                          generalConfigController_->timeOffsetMinutes());
        display_->setBrightness(generalConfigController_->brightness());
        WebServerController::WebserverConfig webserverCfg = {
            .port = DEFAULT_WEB_SERVER_PORT,
            .ftpUsername = generalConfigController_->ftpUsername(),
            .ftpPassword = generalConfigController_->ftpPassword()
        };
        webServerController_->reconfigure(&webserverCfg);
        break;
      }
    default:
        // Handle other events
        return false;
    }
    return true;
}

void Coordinator::state_startup(const Event &event) {
    switch (event.type) {
    case EventType::_ENTER:
        display_->showTimeHHMM(8888);
        break;
    case EventType::_EXIT:
        // Handle exiting the Startup state
        break;
    case EventType::StartupDone:
        transitionTo(State::Idle);
        break;
    }
}

void Coordinator::state_idle(const Event &event) {
    if (basic_event_handler(event)) return;
    switch (event.type) {
      case EventType::_ENTER:
        // Handle entering the Idle state
        break;
      case EventType::_EXIT:
        // Handle exiting the Idle state
        break;
      case EventType::AlarmTriggered:
        transitionTo(State::Alarm);
        break;
    }
}

void Coordinator::state_alarm(const Event &event) {
    if (basic_event_handler(event)) return;
    switch (event.type) {
      case EventType::_ENTER:
        // Handle entering the Alarm state
        // todo: get alarm file info and start the alarm
        break;
      case EventType::_EXIT:
        // Handle exiting the Alarm state
        break;
        // todo: add handler for buttons
      case EventType::AlarmTriggered:
        transitionTo(State::Idle);
        break;
    }
}

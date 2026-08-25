#include <Arduino.h>

#include <HardwareConfig.h>
#include <SoftwareConfig.h>

#include <AlarmController.h>
#include <ClockController.h>
#include <DisplayManager.h>
#include <GeneralConfigController.h>
#include <OnboardLedController.h>
#include <CLIController.h>
#include <SdController.h>
#include <SoundController.h>
#include <WiFiController.h>
#include <WebServerController.h>


typedef struct modules_ {
  DisplayManager *display;
  OnboardLedController *led;
  ClockController *clock;
  SdController *sd_card;
  AlarmController *alarm;
  SoundController *sound;
  GeneralConfigController *config;
  WebServerController *webserver;
  WiFiController *wifi;
  CLIController *cli;
} Modules;

Modules modules;

void create_modules() {
  modules.led = new OnboardLedController(ONBOARD_LED_PIN);

  modules.sd_card = new SdController( SD_SPI_CS_PIN,
                                      SD_SPI_SCK_PIN,
                                      SD_SPI_MISO_PIN,
                                      SD_SPI_MOSI_PIN,
                                      SD_SPI_FREQUENCY_HZ);

  modules.display = new DisplayManager( DISPLAY_CLK_PIN,
                                        DISPLAY_DIO_PIN,
                                        DISPLAY_SEPARATOR_MODE_DEFAULT);

  modules.clock = new ClockController(RTC_SQW_PIN,
                                      RTC_I2C_SDA_PIN,
                                      RTC_I2C_SCL_PIN,
                                      RTC_I2C_FREQUENCY_HZ);

  modules.alarm = new AlarmController(*modules.sd_card);

  modules.sound = new SoundController(*modules.sd_card,
                                      I2S_BCLK_PIN,
                                      I2S_LRCLK_PIN,
                                      I2S_DATA_PIN);

  modules.config = new GeneralConfigController(*modules.sd_card);

  modules.webserver = new WebServerController(*modules.alarm,
                                              *modules.clock,
                                              *modules.sd_card,
                                              *modules.sound,
                                              *modules.display,
                                              *modules.config);

  modules.wifi = new WiFiController(WIFI_CONFIG_PORTAL_TIMEOUT_S,
                                    WIFI_DEFAULT_HOSTNAME);

  modules.cli = new CLIController(*modules.clock,
                                  *modules.sd_card,
                                  *modules.alarm,
                                  *modules.webserver);
}


void initialize_modules() {
  if (!modules.led->initialize()) {
    Serial.println("main: onboard LED initialization failed");
  }

  if (!modules.sd_card->initialize()) {
    Serial.println("main: SD Card initialization failed");
  }

  GeneralConfigController::ConfigData configData = {
    .hostname = WIFI_DEFAULT_HOSTNAME,
    .timezonePosix = RTC_TIMEZONE_POSIX_DEFAULT,
    .timeOffsetMinutes = RTC_TIME_OFFSET_MINUTES_DEFAULT,
    .brightness = DISPLAY_BRIGHTNESS_DEFAULT,
    .ftpUsername = DEFAULT_FTP_USERNAME,
    .ftpPassword = DEFAULT_FTP_PASSWORD
  };

  if (!modules.config->initialize(&configData)) {
    Serial.println("main: general configuration initialization failed");
  }

  if (!modules.display->initialize(modules.config->brightness())) {
    Serial.println("main: display initialization failed");
  }

  if (!modules.wifi->initialize(modules.config->hostname())) {
    Serial.println("main: WiFi initialization failed");
  }

  ClockController::TimeConfig clockConfig = {
    .ntpServer = RTC_NTP_SERVER,
    .timezonePosix = modules.config->timezonePosix(),
    .timeOffsetMinutes = modules.config->timeOffsetMinutes(),
    .daylightOffsetSeconds = RTC_NTP_DAYLIGHT_OFFSET_SECONDS,
    .ntpSyncIntervalMs = RTC_NTP_SYNC_INTERVAL_MS,
    .ntpRetryIntervalMs = RTC_NTP_RETRY_INTERVAL_MS
  };

  if (!modules.clock->initialize(&clockConfig)) {
    Serial.println("main: clock initialization failed");
  }

  if (!modules.alarm->initialize()) {
    Serial.println("main: alarm initialization failed");
  }

  if (!modules.sound->initialize()) {
    Serial.println("main: sound initialization failed");
  }

  if (!modules.webserver->initialize(modules.wifi->connected())) {
    Serial.println("main: web server initialization failed");
  }

  if (!modules.cli->initialize()) {
    Serial.println("main: CLI initialization failed");
  }

  if (!modules.sd_card->isReady()) {
    modules.display->showSdFailure();
  }
}


void setup() {
  Serial.begin(115200);

  create_modules();

  initialize_modules();

  delay(800);
}


void loop() {
  static uint32_t lastDisplayUpdateMs = 0;
  static bool wifi_was_connected = modules.wifi->connected();

  bool wifi_connected = modules.wifi->update();
  if (wifi_connected && !wifi_was_connected) {
    modules.webserver->initialize(true);
  }
  wifi_was_connected = wifi_connected;

  modules.cli->update();

  modules.webserver->update();
  modules.sound->update();

  const uint32_t now = millis();
  if (now - lastDisplayUpdateMs >= 200) {
    lastDisplayUpdateMs = now;

    if (modules.clock->isReady()) {
      modules.clock->update();

      if (modules.clock->isTimeValid()) {
        modules.display->showTimeHHMM(modules.clock->displayValueHHMM());
      } else {
        modules.display->showRtcFailure();
      }
    } else if (modules.sd_card->isReady()) {
      modules.display->showRtcFailure();
    } else {
      modules.display->showSdFailure();
    }
  }

  modules.led->update();
}

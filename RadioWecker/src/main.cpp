#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

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
  WiFiManager *wifi;
  CLIController *cli;
} Modules;

Modules modules;

bool wifiServicesStarted = false;
bool wifiWasConnected = false;
unsigned long nextWifiReconnectAttemptMs = 0;

void create_modules() {
  modules.led = new OnboardLedController(ONBOARD_LED_PIN);
  modules.wifi = new WiFiManager();

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
                                      RTC_I2C_FREQUENCY_HZ,
                                      RTC_NTP_SERVER,
                                      RTC_NTP_GMT_OFFSET_SECONDS,
                                      RTC_NTP_DAYLIGHT_OFFSET_SECONDS,
                                      RTC_NTP_SYNC_INTERVAL_MS,
                                      RTC_NTP_RETRY_INTERVAL_MS);

  modules.alarm = new AlarmController(*modules.sd_card);

  modules.sound = new SoundController(*modules.sd_card,
                                      I2S_BCLK_PIN,
                                      I2S_LRCLK_PIN,
                                      I2S_DATA_PIN);

  modules.config = new GeneralConfigController( *modules.clock,
                                                *modules.sd_card,
                                                *modules.display,
                                                RTC_TIMEZONE_POSIX_DEFAULT,
                                                RTC_TIME_OFFSET_MINUTES_DEFAULT,
                                                DISPLAY_BRIGHTNESS_DEFAULT);

  modules.webserver = new WebServerController(*modules.alarm,
                                              *modules.clock,
                                              *modules.sd_card,
                                              *modules.sound,
                                              *modules.config);

  modules.cli = new CLIController(*modules.clock,
                                  *modules.sd_card,
                                  *modules.alarm,
                                  *modules.webserver);
}


void initialize_modules() {
  if (!modules.sd_card->initialize()) {
    Serial.println("main: SD Card initialization failed");
  }

  if (!modules.config->initialize()) {
    Serial.println("main: general configuration initialization failed");
  }

  modules.display->begin(modules.config->brightness());
  modules.led->begin();

  modules.config->applyToClock();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  modules.wifi->setConfigPortalBlocking(false);
  modules.wifi->setConfigPortalTimeout(300);
  modules.wifi->setHostname("RadioWecker");
  const bool wifiConnected = modules.wifi->autoConnect("RadioWecker-Setup");
  if (wifiConnected) {
    Serial.println("WiFi connected via tzapu WiFiManager");
    Serial.println(WiFi.localIP());
    wifiServicesStarted = true;
  } else {
    Serial.println("WiFi setup started in non-blocking mode");
  }
  wifiWasConnected = wifiConnected;

  modules.clock->begin();

  if (!modules.sd_card->isReady()) {
    modules.display->showSdFailure();
  }

  modules.alarm->begin();
  modules.sound->begin();

  modules.webserver->begin(wifiServicesStarted);

  modules.cli->begin();
}


void setup() {
  Serial.begin(115200);

  create_modules();

  initialize_modules();

  delay(800);
}


void loop() {
  static uint32_t lastDisplayUpdateMs = 0;

  modules.wifi->process();
  const bool wifiConnected = WiFi.status() == WL_CONNECTED;

  if (wifiConnected && !wifiWasConnected) {
    Serial.println("WiFi connected");
    Serial.println(WiFi.localIP());
    modules.webserver->begin(true);
    wifiServicesStarted = true;
  } else if (!wifiConnected && wifiWasConnected) {
    Serial.println("WiFi disconnected");
  }

  if (!wifiConnected) {
    const unsigned long nowMs = millis();
    if (static_cast<long>(nowMs - nextWifiReconnectAttemptMs) >= 0) {
      WiFi.reconnect();
      nextWifiReconnectAttemptMs = nowMs + 10000UL;
    }
  }
  wifiWasConnected = wifiConnected;

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

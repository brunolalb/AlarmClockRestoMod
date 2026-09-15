#include <Arduino.h>

#include <HardwareConfig.h>
#include <SoftwareConfig.h>

#include <AlarmController.h>
#include <ButtonReader.h>
#include <ClockController.h>
#include <DisplayManager.h>
#include <GeneralConfigController.h>
#include <OnboardLedController.h>
#include <CLIController.h>
#include <SdController.h>
#include <SoundController.h>
#include <WiFiController.h>
#include <WebServerController.h>

//#define DISPLAY_OFF
//#define ONBOARDLED_OFF
//#define SDCARD_OFF
//#define SOUND_OFF
//#define WEBSERVER_OFF
//#define WIRELESS_OFF
//#define CLI_OFF
//#define BUTTONS_OFF
//#define ALARMS_OFF
//#define CLOCK_OFF
#define GENERALCONFIG_OFF

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
  ButtonReader *buttons;
} Modules;

Modules modules;

void create_modules() {
#ifndef ONBOARDLED_OFF
  modules.led = new OnboardLedController(ONBOARD_LED_PIN);
#else
  modules.led = nullptr;
#endif

#ifndef SDCARD_OFF
  modules.sd_card = new SdController( SD_SPI_CS_PIN,
                                      SD_SPI_SCK_PIN,
                                      SD_SPI_MISO_PIN,
                                      SD_SPI_MOSI_PIN,
                                      SD_SPI_FREQUENCY_HZ);
#else
  modules.sd_card = nullptr;
#endif

#ifndef DISPLAY_OFF
  modules.display = new DisplayManager( DISPLAY_CLK_PIN,
                                        DISPLAY_DIO_PIN,
                                        DISPLAY_SEPARATOR_MODE_DEFAULT);
#else
  modules.display = nullptr;
#endif

#ifndef CLOCK_OFF
  modules.clock = new ClockController(RTC_SQW_PIN,
                                      RTC_I2C_SDA_PIN,
                                      RTC_I2C_SCL_PIN,
                                      RTC_I2C_FREQUENCY_HZ);
#else
  modules.clock = nullptr;
#endif

#ifndef ALARMS_OFF
  modules.alarm = new AlarmController(modules.sd_card);
#else
  modules.alarm = nullptr;
#endif

#ifndef SOUND_OFF
  SoundController::HardwareConfig hwConfig = {
    .i2sBclkPin = I2S_BCLK_PIN,
    .i2sLrclkPin = I2S_LRCLK_PIN,
    .i2sDataPin = I2S_DATA_PIN,
    .SDPin = AUDIO_SD,
    .GAINMuxS1Pin = AUDIO_GAIN_MUX_S1,
    .GAINMuxS2Pin = AUDIO_GAIN_MUX_S2,
    .GAINMuxS3Pin = AUDIO_GAIN_MUX_S3,
    .volumePotentiometerPin = AUDIO_VOLUME_POT
  };
  modules.sound = new SoundController(modules.sd_card,
                                      &hwConfig);
#else
  modules.sound = nullptr;
#endif

#ifndef GENERALCONFIG_OFF
  modules.config = new GeneralConfigController(modules.sd_card);
#else
  modules.config = nullptr;
#endif

#ifndef WEBSERVER_OFF
  modules.webserver = new WebServerController(modules.alarm,
                                              modules.clock,
                                              modules.sd_card,
                                              modules.sound,
                                              modules.display,
                                              modules.config);
#else
  modules.webserver = nullptr;
#endif

#ifndef WIRELESS_OFF
  modules.wifi = new WiFiController();
#else
  modules.wifi = nullptr;
#endif

#ifndef BUTTONS_OFF
  ButtonReader::HardwareConfig ButtonsHWConfig = {
    .i2cSdaPin = RADIO_BUTTONS_I2C_SDA_PIN,
    .i2cSclPin = RADIO_BUTTONS_I2C_SCL_PIN,
    .i2cFrequencyHz = RADIO_BUTTONS_I2C_FREQUENCY_HZ,
    .i2cAddress = RADIO_BUTTONS_I2C_ADDRESS
  };
  ButtonReader::ButtonsChannels buttonsChannels = {
    .RADIO_OFF = RADIO_BUTTON_OFF_CHANNEL,
    .RADIO_ON = RADIO_BUTTON_ON_CHANNEL,
    .RADIO_AUTOM = RADIO_BUTTON_AUTOM_CHANNEL,
    .RADIO_ALARM = RADIO_BUTTON_ALARM_CHANNEL,
    .RADIO_MW = RADIO_BUTTON_MW_CHANNEL,
    .RADIO_FM = RADIO_BUTTON_FM_CHANNEL,
    .RADIO_AFC = RADIO_BUTTON_AFC_CHANNEL,
    .DISPLAY_SLOW = DISPLAY_BUTTON_SLOW_CHANNEL,
    .DISPLAY_FAST = DISPLAY_BUTTON_FAST_CHANNEL,
    .DISPLAY_SLEEP_TOP = DISPLAY_BUTTON_SLEEP_TOP_CHANNEL,
    .DISPLAY_SIGNAL = DISPLAY_BUTTON_SIGNAL_CHANNEL,
    .DISPLAY_TIME = DISPLAY_BUTTON_TIME_CHANNEL,
    .DISPLAY_SLEEP_FRONT = DISPLAY_BUTTON_SLEEP_FRONT_CHANNEL,
    .DISPLAY_ILLUM = DISPLAY_BUTTON_ILLUM_CHANNEL
  };
  modules.buttons = new ButtonReader(&ButtonsHWConfig,
                                     &buttonsChannels);
#else
  modules.buttons = nullptr;
#endif

#ifndef CLI_OFF
  modules.cli = new CLIController(modules.clock,
                                  modules.sd_card,
                                  modules.alarm,
                                  modules.webserver,
                                  modules.buttons);
#else
  modules.cli = nullptr;
#endif
}


void initialize_modules() {
#ifndef ONBOARDLED_OFF
  if (!modules.led->initialize()) {
    Serial.println("main: onboard LED initialization failed");
  }
#endif

#ifndef SDCARD_OFF
  if (!modules.sd_card->initialize()) {
    Serial.println("main: SD Card initialization failed");
  }
#endif

#ifndef GENERALCONFIG_OFF
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
#endif

#ifndef DISPLAY_OFF
  if (!modules.display->initialize(modules.config ? modules.config->brightness() : DISPLAY_BRIGHTNESS_DEFAULT)) {
    Serial.println("main: display initialization failed");
  }
#endif

#ifndef WIRELESS_OFF
  WiFiController::WifiConfig wifiConfig = {
    .hostname = modules.config ? modules.config->hostname() : WIFI_DEFAULT_HOSTNAME,
    .config_portal_timeout_sec = WIFI_CONFIG_PORTAL_TIMEOUT_S
  };
  if (!modules.wifi->initialize(&wifiConfig)) {
    Serial.println("main: WiFi initialization failed");
  }
#else
  WiFi.mode(WIFI_OFF);
#endif

#ifndef BUTTONS_OFF
  if (!modules.buttons->initialize(modules.display ? modules.display->display() : nullptr)) {
    Serial.println("main: button reader initialization failed");
  }
#endif

#ifndef CLOCK_OFF
  ClockController::TimeConfig clockConfig = {
    .ntpServer = RTC_NTP_SERVER,
    .timezonePosix = modules.config ? modules.config->timezonePosix() : RTC_TIMEZONE_POSIX_DEFAULT,
    .timeOffsetMinutes = modules.config ? modules.config->timeOffsetMinutes() : RTC_TIME_OFFSET_MINUTES_DEFAULT,
    .daylightOffsetSeconds = RTC_NTP_DAYLIGHT_OFFSET_SECONDS,
    .ntpSyncIntervalMs = RTC_NTP_SYNC_INTERVAL_MS,
    .ntpRetryIntervalMs = RTC_NTP_RETRY_INTERVAL_MS
  };
  if (!modules.clock->initialize(&clockConfig)) {
    Serial.println("main: clock initialization failed");
  }
#endif

#ifndef ALARMS_OFF
  if (!modules.alarm->initialize()) {
    Serial.println("main: alarm initialization failed");
  }
#endif

#ifndef SOUND_OFF
  if (!modules.sound->initialize()) {
    Serial.println("main: sound initialization failed");
  }
#endif

#ifndef WEBSERVER_OFF
  if (!modules.webserver->initialize(modules.wifi ? modules.wifi->connected() : false)) {
    Serial.println("main: web server initialization failed");
  }
#endif

#ifndef CLI_OFF
  if (!modules.cli->initialize()) {
    Serial.println("main: CLI initialization failed");
  }
#endif
}


void setup() {
  Serial.begin(115200);

  create_modules();

  initialize_modules();

  delay(800);
}


void loop() {
#ifndef WEBSERVER_OFF
  static bool wifi_was_connected = modules.wifi ? modules.wifi->connected() : false;
  bool wifi_connected = modules.wifi ? modules.wifi->update() : false;
  if (wifi_connected && !wifi_was_connected) {
    modules.webserver->initialize(true);
  }
  wifi_was_connected = wifi_connected;
#endif

#ifndef CLI_OFF
  modules.cli->update();
#endif

#ifndef WEBSERVER_OFF
  modules.webserver->update();
#endif
#ifndef SOUND_OFF
  modules.sound->update();
#endif
#ifndef BUTTONS_OFF
  modules.buttons->update();
#endif
#ifndef CLOCK_OFF
  modules.clock->update();
#endif
#ifndef DISPLAY_OFF
  modules.display->showTimeHHMM(modules.clock ? modules.clock->displayValueHHMM() : 8888);
#endif

#ifndef ONBOARDLED_OFF
  modules.led->update();
#endif
}

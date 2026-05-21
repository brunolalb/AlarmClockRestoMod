#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include <AlarmController.h>
#include <ClockController.h>
#include <DisplayManager.h>
#include <HardwareConfig.h>
#include <OnboardLedController.h>
#include <SerialController.h>
#include <SdController.h>
#include <WebServerController.h>

DisplayManager displayManager(DISPLAY_CLK_PIN, DISPLAY_DIO_PIN);
OnboardLedController onboardLed(ONBOARD_LED_PIN);
ClockController clockController(RTC_SQW_PIN,
                                RTC_I2C_SDA_PIN,
                                RTC_I2C_SCL_PIN,
                                RTC_I2C_FREQUENCY_HZ,
                                RTC_NTP_SERVER,
                                RTC_NTP_GMT_OFFSET_SECONDS,
                                RTC_NTP_DAYLIGHT_OFFSET_SECONDS,
                                RTC_NTP_SYNC_INTERVAL_MS,
                                RTC_NTP_RETRY_INTERVAL_MS);
SdController sdController(SD_SPI_CS_PIN, SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_FREQUENCY_HZ);
AlarmController alarmController(sdController);
WebServerController webServerController(alarmController, clockController, displayManager);
WiFiManager tzapuWifiManager;
SerialController serialController(clockController, sdController, alarmController, webServerController);

namespace {
constexpr const char* kGeneralConfigPath = "/general_config.json";

struct BootGeneralConfig {
  String timezone = RTC_TIMEZONE_POSIX_DEFAULT;
  int16_t timeOffsetMinutes = RTC_TIME_OFFSET_MINUTES_DEFAULT;
  uint8_t brightness = DISPLAY_BRIGHTNESS_DEFAULT;
};

BootGeneralConfig loadBootGeneralConfig() {
  BootGeneralConfig config;

  if (!LittleFS.begin(true)) {
    return config;
  }

  if (!LittleFS.exists(kGeneralConfigPath)) {
    return config;
  }

  File file = LittleFS.open(kGeneralConfigPath, FILE_READ);
  if (!file) {
    return config;
  }

  StaticJsonDocument<384> doc;
  const DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    return config;
  }

  const String timezone = doc["timezone"] | RTC_TIMEZONE_POSIX_DEFAULT;
  const int offset = doc["timeOffsetMinutes"] | RTC_TIME_OFFSET_MINUTES_DEFAULT;
  const int brightness = doc["brightness"] | DISPLAY_BRIGHTNESS_DEFAULT;

  if (timezone.length() > 0) {
    config.timezone = timezone;
  }

  if (offset >= -720 && offset <= 840) {
    config.timeOffsetMinutes = static_cast<int16_t>(offset);
  }

  if (brightness >= 0 && brightness <= 7) {
    config.brightness = static_cast<uint8_t>(brightness);
  }

  return config;
}
}

void setup() {
  Serial.begin(115200);

  const BootGeneralConfig bootConfig = loadBootGeneralConfig();

  displayManager.begin(bootConfig.brightness);
  onboardLed.begin();

  clockController.applyTimeConfig(bootConfig.timezone, bootConfig.timeOffsetMinutes);

  WiFi.mode(WIFI_STA);
  tzapuWifiManager.setConfigPortalTimeout(300);
  tzapuWifiManager.setHostname("RadioWecker");
  const bool wifiConnected = tzapuWifiManager.autoConnect("RadioWecker-Setup");
  if (wifiConnected) {
    Serial.println("WiFi connected via tzapu WiFiManager");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi setup timed out; continuing without WiFi");
  }

  clockController.begin();

  sdController.initialize();
  if (sdController.isReady()) {
    displayManager.showSdSelfTestResult(sdController.selfTestPassed());
  } else {
    displayManager.showSdFailure();
  }

  alarmController.begin();

  webServerController.begin(wifiConnected);

  serialController.begin();

  delay(800);
}

void loop() {
  serialController.update();

  webServerController.update();

  if (clockController.isReady()) {
    clockController.update();

    if (clockController.isTimeValid()) {
      displayManager.showTimeHHMM(clockController.displayValueHHMM());
    } else {
      displayManager.showRtcFailure();
    }
  } else if (sdController.isReady() && sdController.selfTestPassed()) {
    displayManager.showRtcFailure();
  } else if (!sdController.isReady()) {
    displayManager.showSdFailure();
  } else {
    displayManager.showSdSelfTestResult(false);
  }

  onboardLed.update();

  delay(100);
}

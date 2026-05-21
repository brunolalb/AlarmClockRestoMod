#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include <AlarmController.h>
#include <ClockController.h>
#include <DisplayManager.h>
#include <GeneralConfigController.h>
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
GeneralConfigController generalConfigController(clockController,
                                                sdController,
                                                displayManager,
                                                RTC_TIMEZONE_POSIX_DEFAULT,
                                                RTC_TIME_OFFSET_MINUTES_DEFAULT,
                                                DISPLAY_BRIGHTNESS_DEFAULT);
WebServerController webServerController(alarmController, clockController, sdController, generalConfigController);
WiFiManager tzapuWifiManager;
SerialController serialController(clockController, sdController, alarmController, webServerController);

void setup() {
  Serial.begin(115200);

  sdController.initialize();
  generalConfigController.loadFromStorage();

  displayManager.begin(generalConfigController.brightness());
  onboardLed.begin();

  generalConfigController.applyToClock();

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

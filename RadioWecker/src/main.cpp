#include <Arduino.h>
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
ClockController clockController(RTC_SQW_PIN, RTC_I2C_SDA_PIN, RTC_I2C_SCL_PIN, RTC_I2C_FREQUENCY_HZ);
SdController sdController(SD_SPI_CS_PIN, SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_FREQUENCY_HZ);
AlarmController alarmController(sdController);
WebServerController webServerController(alarmController);
WiFiManager tzapuWifiManager;
SerialController serialController(clockController, sdController, alarmController, webServerController);

void setup() {
  Serial.begin(115200);
  displayManager.begin(7);
  onboardLed.begin();

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
      displayManager.showTimeMMSS(clockController.displayValueMMSS());
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
  onboardLed.pulseActivity();

  delay(100);
}

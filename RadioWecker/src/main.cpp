#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>

#include <AlarmController.h>
#include <ClockController.h>
#include <DisplayManager.h>
#include <HardwareConfig.h>
#include <OnboardLedController.h>
#include <SerialController.h>
#include <SdController.h>

DisplayManager displayManager(DISPLAY_CLK_PIN, DISPLAY_DIO_PIN);
OnboardLedController onboardLed(ONBOARD_LED_PIN);
ClockController clockController(RTC_SQW_PIN);
SdController sdController(SD_CS_PIN);
AlarmController alarmController(sdController);
WiFiManager tzapuWifiManager;
WebServer webServer(80);
bool webServerStarted = false;
SerialController serialController(clockController, sdController, alarmController, webServerStarted);

bool ensureInternalFsMounted() {
  static bool mounted = false;
  if (mounted) {
    return true;
  }

  mounted = LittleFS.begin(true);
  return mounted;
}

void handleAlarmPage() {
  if (!ensureInternalFsMounted()) {
    webServer.send(500, "text/plain", "LittleFS mount failed");
    return;
  }

  File page = LittleFS.open("/alarm.html", FILE_READ);
  if (!page) {
    webServer.send(404, "text/plain", "alarm.html not found");
    return;
  }

  webServer.streamFile(page, "text/html");
  page.close();
}

void handleIndexPage() {
  if (!ensureInternalFsMounted()) {
    webServer.send(500, "text/plain", "LittleFS mount failed");
    return;
  }

  File page = LittleFS.open("/index.html", FILE_READ);
  if (!page) {
    webServer.send(404, "text/plain", "index.html not found");
    return;
  }

  webServer.streamFile(page, "text/html");
  page.close();
}

void setupWebServer() {
  if (webServerStarted) {
    return;
  }

  webServer.on("/", HTTP_GET, handleIndexPage);
  webServer.on("/alarm", HTTP_GET, handleAlarmPage);
  webServer.on("/api/alarm", HTTP_GET, []() { alarmController.handleGetAlarmConfig(webServer); });
  webServer.on("/api/alarm", HTTP_POST, []() { alarmController.handleSaveAlarmConfig(webServer); });
  webServer.on("/api/music", HTTP_GET, []() { alarmController.handleListMusicFiles(webServer); });
  webServer.onNotFound([]() {
    webServer.send(404, "application/json", "{\"ok\":false,\"error\":\"Not found\"}");
  });

  webServer.begin();
  webServerStarted = true;
  Serial.println("Web server running on port 80");
}

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

  if (wifiConnected) {
    setupWebServer();
  }

  serialController.begin();

  delay(800);
}

void loop() {
  serialController.update();

  if (webServerStarted) {
    webServer.handleClient();
  }

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

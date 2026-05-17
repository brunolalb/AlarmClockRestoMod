#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <ClockController.h>
#include <DisplayManager.h>
#include <OnboardLedController.h>
#include <SdController.h>

const uint8_t CLK = D2;
const uint8_t DIO = D3;
const uint8_t SD_CS_PIN = D10;
const uint8_t RTC_SQW_PIN = A6;

#if defined(LED_BUILTIN)
static constexpr uint8_t ONBOARD_LED_PIN = LED_BUILTIN;
#else
static constexpr uint8_t ONBOARD_LED_PIN = 13;
#endif

DisplayManager displayManager(CLK, DIO);
OnboardLedController onboardLed(ONBOARD_LED_PIN);
ClockController clockController;
SdController sdController;
WiFiManager tzapuWifiManager;
static const char* ALARM_FILE = "/alarm_config.json";

WebServer webServer(80);
bool webServerStarted = false;
static constexpr uint8_t MAX_ALARMS = 10;

struct AlarmSettings {
  String time;
  bool days[7];
  uint16_t snoozeMin;
  String soundType;
  String musicPath;
};

AlarmSettings alarmSettings[MAX_ALARMS];
uint8_t alarmCount = 0;

bool ensureInternalFsMounted();

void setDefaultAlarmSettings(AlarmSettings& settings) {
  settings.time = "07:00";
  for (uint8_t i = 0; i < 7; ++i) {
    settings.days[i] = (i >= 1 && i <= 5);
  }
  settings.snoozeMin = 10;
  settings.soundType = "buzzer";
  settings.musicPath = "";
}

bool isValidTimeString(const String& timeValue) {
  if (timeValue.length() != 5 || timeValue.charAt(2) != ':') {
    return false;
  }

  const char h0 = timeValue.charAt(0);
  const char h1 = timeValue.charAt(1);
  const char m0 = timeValue.charAt(3);
  const char m1 = timeValue.charAt(4);
  if (!isDigit(h0) || !isDigit(h1) || !isDigit(m0) || !isDigit(m1)) {
    return false;
  }

  const uint8_t hours = static_cast<uint8_t>((h0 - '0') * 10 + (h1 - '0'));
  const uint8_t minutes = static_cast<uint8_t>((m0 - '0') * 10 + (m1 - '0'));
  return hours < 24 && minutes < 60;
}

bool parseAlarmFromJson(JsonVariantConst alarmVariant, AlarmSettings& outSettings) {
  JsonObjectConst alarmObj = alarmVariant.as<JsonObjectConst>();
  if (alarmObj.isNull()) {
    return false;
  }

  const String timeValue = alarmObj["time"] | "";
  const String soundType = alarmObj["soundType"] | "";
  const String musicPath = alarmObj["musicPath"] | "";
  const uint16_t snoozeMin = alarmObj["snoozeMin"] | 0;
  JsonArrayConst days = alarmObj["days"].as<JsonArrayConst>();

  if (!isValidTimeString(timeValue) ||
      !(soundType == "buzzer" || soundType == "music") ||
      snoozeMin < 1 || snoozeMin > 120 ||
      days.isNull() || days.size() != 7) {
    return false;
  }

  bool hasAtLeastOneDay = false;
  for (uint8_t i = 0; i < 7; ++i) {
    outSettings.days[i] = days[i] | false;
    hasAtLeastOneDay = hasAtLeastOneDay || outSettings.days[i];
  }

  if (!hasAtLeastOneDay) {
    return false;
  }

  if (soundType == "music" && musicPath.length() == 0) {
    return false;
  }

  outSettings.time = timeValue;
  outSettings.snoozeMin = snoozeMin;
  outSettings.soundType = soundType;
  outSettings.musicPath = musicPath;
  return true;
}

void appendAlarmToJsonArray(const AlarmSettings& settings, JsonArray& alarms) {
  JsonObject alarmObj = alarms.createNestedObject();
  alarmObj["time"] = settings.time;
  JsonArray days = alarmObj.createNestedArray("days");
  for (uint8_t i = 0; i < 7; ++i) {
    days.add(settings.days[i]);
  }
  alarmObj["snoozeMin"] = settings.snoozeMin;
  alarmObj["soundType"] = settings.soundType;
  alarmObj["musicPath"] = settings.musicPath;
}

bool saveAlarmSettings(const AlarmSettings* settings, uint8_t count) {
  if (!sdController.isReady()) {
    return false;
  }

  if (SD.exists(ALARM_FILE) && !SD.remove(ALARM_FILE)) {
    return false;
  }

  StaticJsonDocument<4096> doc;
  JsonArray alarms = doc.createNestedArray("alarms");
  for (uint8_t i = 0; i < count; ++i) {
    appendAlarmToJsonArray(settings[i], alarms);
  }

  File file = SD.open(ALARM_FILE, FILE_WRITE);
  if (!file) {
    return false;
  }

  const bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

bool loadAlarmSettings(AlarmSettings* settings, uint8_t& count) {
  if (!sdController.isReady()) {
    return false;
  }

  if (!SD.exists(ALARM_FILE)) {
    setDefaultAlarmSettings(settings[0]);
    count = 1;
    return saveAlarmSettings(settings, count);
  }

  File file = SD.open(ALARM_FILE, FILE_READ);
  if (!file) {
    return false;
  }

  StaticJsonDocument<4096> doc;
  const DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    return false;
  }

  JsonArray alarms = doc["alarms"].as<JsonArray>();
  if (!alarms.isNull()) {
    if (alarms.size() > MAX_ALARMS) {
      return false;
    }

    uint8_t parsedCount = 0;
    for (JsonVariant alarmVariant : alarms) {
      if (!parseAlarmFromJson(alarmVariant, settings[parsedCount])) {
        return false;
      }
      parsedCount++;
    }

    count = parsedCount;
    return true;
  }

  // Backward compatibility: migrate old single-alarm schema.
  if (!parseAlarmFromJson(doc.as<JsonVariantConst>(), settings[0])) {
    return false;
  }

  count = 1;
  return saveAlarmSettings(settings, count);
}

bool saveCurrentAlarmSettings() {
  return saveAlarmSettings(alarmSettings, alarmCount);
}

void sendAlarmConfigJson() {
  StaticJsonDocument<4096> doc;
  doc["maxAlarms"] = MAX_ALARMS;
  JsonArray alarms = doc.createNestedArray("alarms");
  for (uint8_t i = 0; i < alarmCount; ++i) {
    appendAlarmToJsonArray(alarmSettings[i], alarms);
  }

  String payload;
  serializeJson(doc, payload);
  webServer.send(200, "application/json", payload);
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

void handleGetAlarmConfig() {
  sendAlarmConfigJson();
}

void handleSaveAlarmConfig() {
  if (webServer.method() != HTTP_POST) {
    webServer.send(405, "application/json", "{\"ok\":false,\"error\":\"Method not allowed\"}");
    return;
  }

  if (!webServer.hasArg("plain")) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Missing JSON body\"}");
    return;
  }

  StaticJsonDocument<4096> doc;
  const DeserializationError err = deserializeJson(doc, webServer.arg("plain"));
  if (err) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid JSON\"}");
    return;
  }

  JsonArray alarms = doc["alarms"].as<JsonArray>();
  if (alarms.isNull() || alarms.size() > MAX_ALARMS) {
    webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid alarms array\"}");
    return;
  }

  AlarmSettings next[MAX_ALARMS];
  uint8_t nextCount = 0;
  for (JsonVariant alarmVariant : alarms) {
    if (!parseAlarmFromJson(alarmVariant, next[nextCount])) {
      webServer.send(400, "application/json", "{\"ok\":false,\"error\":\"Invalid alarm entry\"}");
      return;
    }
    nextCount++;
  }

  if (!saveAlarmSettings(next, nextCount)) {
    webServer.send(500, "application/json", "{\"ok\":false,\"error\":\"Failed to save alarm\"}");
    return;
  }

  alarmCount = nextCount;
  for (uint8_t i = 0; i < alarmCount; ++i) {
    alarmSettings[i] = next[i];
  }

  StaticJsonDocument<96> response;
  response["ok"] = true;
  response["count"] = alarmCount;
  String payload;
  serializeJson(response, payload);
  webServer.send(200, "application/json", payload);
}

void handleListMusicFiles() {
  DynamicJsonDocument doc(1024);
  JsonArray files = doc.to<JsonArray>();

  if (sdController.isReady()) {
    File root = SD.open("/");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        if (!file.isDirectory()) {
          String name = String(file.name());
          String lower = name;
          lower.toLowerCase();
          if (lower.endsWith(".mp3") || lower.endsWith(".wav") || lower.endsWith(".ogg")) {
            files.add(name);
          }
        }
        file.close();
        file = root.openNextFile();
      }
      root.close();
    }
  }

  String payload;
  serializeJson(files, payload);
  webServer.send(200, "application/json", payload);
}

void setupWebServer() {
  if (webServerStarted) {
    return;
  }

  webServer.on("/", HTTP_GET, handleAlarmPage);
  webServer.on("/api/alarm", HTTP_GET, handleGetAlarmConfig);
  webServer.on("/api/alarm", HTTP_POST, handleSaveAlarmConfig);
  webServer.on("/api/music", HTTP_GET, handleListMusicFiles);
  webServer.onNotFound([]() {
    webServer.send(404, "application/json", "{\"ok\":false,\"error\":\"Not found\"}");
  });

  webServer.begin();
  webServerStarted = true;
  Serial.println("Web server running on port 81");
}

bool ensureInternalFsMounted() {
  static bool mounted = false;
  if (mounted) {
    return true;
  }

  mounted = LittleFS.begin(true);
  return mounted;
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

  clockController.begin(RTC_SQW_PIN);

  sdController.initialize(SD_CS_PIN);
  if (sdController.isReady()) {
    displayManager.showSdSelfTestResult(sdController.selfTestPassed());

    if (!loadAlarmSettings(alarmSettings, alarmCount)) {
      setDefaultAlarmSettings(alarmSettings[0]);
      alarmCount = 1;
      saveCurrentAlarmSettings();
    }
  } else {
    displayManager.showSdFailure();

    setDefaultAlarmSettings(alarmSettings[0]);
    alarmCount = 1;
  }

  if (wifiConnected) {
    setupWebServer();
  }

  delay(800);
}

void loop() {
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
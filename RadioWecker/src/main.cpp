#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <I2C_RTC.h>
#include <AyresWiFiManager.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <TM1637.h>
#include <OnboardLedController.h>

const uint8_t CLK = D2;
const uint8_t DIO = D3;
const uint8_t SD_CS_PIN = D10;
const uint8_t RTC_SQW_PIN = A6;

TM1637 tm(CLK, DIO);

#if defined(LED_BUILTIN)
static constexpr uint8_t ONBOARD_LED_PIN = LED_BUILTIN;
#else
static constexpr uint8_t ONBOARD_LED_PIN = 13;
#endif

OnboardLedController onboardLed(ONBOARD_LED_PIN);
AyresWiFiManager wifiManager;

#define RTC_USE_DS3231 0
#if RTC_USE_DS3231
using RtcChip = DS3231;
#else
using RtcChip = DS1307;
#endif

RtcChip rtc;

struct TimeLocationSettings {
  String city;
  String tz;
};

bool sdReady = false;
bool sdTestPassed = false;
bool rtcReady = false;
bool rtcTimeValid = false;
uint8_t rtcHour = 0;
uint8_t rtcMinute = 0;
uint8_t rtcSecond = 0;
int rtcDisplayedHHMM = 0;
volatile bool rtcSecondTick = false;

TimeLocationSettings timeLocation;
bool ntpTimeSynced = false;
bool wasWifiConnected = false;
unsigned long lastNtpSyncMs = 0;

static const char* NTP1 = "pool.ntp.org";
static const char* NTP2 = "time.google.com";
static const char* LOCATION_FILE = "/time_location.json";
static const char* ALARM_FILE = "/alarm_config.json";
static const uint32_t NTP_SYNC_RETRY_MS = 60000;
static const uint32_t NTP_RESYNC_INTERVAL_MS = 21600000;

WebServer webServer(81);
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
  if (!sdReady) {
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
  if (!sdReady) {
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

  if (sdReady) {
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

void IRAM_ATTR onRtcSecondTick() {
  rtcSecondTick = true;
}

bool initializeClockFromTm(const struct tm& now) {
  if (now.tm_hour < 0 || now.tm_hour > 23 || now.tm_min < 0 || now.tm_min > 59 ||
      now.tm_sec < 0 || now.tm_sec > 59) {
    return false;
  }

  rtcHour = static_cast<uint8_t>(now.tm_hour);
  rtcMinute = static_cast<uint8_t>(now.tm_min);
  rtcSecond = static_cast<uint8_t>(now.tm_sec);
  rtcDisplayedHHMM = rtcHour * 100 + rtcMinute;
  return true;
}

bool initializeRtcTimeFromChip() {
  struct tm now = rtc.getDateTime();
  return initializeClockFromTm(now);
}

bool ensureInternalFsMounted() {
  static bool mounted = false;
  if (mounted) {
    return true;
  }

  mounted = LittleFS.begin(true);
  return mounted;
}

bool saveTimeLocationSettings(const TimeLocationSettings& settings) {
  if (!ensureInternalFsMounted()) {
    return false;
  }

  StaticJsonDocument<192> doc;
  doc["city"] = settings.city;
  doc["tz"] = settings.tz;

  File file = LittleFS.open(LOCATION_FILE, FILE_WRITE);
  if (!file) {
    return false;
  }

  const bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

bool loadTimeLocationSettings(TimeLocationSettings& settings) {
  if (!ensureInternalFsMounted()) {
    return false;
  }

  if (!LittleFS.exists(LOCATION_FILE)) {
    settings.city = "Berlin";
    settings.tz = "CET-1CEST,M3.5.0/2,M10.5.0/3";
    return saveTimeLocationSettings(settings);
  }

  File file = LittleFS.open(LOCATION_FILE, FILE_READ);
  if (!file) {
    return false;
  }

  StaticJsonDocument<192> doc;
  const DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    return false;
  }

  const char* city = doc["city"] | "";
  const char* tz = doc["tz"] | "";
  if (strlen(city) == 0 || strlen(tz) == 0) {
    return false;
  }

  settings.city = city;
  settings.tz = tz;
  return true;
}

bool syncClockFromNtp() {
  if (!wifiManager.isConnected()) {
    return false;
  }

  configTzTime(timeLocation.tz.c_str(), NTP1, NTP2);

  struct tm now;
  if (!getLocalTime(&now, 10000)) {
    return false;
  }

  if (!initializeClockFromTm(now)) {
    return false;
  }

  if (rtcReady) {
    rtc.setTime(static_cast<uint8_t>(now.tm_hour),
                static_cast<uint8_t>(now.tm_min),
                static_cast<uint8_t>(now.tm_sec));
    rtc.setDate(static_cast<uint8_t>(now.tm_mday),
                static_cast<uint8_t>(now.tm_mon + 1),
                static_cast<uint16_t>(now.tm_year + 1900));
    rtc.updateWeek();
  }

  ntpTimeSynced = true;
  lastNtpSyncMs = millis();
  return true;
}

void advanceSoftwareClockOneSecond() {
  rtcSecond++;
  if (rtcSecond >= 60) {
    rtcSecond = 0;
    rtcMinute++;
    if (rtcMinute >= 60) {
      rtcMinute = 0;
      rtcHour = static_cast<uint8_t>((rtcHour + 1) % 24);
    }
  }

  rtcDisplayedHHMM = rtcHour * 100 + rtcMinute;
}

bool runSdSelfTest() {
  const char *testPath = "/sdtest.txt";
  const char *marker = "TM1637_SD_OK";

  if (SD.exists(testPath) && !SD.remove(testPath)) {
    return false;
  }

  File out = SD.open(testPath, FILE_WRITE);
  if (!out) {
    return false;
  }

  out.println(marker);
  out.close();

  File in = SD.open(testPath, FILE_READ);
  if (!in) {
    return false;
  }

  String line = in.readStringUntil('\n');
  in.close();
  line.trim();

  return line == marker;
}

void setup() {
  Serial.begin(115200);
  tm.init();
  tm.setBrightness(7);
  tm.colonOff();
  onboardLed.begin();

  wifiManager.setHostname("radiowecker");
  wifiManager.setAPCredentials("RadioWecker-Setup", "");
  wifiManager.setPortalTimeout(300);
  wifiManager.setProtectedJsons({"wifi.json", "time_location.json"});
  wifiManager.begin();
  wifiManager.run();
  wifiManager.setAutoReconnect(true);

  if (!loadTimeLocationSettings(timeLocation)) {
    timeLocation.city = "Berlin";
    timeLocation.tz = "CET-1CEST,M3.5.0/2,M10.5.0/3";
    saveTimeLocationSettings(timeLocation);
  }

  rtcReady = rtc.begin() != 0;
  if (rtcReady && !rtc.isRunning()) {
    rtc.startClock();
  }

  if (rtcReady) {
    rtcTimeValid = initializeRtcTimeFromChip();

#if RTC_USE_DS3231
    rtc.enableSqwePin();
    rtc.setOutPin(SQW001Hz);
#else
    rtc.setOutPin(SQW001Hz);
#endif

    pinMode(RTC_SQW_PIN, INPUT_PULLUP);
    const int sqwInterrupt = digitalPinToInterrupt(RTC_SQW_PIN);
    if (sqwInterrupt != NOT_AN_INTERRUPT) {
      attachInterrupt(sqwInterrupt, onRtcSecondTick, FALLING);
    }
  }

  sdReady = SD.begin(SD_CS_PIN);
  if (sdReady) {
    sdTestPassed = runSdSelfTest();
    tm.display(sdTestPassed ? "TSTP" : "TSTF");

    if (!loadAlarmSettings(alarmSettings, alarmCount)) {
      setDefaultAlarmSettings(alarmSettings[0]);
      alarmCount = 1;
      saveCurrentAlarmSettings();
    }
  } else {
    tm.display("SDFL");

    setDefaultAlarmSettings(alarmSettings[0]);
    alarmCount = 1;
  }

  delay(800);
}

void loop() {
  wifiManager.update();
  if (webServerStarted) {
    webServer.handleClient();
  }

  const bool wifiConnected = wifiManager.isConnected();
  if (wifiConnected && !webServerStarted) {
    setupWebServer();
  }
  if (!wifiConnected) {
    wifiManager.reintentarConexionSiNecesario();
    ntpTimeSynced = false;
  }

  if (wifiConnected && (!wasWifiConnected || !ntpTimeSynced ||
                        (millis() - lastNtpSyncMs) >= NTP_RESYNC_INTERVAL_MS)) {
    rtcTimeValid = syncClockFromNtp() || rtcTimeValid;
  }
  wasWifiConnected = wifiConnected;

  if (rtcReady) {
    tm.colonOn();

    if (rtcSecondTick) {
      noInterrupts();
      rtcSecondTick = false;
      interrupts();

      if (rtcTimeValid) {
        advanceSoftwareClockOneSecond();
      }
    }

    if (!rtcTimeValid && wifiConnected && (millis() - lastNtpSyncMs) >= NTP_SYNC_RETRY_MS) {
      rtcTimeValid = syncClockFromNtp();
    }

    if (rtcTimeValid) {
      tm.display(rtcDisplayedHHMM, false, true);
    } else {
      tm.colonOff();
      tm.display("RTCF");
    }
  } else if (sdReady && sdTestPassed) {
    tm.colonOff();
    tm.display("RTCF");
  } else if (!sdReady) {
    tm.colonOff();
    tm.display("SDFL");
  } else {
    tm.colonOff();
    tm.display("TSTF");
  }

  onboardLed.update();

  onboardLed.pulseActivity();

  delay(100);
}
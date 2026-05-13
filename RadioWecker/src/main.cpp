#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <I2C_RTC.h>
#include <AyresWiFiManager.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
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
static const uint32_t NTP_SYNC_RETRY_MS = 60000;
static const uint32_t NTP_RESYNC_INTERVAL_MS = 21600000;

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
  } else {
    tm.display("SDFL");
  }

  delay(800);
}

void loop() {
  wifiManager.update();

  const bool wifiConnected = wifiManager.isConnected();
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
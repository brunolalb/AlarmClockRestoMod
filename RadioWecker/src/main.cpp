#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <I2C_RTC.h>
#include <TM1637.h>
#include <OnboardLedController.h>

const uint8_t CLK = D2;
const uint8_t DIO = D3;
const uint8_t SD_CS_PIN = D10;

TM1637 tm(CLK, DIO);

#if defined(LED_BUILTIN)
static constexpr uint8_t ONBOARD_LED_PIN = LED_BUILTIN;
#else
static constexpr uint8_t ONBOARD_LED_PIN = 13;
#endif

OnboardLedController onboardLed(ONBOARD_LED_PIN);

#define RTC_USE_DS3231 0
#if RTC_USE_DS3231
using RtcChip = DS3231;
#else
using RtcChip = DS1307;
#endif

RtcChip rtc;

bool sdReady = false;
bool sdTestPassed = false;
bool rtcReady = false;

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

  rtcReady = rtc.begin() != 0;
  if (rtcReady && !rtc.isRunning()) {
    rtc.startClock();
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
  if (rtcReady) {
    tm.colonOn();
    struct tm now = rtc.getDateTime();
    const int hhmm = now.tm_hour * 100 + now.tm_min;
    tm.display(hhmm, false, true);
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
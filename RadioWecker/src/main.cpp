#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <I2C_RTC.h>
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
bool rtcTimeValid = false;
uint8_t rtcHour = 0;
uint8_t rtcMinute = 0;
uint8_t rtcSecond = 0;
int rtcDisplayedHHMM = 0;
volatile bool rtcSecondTick = false;

void IRAM_ATTR onRtcSecondTick() {
  rtcSecondTick = true;
}

bool initializeRtcTimeFromChip() {
  struct tm now = rtc.getDateTime();

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
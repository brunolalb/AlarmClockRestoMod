#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
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
bool sdReady = false;
bool sdTestPassed = false;

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
  onboardLed.begin();

  sdReady = SD.begin(SD_CS_PIN);
  if (sdReady) {
    sdTestPassed = runSdSelfTest();
    tm.display(sdTestPassed ? "TSTP" : "TSTF");
  } else {
    tm.display("SDFL");
  }

  delay(800);
}

unsigned int counter = 0;

void loop() {
  if (sdReady && sdTestPassed) {
    tm.display(counter, false, true);
  } else if (!sdReady) {
    tm.display("SDFL");
  } else {
    tm.display("TSTF");
  }

  onboardLed.update();

  counter++;
  if (counter == 10000) {
    counter = 0;
  }

  if ((counter % 10) == 0) {
    onboardLed.pulseActivity();
  }

  delay(100);
}
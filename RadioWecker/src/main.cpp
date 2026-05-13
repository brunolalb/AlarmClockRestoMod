#include <Arduino.h>
#include <TM1637.h>
#include <OnboardLedController.h>

const uint8_t CLK = D13;
const uint8_t DIO = D11;

TM1637 tm(CLK, DIO);

#if defined(LED_BUILTIN)
static constexpr uint8_t ONBOARD_LED_PIN = LED_BUILTIN;
#else
static constexpr uint8_t ONBOARD_LED_PIN = 13;
#endif

OnboardLedController onboardLed(ONBOARD_LED_PIN);

void setup() {
  tm.init();
  tm.setBrightness(7);
  onboardLed.begin();
}

unsigned int counter = 0;

void loop() {
  tm.display(counter, false, true);
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
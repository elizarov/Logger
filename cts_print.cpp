#include <avr/sleep.h>

#include "cts_print.h"
#include "Timeout.h"
#include "powersave.h"
#include "Pins.h"

const long INITIAL_PRINT_INTERVAL = 1000L; // wait 1 s before first print to get XBee time to initialize & join
const long PRINT_INTERVAL         = 250L;  // wait 250 ms between prints 
const long WAKEUP_INTERVAL        = 1000L; // wait at most 1s for XBee to wakeup 

Timeout printTimeout(INITIAL_PRINT_INTERVAL);

XBeeMode setMode = XBEE_SLEEP; // initially it is powered, but asleep
XBeeMode curMode = XBEE_SLEEP;

void setupPrint() {
  Serial.begin(57600);  
  pinMode(LED_PIN, OUTPUT);
}

void wakeupXBee() {
  pinMode(SLEEP_RQ_PIN, OUTPUT); 
  digitalWrite(SLEEP_RQ_PIN, 0); // assert SLEEP_RQ to wakeup module
}

void sleepXBee() {
  pinMode(SLEEP_RQ_PIN, INPUT); // release (float) SLEEP_RQ to put XBee to sleep
}

void powerUpXBee() {
  pinMode(POWER_BEE_PIN, INPUT); // release power pin, it will be pulled up to enable power to bee
}

void powerDownXBee() {
  pinMode(POWER_BEE_PIN, OUTPUT); 
  digitalWrite(POWER_BEE_PIN, 0); // write zero to power down
}

void switchCurMode(XBeeMode mode) {
  if (curMode == mode)
    return;
  curMode = mode;  
  switch (mode) {
  case XBEE_OFF:
    powerDownXBee();
    sleepXBee();    
    break;
  case XBEE_SLEEP:
    powerUpXBee();
    sleepXBee();
    break;
  case XBEE_ON:
    powerUpXBee();
    wakeupXBee();
  }  
}

void setXBeeMode(XBeeMode mode) {
  if (setMode == mode) 
    return;
  setMode = mode;  
  switchCurMode(mode);
}

XBeeMode getXBeeMode() {
  return setMode;  
}

void beginPrint() {
  // avoid printing too often (just in case...)
  while (!printTimeout.check())
    sleep_mode(); // just wait...
  printTimeout.reset(PRINT_INTERVAL);
  switchCurMode(XBEE_ON);
  digitalWrite(LED_PIN, 1); // turn on the led before writing
  Timeout wakeupTimeout(WAKEUP_INTERVAL);
  digitalWrite(CTS_PIN, 1); // pull up CTS pin...
  while (digitalRead(CTS_PIN)) {
    if (wakeupTimeout.check())
      break;
    sleep_mode(); // just wait until CTS is asserted (low) or timeout
  }
  digitalWrite(CTS_PIN, 0); // stop pulling up CTS pin
}

void endPrint() {
  Serial.flush(); // flush serial buffer
  digitalWrite(LED_PIN, 0); // turn off the led
  switchCurMode(setMode);
}


#include <avr/sleep.h>
#include <util/delay.h>

#include "cts_print.h"
#include "Timeout.h"
#include "powersave.h"
#include "Pins.h"

const long INITIAL_PRINT_INTERVAL = 1000L; // wait 1 s before first print to get XBee time to initialize & join
const long PRINT_INTERVAL         = 250L;  // wait 250 ms between prints 
const long WAKEUP_INTERVAL        = 1000L; // wait at most 1s for XBee to wakeup 

Timeout printTimeout(INITIAL_PRINT_INTERVAL);

uint8_t lastXBeeError;

// initially not init
XBeeMode setMode = XBEE_NOT_INIT; 
XBeeMode curMode = XBEE_NOT_INIT;

inline void powerXBee(bool on) {
  digitalWrite(POWER_BEE_PIN, on ? 1 : 0); // write zero to power down
}

inline void wakeXBee(bool on) {
  digitalWrite(SLEEP_RQ_PIN, on ? 0 : 1); // low to wakeup
}

void switchMode(XBeeMode mode) {
  if (curMode == mode)
    return;
  curMode = mode;
  powerXBee(mode != XBEE_OFF);
  wakeXBee(mode == XBEE_ON);
}

void setupPrint() {
  Serial.begin(57600);  
  pinMode(LED_PIN, OUTPUT);
  pinMode(SLEEP_RQ_PIN, OUTPUT); 
  pinMode(POWER_BEE_PIN, OUTPUT); 
  pinMode(CTS_PIN, INPUT); 
  // power on setup
  switchMode(XBEE_ON);
}

void setXBeeMode(XBeeMode mode) {
  if (setMode == mode) 
    return;
  setMode = mode;  
  switchMode(mode);
}

bool beginPrint() {
  if (setMode == XBEE_OFF) {
    lastXBeeError = XBEE_ERROR_OFF;
    return false; // don't print at all when set to OFF
  }
  // avoid printing too often (just in case...)
  while (!printTimeout.check())
    sleep_mode(); // just wait...
  printTimeout.reset(PRINT_INTERVAL);
  switchMode(XBEE_ON);
  digitalWrite(CTS_PIN, 1); // pull up CTS pin...
  digitalWrite(LED_PIN, 1); // turn on the led before waiting for CTS
  Timeout wakeupTimeout(WAKEUP_INTERVAL);
  lastXBeeError = 0;
  while (digitalRead(CTS_PIN)) {
    if (wakeupTimeout.check()) {
      lastXBeeError = XBEE_ERROR_NO_CTS;
      break;
    }
    sleep_mode(); // just wait until CTS is asserted (low) or timeout
  }
  digitalWrite(CTS_PIN, 0); // stop pulling up CTS pin
  return true; // print anyway, even if timeout waiting
}

void endPrint() {
  Serial.flush(); // flush serial buffer
  digitalWrite(LED_PIN, 0); // turn off the led
  switchMode(setMode);
}


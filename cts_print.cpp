#include <avr/sleep.h>

#include "cts_print.h"
#include "Timeout.h"
#include "powersave.h"
#include "Pins.h"

const long INITIAL_PRINT_INTERVAL = 1000L; // wait 1 s before first print to get XBee time to initialize & join
const long PRINT_INTERVAL         = 250L;  // wait 250 ms between prints 
const long WAKEUP_INTERVAL        = 1000L; // wait at most 1s for XBee to wakeup 

Timeout printTimeout(INITIAL_PRINT_INTERVAL);

bool forcedEnable = false;

void setupPrint() {
  Serial.begin(57600);  
  pinMode(LED_PIN, OUTPUT);
}

void enableXBee() {
  pinMode(SLEEP_RQ_PIN, OUTPUT); 
  digitalWrite(SLEEP_RQ_PIN, 0); // assert SLEEP_RQ to wakeup module
}

void disableXBee() {
  pinMode(SLEEP_RQ_PIN, INPUT); // release (float) SLEEP_RQ to put XBee to sleep
}

void forceXBeeEnable(bool enable) {
  if (enable && !forcedEnable) {
    enableXBee();    
  } else if (!enable && forcedEnable) {
    disableXBee();
  }
  forcedEnable = enable;
}

void beginPrint() {
  // avoid printing too often (just in case...)
  while (!printTimeout.check())
    sleep_mode(); // just wait...
  printTimeout.reset(PRINT_INTERVAL);
  if (forcedEnable) {
    digitalWrite(LED_PIN, 1); // turn on the led when powered on
  } else  
    enableXBee();
  Timeout wakeupTimeout(WAKEUP_INTERVAL);
  digitalWrite(CTS_PIN, 1); // pull up CTS pin...
  while (digitalRead(CTS_PIN)) {
    if (wakeupTimeout.check())
      break;
    sleep_mode(); // just wait until CTS is asserted (low) or timeout
  }
  digitalWrite(CTS_PIN, 0); // stop pulling up  
}

void endPrint() {
  Serial.flush(); // flush serial buffer
  if (forcedEnable)
    digitalWrite(LED_PIN, 0); // turn off the led
  else  
    disableXBee();
}

void printOn_P(Print& out, PGM_P str) {
  while (1) {
    char ch = pgm_read_byte_near(str++);
    if (!ch)
      return;
    out.write(ch);
  }
}

void print_P(PGM_P str) { 
  printOn_P(Serial, str); 
}


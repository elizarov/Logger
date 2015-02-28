#ifndef CTS_PRINT_H_
#define CTS_PRINT_H_

#include <Arduino.h>
#include <avr/pgmspace.h>

enum XBeeMode {
  XBEE_OFF,
  XBEE_SLEEP,
  XBEE_ON
};

void setupPrint();
void setXBeeMode(XBeeMode mode);
XBeeMode getXBeeMode();

void beginPrint();
void endPrint();

#endif


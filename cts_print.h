#ifndef CTS_PRINT_H_
#define CTS_PRINT_H_

#include <Arduino.h>
#include <avr/pgmspace.h>

enum XBeeMode {
  XBEE_NOT_INIT,
  XBEE_OFF,
  XBEE_SLEEP,
  XBEE_ON
};

const uint8_t XBEE_ERROR_OFF = 0xd1;
const uint8_t XBEE_ERROR_NO_CTS = 0xd2;

extern uint8_t lastXBeeError;

void setupPrint();
void setXBeeMode(XBeeMode mode);

bool beginPrint();
void endPrint();

#endif


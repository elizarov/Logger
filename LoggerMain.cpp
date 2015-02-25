#include <Timeout.h>
#include <FixNum.h>
#include <DS18B20.h>
#include <DS3231.h>
#include <TWIMaster.h>
#include <TWISlave.h>

#include "battery_hal.h"
#include "cts_print.h"
#include "logging.h"
#include "powersave.h"
#include "FanControlData.h"
#include "Pins.h"

#include <avr/sleep.h>

//------- ALL TIME DEFS ------

const unsigned long INITIAL_DUMP_INTERVAL = 2 * Timeout::SECOND;
const unsigned long PERIODIC_DUMP_INTERVAL = 5 * Timeout::SECOND;
const unsigned long PERIODIC_DUMP_SKEW = 1 * Timeout::SECOND;

// must receive data from FanControl in this time or clear last received data
const unsigned long RECEIVE_INTERVAL = 5 * Timeout::SECOND;

//------- HARDWARE ------

DS18B20 ds18b20(DS18B20_PIN);
DS3231 rtc;

//------- STATE ------

DateTimeParser dtParser("[T:", "]");

//------- FAN CONTROL LINK ------

FanControlData data;
FanControlData dataRx; // data that is being received
LoggerData logger;
LoggerData loggerTx; // data that is being transmitted
Timeout receiveTimeout(RECEIVE_INTERVAL);

void twiSlaveCall(uint8_t addr, bool slaveTransmit) {
  if (slaveTransmit) {
    loggerTx = logger;
    TWISlave.use(loggerTx);
  } else
    TWISlave.use(dataRx);
}

void twiSlaveDone(uint8_t size, bool slaveTransmit) {
  if (slaveTransmit)
    return; // don't care
  if (size == sizeof(data) && dataRx.ok()) {
    data = dataRx;
    receiveTimeout.reset(RECEIVE_INTERVAL);
  }
}

void setupTWI() {
  TWISlave.begin(FAN_CONTROL_ADDR);
}

void checkReceive() {
  if (receiveTimeout.check())
    data.clear();
}

//------- DUMP STATE -------

const char HIGHLIGHT_CHAR = '*';

boolean firstDump = true; 
Timeout dump(INITIAL_DUMP_INTERVAL);
char dumpLine[] = "[L:+??.? i+??.? ??% o+??.? ??% v??.?p?r?????b?.??c0;u00000000]* ";

byte indexOf(byte start, char c) {
  for (byte i = start; dumpLine[i] != 0; i++)
    if (dumpLine[i] == c)
      return i;
  return 0;
}

#define POSITIONS0(P0,C2,POS,SIZE)                     \
        byte POS = P0;                                \
      	byte SIZE = indexOf(POS, C2) - POS;

#define POSITIONS(P0,C1,C2,POS,SIZE)                  \
        POSITIONS0(indexOf(P0, C1) + 1,C2,POS,SIZE)

byte highlightPos = indexOf(0, HIGHLIGHT_CHAR);

POSITIONS(    0, ':', ' ', tPos, tSize)
POSITIONS( tPos, 'i', ' ', itPos, itSize)
POSITIONS(itPos, ' ', '%', ihPos, ihSize)
POSITIONS(ihPos, 'o', ' ', otPos, otSize)
POSITIONS(otPos, ' ', '%', ohPos, ohSize)
POSITIONS(ohPos, 'v', 'p', vPos, vSize)
POSITIONS( vPos, 'p', 'r', pPos, pSize)
POSITIONS( pPos, 'r', 'b', rPos, rSize)
POSITIONS( rPos, 'b', 'c', bPos, bSize)
POSITIONS( bPos, 'c', ';', cPos, cSize)
POSITIONS( cPos, 'u', ']', uptimePos, uptimeSize)

const unsigned long DAY_LENGTH_MS = 24 * 60 * 60000L;

unsigned long daystart = 0;
int updays = 0;

inline void prepareDecimal(int x, int pos, uint8_t size, fmt_t fmt = FMT_NONE) {
  formatDecimal(x, &dumpLine[pos], size, fmt);
}

template<typename T, prec_t prec> void prepareDecimal(FixNum<T,prec> x, int pos, uint8_t size, fmt_t fmt = (fmt_t)prec) {
  x.format(&dumpLine[pos], size, fmt);
}

const char DUMP_REGULAR = 0;
const char DUMP_FIRST = HIGHLIGHT_CHAR;

void makeDump(char dumpType) {
  // Write our local temp
  prepareDecimal(ds18b20.getTemp(), tPos, tSize, 1 | FMT_SIGN);

  // Write received data 
  prepareDecimal(data.tempIn, itPos, itSize, 1 | FMT_SIGN);
  prepareDecimal(data.rhIn, ihPos, ihSize, FMT_ZERO);
  prepareDecimal(data.tempOut, otPos, otSize, 1 | FMT_SIGN);
  prepareDecimal(data.rhOut, ohPos, ohSize, FMT_ZERO);

  // other status  
  prepareDecimal(data.voltage, vPos, vSize, 1);
  prepareDecimal(data.fanPower, pPos, pSize);
  prepareDecimal(data.fanRPM, rPos, rSize);
  prepareDecimal(getBatteryVoltage(), bPos, bSize, 2);
  prepareDecimal(getChargeStatus(), cPos, cSize);

  // prepare uptime
  unsigned long time = millis();
  while ((time - daystart) > DAY_LENGTH_MS) {
    daystart += DAY_LENGTH_MS;
    updays++;
  }
  prepareDecimal(updays, uptimePos, uptimeSize - 6, FMT_ZERO);
  time -= daystart;
  time /= 1000; // convert seconds
  prepareDecimal(time % 60, uptimePos + uptimeSize - 2, 2, FMT_ZERO);
  time /= 60; // minutes
  prepareDecimal(time % 60, uptimePos + uptimeSize - 4, 2, FMT_ZERO);
  time /= 60; // hours
  prepareDecimal((int) time, uptimePos + uptimeSize - 6, 2, FMT_ZERO);

  // print
  if (dumpType == DUMP_REGULAR) {
    dumpLine[highlightPos] = 0;
  } else {
    byte i = highlightPos;
    dumpLine[i++] = dumpType;
    if (dumpType != HIGHLIGHT_CHAR)
      dumpLine[i++] = HIGHLIGHT_CHAR; // must end with highlight (signal) char
    dumpLine[i++] = 0; // and the very last char must be zero
  }
  beginPrint();
  Serial.println(dumpLine);
  endPrint();
  // log
  DateTime::Str now = rtc.now().format();
  if (openLog(now)) {
    logFile.print('[');
    logFile.print(now);
    dumpLine[0] = ' ';
    logFile.println(dumpLine);
    dumpLine[0] = '[';
    closeLog();
  }
  // prepare for next dump
  dump.reset(PERIODIC_DUMP_INTERVAL + random(-PERIODIC_DUMP_SKEW, PERIODIC_DUMP_SKEW));
  firstDump = false;
}

inline void dumpState() {
  if (dump.check())
    makeDump(firstDump ? DUMP_FIRST : DUMP_REGULAR);
}

//------- SETUP & MAIN -------

void setup() {
  setupPowerSave();
  setupPrint();
  setupTWI();
  beginPrint();
  print_C("{L:Logger started ");
  print(rtc.now().format());
  print_C("|T}*\r\n");
  endPrint();
}

void loop() {
  forceXBeeEnable(isPoweredOn());
  ds18b20.check();
  checkReceive();
  dumpState();
  while (Serial.available()) {
    char c = Serial.read();
    if (dtParser.parse(c))
      rtc.adjust(dtParser);
  }
  sleep_mode(); // save power
}

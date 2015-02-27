#include <Timeout.h>
#include <FixNum.h>
#include <FmtRef.h>
#include <Uptime.h>
#include <OneWire.h>
#include <DS18B20.h>
#include <DS3231.h>
#include <TWIMaster.h>
#include <TWISlave.h>

#include "battery_hal.h"
#include "cts_print.h"
#include "logging.h"
#include "powersave.h"
#include "LoggerData.h"
#include "Pins.h"

#include <avr/sleep.h>

//------- ALL TIME DEFS ------

const long INITIAL_DUMP_INTERVAL = 2000;
const long PERIODIC_DUMP_INTERVAL = 60000;
const long PERIODIC_DUMP_SKEW = 5000;

// must receive data from FanControl in this time or clear last received data
const long RECEIVE_INTERVAL = 15000;

//------- HARDWARE ------

DS18B20 tempRefSensor(DS18B20_PIN);
DS3231 rtc;

//------- STATE ------

DateTimeParser dtParser("[T:", "]");

//------- LOGGER I2C LINK ------

volatile bool dataInUse; // true when data is being used for output (do not overwrite)
volatile bool dataReceived;
LoggerIn data;  // the most recent Ok received data
LoggerIn dataRxBuf; // data that is being received in TWISlave ISR

volatile bool loggerNeedsUpdate; // true logger needs to be updated (when master is transmitting)
volatile bool loggerUpdating; // true when logger is being updated
LoggerOut logger;
LoggerOut loggerTxBuf; // data that is being transmitted in TWISlave ISR

Timeout receiveTimeout;

// Called from TWISlave ISR
void twiSlaveTransmit(uint8_t doneSize, bool more) {
  if (doneSize == 0) {
    if (!loggerUpdating)
      loggerTxBuf = logger;
    TWISlave.use(loggerTxBuf);
  }
}

// Called from TWISlave ISR
void twiSlaveReceive(uint8_t doneSize, bool more) {
  if (doneSize == 0) {
    TWISlave.use(dataRxBuf);
    loggerNeedsUpdate = true; // update logger asap for master will request it
  } else if (!more && dataRxBuf.ok(doneSize)) {
    if (!dataInUse) {
      data = dataRxBuf;
      dataReceived = true;
    }  
  }
}

inline void setupTWI() {
  TWISlave.begin(LOGGER_ADDR);
}

inline void checkReceive() {
  if (dataReceived) {
    dataReceived = false;
    receiveTimeout.reset(RECEIVE_INTERVAL);
  }
}

inline void checkTransmit() {
  if (!loggerNeedsUpdate)
    return;
  loggerNeedsUpdate = false;  
  loggerUpdating = true;
  logger.tempRef = tempRefSensor.getTemp();
  logger.voltage = getBatteryVoltage();
  logger.charge = getChargeStatus();
  logger.prepare();
  loggerUpdating = false;
}

//------- DUMP STATE -------

const char HIGHLIGHT_CHAR = '*';

boolean firstDump = true; 
Timeout dump(INITIAL_DUMP_INTERVAL);

char dumpPrefix[] = "[L:+??.? ";
char dumpSuffix[] = " b?.??c0;u00000000]* ";

char* highlightPtr = FmtRef::find(dumpSuffix, HIGHLIGHT_CHAR);

FmtRef tRef(dumpPrefix);
FmtRef bRef(dumpSuffix, 'b');
FmtRef cRef(dumpSuffix, 'c');
FmtRef uRef(dumpSuffix, 'u');

const char DUMP_REGULAR = 0;
const char DUMP_FIRST = HIGHLIGHT_CHAR;

void makeDump(char dumpType) {
  // Write our local temp
  tRef = tempRefSensor.getTemp();
  bRef = getBatteryVoltage();
  cRef = (int8_t)getChargeStatus();
  uRef = uptime();

  // print
  if (dumpType == DUMP_REGULAR) {
    *highlightPtr = 0;
  } else {
    char* ptr = highlightPtr;
    *(ptr++) = dumpType;
    if (dumpType != HIGHLIGHT_CHAR)
      *(ptr++) = HIGHLIGHT_CHAR; // must end with highlight (signal) char
    *ptr = 0; // and the very last char must be zero
  }
  beginPrint();
  Serial.print(dumpPrefix);
  dataInUse = true;
  Serial.print(data.buf);
  Serial.println(dumpSuffix);
  endPrint();
  // log
  DateTime::Str now = rtc.now().format();
  if (openLog(now)) {
    dumpPrefix[0] = ' ';
    logFile.print('[');
    logFile.print(now);
    logFile.print(dumpPrefix);
    logFile.print(data.buf);
    logFile.println(dumpSuffix);
    dumpPrefix[0] = '[';
    closeLog();
  }
  if (receiveTimeout.check())
    data.clear(); // clear data after dumping if was not updated for too long
  dataInUse = false;
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
  tempRefSensor.check();
  checkReceive();
  checkTransmit();
  dumpState();
  while (Serial.available()) {
    char c = Serial.read();
    if (dtParser.parse(c))
      rtc.adjust(dtParser);
  }
  sleep_mode(); // save power
}

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

//------- HARDWARE ------

DS18B20 tempRefSensor(DS18B20_PIN);
DS3231 rtc;

//------- STATE ------

DateTimeParser dtParser("[T:", "]");

//------- VOLATEGE TO CONTROL BEE ------

const voltage_t XBEE_ON_V_THRESHOLD = fixnum8_1(40); // 4.0 Volts
const voltage_t XBEE_SLEEP_V_THRESHOLD = fixnum8_1(35); // 3.5 Volts

void updateXBeeMode() {
  voltage_t v = getBatteryVoltage();
  if (v > XBEE_ON_V_THRESHOLD)
    setXBeeMode(XBEE_ON);
  else if (v > XBEE_SLEEP_V_THRESHOLD)
    setXBeeMode(XBEE_SLEEP);
  else  
    setXBeeMode(XBEE_OFF);  
}

//------- LOGGER I2C LINK ------

LoggerIn data;  // the most recent Ok received data
LoggerIn dataRxBuf; // data that is being received in TWISlave ISR

volatile bool loggerNeedsUpdate; // true logger needs to be updated (when master is transmitting)
LoggerOut logger;
LoggerOut loggerTxBuf; // data that is being transmitted in TWISlave ISR

// Called from TWISlave ISR
void twiSlaveTransmit(uint8_t doneSize, bool more) {
  if (doneSize == 0) {
    loggerTxBuf = logger;
    TWISlave.use(loggerTxBuf);
  }
}

// Called from TWISlave ISR
void twiSlaveReceive(uint8_t doneSize, bool more) {
  if (doneSize == 0) {
    TWISlave.use(dataRxBuf);
    loggerNeedsUpdate = true; // update logger asap, for master will request it
  } else if (!more && dataRxBuf.ok(doneSize)) {
    data = dataRxBuf;
  }
}

inline void setupTWI() {
  TWISlave.begin(LOGGER_ADDR);
}

inline void checkTransmit() {
  if (!loggerNeedsUpdate)
    return;
  loggerNeedsUpdate = false;  
  LoggerOut l; // used to fill in new data before copying to logger
  l.tempRef = tempRefSensor.getTemp();
  l.voltage = getBatteryVoltage();
  l.charge = getChargeStatus();
  l.lastError = lastCardError;
  if (l.lastError == 0)
    l.lastError = lastXBeeError;
  l.prepare();
  // disable interrupts and copy to destination
  cli();
  logger = l;
  sei();
}

//------- DUMP STATE -------

const char HIGHLIGHT_CHAR = '*';

bool firstDump = true; 
uint8_t errorReported = 0;
Timeout dump(INITIAL_DUMP_INTERVAL);

char dumpPrefix[] = "[L:+??.? ";
char dumpBuf[MAX_LOGGER_IN_SIZE]; // copy received chars here
char dumpSuffix[] = " b?.??c0;s00u00000000]* ";

char* highlightPtr = FmtRef::find(dumpSuffix, HIGHLIGHT_CHAR);

FmtRef tRef(dumpPrefix);
FmtRef bRef(dumpSuffix, 'b');
FmtRef cRef(dumpSuffix, 'c');
FmtRef sRef(dumpSuffix, 's');
FmtRef uRef(dumpSuffix, 'u');

const char DUMP_REGULAR = 0;
const char DUMP_FIRST = HIGHLIGHT_CHAR;
const char DUMP_ERROR = 'e';

void makeDump(char dumpType) {
  // Write our local temp
  tRef = tempRefSensor.getTemp();
  bRef = getBatteryVoltage();
  cRef = (int8_t)getChargeStatus();
  sRef = (int8_t)lastCardError; // work around the fact that ufixnum at 0 is NAN
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
  // Take a copy of data buffer and clear data (print only fresly received next time)
  // Disable interrupts to read data atomically.
  cli();
  strcpy(dumpBuf, data.buf);
  data.clear();
  sei();
  // print 
  if (beginPrint()) {
    Serial.print(dumpPrefix);
    Serial.print(dumpBuf);
    Serial.println(dumpSuffix);
    endPrint();
  }
  // log
  DateTime now = rtc.now();
  if (now.valid()) {
    DateTime::Str nowStr = now.format();
    if (openLog(nowStr)) {
      dumpPrefix[0] = ' ';
      logFile.print('[');
      logFile.print(nowStr);
      logFile.print(dumpPrefix);
      logFile.print(dumpBuf);
      logFile.println(dumpSuffix);
      dumpPrefix[0] = '[';
      closeLog();
    } 
  }
  // prepare for next dump
  dump.reset(PERIODIC_DUMP_INTERVAL + random(-PERIODIC_DUMP_SKEW, PERIODIC_DUMP_SKEW));
  firstDump = false;
}

inline void dumpState() {
  if (dump.check()) {
    if (firstDump)
      makeDump(DUMP_FIRST);
    else if (errorReported != lastCardError) {
      errorReported = lastCardError;
      makeDump(DUMP_ERROR);
    } else {
      makeDump(DUMP_REGULAR);
    }
  }
}

//------- SETUP & MAIN -------

void setup() {
  setupPowerSave();
  setupPrint();
  setupLog();
  setupTWI();
  if (beginPrint()) {
    Serial.print(F("{L:Logger started "));
    Serial.print(rtc.now().format());
    Serial.println(F("}*"));
    Serial.println(F("<L:T>"));
    endPrint();
  }
}

void loop() {
  updateXBeeMode();
  tempRefSensor.check();
  checkTransmit();
  dumpState();
  while (Serial.available()) {
    char c = Serial.read();
    if (dtParser.parse(c))
      rtc.adjust(dtParser);
  }
  sleep_mode(); // save power
}

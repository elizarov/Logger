#include "logging.h"
#include "powersave.h"
#include "Pins.h"

// It seems that card takes a while to function after reboot, so we should not power cyccle it and
// any error, but only if there are many errors in a row
const uint8_t REBOOT_CARD_ERROR_LIMIT = 3; // reboot card after n errors

char name[] = "YY-MM-DD.LOG";

SdCard card;
Fat16 logFile;
uint8_t cardErrorCnt;
uint8_t lastCardError;
bool cardInit;
bool fatInit;
uint8_t saveSPCR;

void done() {
  // normally, keep card powered on (it will sleep)
  saveSPCR = SPCR;
  SPCR = 0; // disable SPI
  power_spi_disable(); // power off SPI
}

void setupLog() {
  pinMode(TF_EN_PIN, OUTPUT);
  digitalWrite(TF_EN_PIN, 0); // power off card (power cycle card to reset)
}

bool error(uint8_t def) {
  lastCardError = card.errorCode == 0 ? def : card.errorCode;
  if (++cardErrorCnt >= REBOOT_CARD_ERROR_LIMIT) {
    cardErrorCnt = 0;
    digitalWrite(TF_EN_PIN, 0); // power cycle card to reset it
    cardInit = false;
    fatInit = false;
  }
  done();
  return false;
}

// Expects YY-MM-DD... string in date (reads first 8 chars)
bool openLog(const char* date) {
  memcpy(&name, date, 8);

  digitalWrite(TF_EN_PIN, 1); // power ON card 
  power_spi_enable(); // power ON SPI
  SPCR = saveSPCR; // restore SPI state

  // initialize card
  if (!cardInit) {     
    if (!card.begin(TF_CS_PIN, SPI_HALF_SPEED))
      return error(0x11);
    cardInit = true;
  }
  
  // initialize a FAT16 volume 
  if (!fatInit) {
    if (!Fat16::init(&card))
      return error(0x12);
    fatInit = true;
  }
  
  // clear write error
  logFile.writeError = false;
  
  // O_CREAT - create the file if it does not exist
  // O_APPEND - seek to the end of the file prior to each write
  // O_WRITE - open for write
  if (!logFile.open(name, O_CREAT | O_APPEND | O_WRITE))
    return error(0x13);
  if (logFile.writeError)
    return error(0x14);
  return true;
}

bool closeLog() {
  // sync data to card
  if (!logFile.close())
    return error(0x15);
  lastCardError = 0;  
  done(); // success  
  return true;
}


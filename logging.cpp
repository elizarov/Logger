#include "logging.h"
#include "powersave.h"
#include "Pins.h"

char name[] = "YY-MM-DD.LOG";

SdCard card;
Fat16 logFile;
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

uint8_t error(uint8_t def) {
  digitalWrite(TF_EN_PIN, 0); // power cycle card to reset it
  cardInit = false;
  done();
  return card.errorCode == 0 ? def : card.errorCode;
}

// Expects YY-MM-DD... string in date (reads first 8 chars)
uint8_t openLog(const char* date) {
  memcpy(&name, date, 8);

  digitalWrite(TF_EN_PIN, 1); // power ON card 
  power_spi_enable(); // power ON SPI
  SPCR = saveSPCR; // restore SPI state

  if (!cardInit) {     
    if (!card.begin(TF_CS_PIN, SPI_HALF_SPEED))
      return error(0xe1);
    cardInit = true;
  }
  
  // initialize a FAT16 volume (first time only!)
  if (!fatInit) {
    if (!Fat16::init(&card))
      return error(0xe2);
    fatInit = true;
  }
  
  // clear write error
  logFile.writeError = false;
  
  // O_CREAT - create the file if it does not exist
  // O_APPEND - seek to the end of the file prior to each write
  // O_WRITE - open for write
  if (!logFile.open(name, O_CREAT | O_APPEND | O_WRITE))
    return error(0xe3);
  if (logFile.writeError)
    return error(0xe4);
  return 0;
}

void closeLog() {
  logFile.close(); // sync data to card
  done(); // success  
}


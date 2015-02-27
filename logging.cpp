#include "logging.h"
#include "powersave.h"
#include "Pins.h"

char name[] = "YY-MM-DD.LOG";

SdCard card;
Fat16 logFile;

void powerDown() {
  digitalWrite(TF_EN_PIN, 0); // power down card
  SPCR = 0; // disable SPI
  power_spi_disable(); // power off SPI
}

// Expects YY-MM-DD... string in date (reads first 8 chars)
bool openLog(const char* date) {
  memcpy(&name, date, 8);

  power_spi_enable(); // power on SPI
  digitalWrite(TF_EN_PIN, 1); // power up card
     
  if (!card.init(0, TF_CS_PIN)) {
    powerDown();
    return false;
  }
  
  // initialize a FAT16 volume
  if (!Fat16::init(&card)) {
    powerDown();
    return false;
  }
  
  // clear write error
  logFile.writeError = false;
  
  // O_CREAT - create the file if it does not exist
  // O_APPEND - seek to the end of the file prior to each write
  // O_WRITE - open for write
  if (!logFile.open(name, O_CREAT | O_APPEND | O_WRITE)) {
    powerDown();
    return false;
  }

  return true;
}

void closeLog() {
   logFile.close(); // sync data to card
   powerDown();
}

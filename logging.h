#ifndef LOGGING_H_
#define LOGGING_H_

#include <Arduino.h>
#include <Fat16.h>

extern Fat16 logFile;

void setupLog();

// Expects YY-MM-DD... string in date (reads first 8 chars)
bool openLog(const char* date);
void closeLog();

#endif

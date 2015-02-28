#ifndef BATTERY_HAL_H_
#define BATTERY_HAL_H_

#include <Arduino.h>
#include "FixNum.h"
#include "LoggerData.h"

typedef fixnum16_2 voltage_t;

voltage_t getBatteryVoltage(); 
Charge getChargeStatus(); 

#endif

#include "battery_hal.h"
#include "powersave.h"
#include "Timeout.h"
#include "Pins.h"

const unsigned long QUERY_TIMEOUT = 500L; // check charge status at most 2 times per second
const int POWER_V_THRESHOLD = 4; // 4.00 Volts

Timeout queryTimeout(0); // immediately check first time

voltage_t lastBatteryVoltage;
Charge lastChargeStatus;

void check() {
  if (!queryTimeout.check())
    return;
  queryTimeout.reset(QUERY_TIMEOUT);
  power_adc_enable();
  analogReference(INTERNAL); 
  analogRead(LIPO_VOL_PIN); // make a reading and discard its value 
  // voltage multiplier = (1.1 / 1024)*(10+2) / 2 = 11 / 10 / 1024 * 12 / 2 = 132 / 20480 
  lastBatteryVoltage = voltage_t((long)analogRead(LIPO_VOL_PIN) * (132L * voltage_t::multiplier) / 20480);  
  int ch = analogRead(CH_STATUS_PIN);
  power_adc_disable();
  if (ch > 900)
    lastChargeStatus = CHARGE_OFF;
  else if (ch > 550)
    lastChargeStatus = CHARGE_ON;
  else if (ch > 350)
    lastChargeStatus = CHARGE_DONE;
  else
    lastChargeStatus = CHARGE_ERROR;
}

FixNum<int,2> getBatteryVoltage() {
   check();
   return lastBatteryVoltage;
}

// can be called often, will cache last value for a while
Charge getChargeStatus() {
  check();
  return lastChargeStatus;
}

bool isPoweredOn() {
  check();
  return lastBatteryVoltage > POWER_V_THRESHOLD || lastChargeStatus != CHARGE_OFF;
}

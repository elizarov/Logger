#include "powersave.h"

void setupPowerSave() {
  // turn off Timer1, Timer2, SPI, DAC
  power_timer1_disable();
  power_timer2_disable();
  power_spi_disable();
  power_adc_disable();
}


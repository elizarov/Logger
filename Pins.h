/*

Hardware is based on Seeeduino Stalker v2.3.

Solder jumpers on Stalker:
  - RTC INT (connected to D2) to receive interrupts from RTC clock (not acutally used)
  - TF_EF (connected to D4) to power up SDCard

Addional sensors:
  - DS18B20 for reference outside temperature (connected to D8)

XBee for communication with additional wiring:
  - CTS (connectd to D3)
  - SLEEP_RQ (connected to D9)

All pins and ports:

D0    PD0   RXD  \ Serial / XBee
D1    PD1   TXD  /
D2    PD2   RTC INT (INT0)
D3    PD3   BEE CTS (INT1)  -----> XBEE PIN 12 (aka DIO7)
D4    PD4   TF_EN
D5    PD5   POWER_BEE
D6    PD6 
D7    PD7 

D8    PB0   DS18B20 Data
D9    PB1   BEE SLEEP RQ  ------> XBEE PIN 9 (aka DIO8)
D10   PB2   TF_CS
D11   PB3   MOIS \
D12   PB4   MISO | TF SPI
D13   PB5   SCK  /        ------> LED

A0    PC0
A1    PC1
A2    PC2
A3    PC3
A4    PC4   SDA  \  I2C
A5    PC5   SCL  /

      ADC6  CH_STATUS
      ADC7  LIPO_VOL
 
*/

const uint8_t CTS_PIN = 3;
const uint8_t TF_EN_PIN = 4;
const uint8_t POWER_BEE_PIN = 5;

const uint8_t DS18B20_PIN = 8;
const uint8_t SLEEP_RQ_PIN = 9;
const uint8_t TF_CS_PIN = 10;
const uint8_t LED_PIN = 13;

const uint8_t CH_STATUS_PIN = A6;
const uint8_t LIPO_VOL_PIN = A7;

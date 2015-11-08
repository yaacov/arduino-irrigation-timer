## Irrigation Timer sketch for Arduino

Arduino irrigation timer (24 hour) with Modbus serial communication.

(based on modbus-master-slave library example)

### Requires

##### Modbus Master-Slave library for Arduino
https://github.com/smarmengol/Modbus-Master-Slave-for-Arduino

##### Time library for Arduino
http://www.pjrc.com/teensy/td_libs_Time.html

##### S1307RTC Library for Arduino
http://www.pjrc.com/teensy/td_libs_DS1307RTC.html

### Register Table

##### Time registers

0 - msb of time (uint32_t)

1 - lsb of time (uint32_t)

2 - unit is connected to a DS1307 Real Time Clock module

##### Unit comunication status registers

3 - number of out coming messages

4 - error counter

##### Digital I/O's registers

One register (16bit) control 16 digital I/O's, each bit controls one digital I/O pin.

bit 3 - control D3 pin, bit 4 control D4 pin ... and bit 12 control D12 pin

(*) pins 0 and 1 are RX,TX pins

(**) pin D13 is reserved for led indicator

(***) pin CTRL_PIN is reserved for RS485 control

###### example:
to override timer and set pin 10 to HIGH -
set registers 5 and 6 to 0x0400

5 - state of digital I/O's: 0 - LOW, 1 - HIGH

6 - override digital I/O's timer: 0 - use timer, 1 - manual (override timer),

##### Timer program registers

Each day is divided to 96 15min parts.

register 7 control pins state from 00:00 to 00:15

register 8 control pins state from 00:15 to 00:30

...

register 103 (last) control pins state from 23:45 to 24:00

###### example:
to set pin 12 to high from 06:00 to 06:30 -

set registers 31 and 32 to 0x0800

7..102 - timer array for digital i/o's

##### User data registers

103..109 - registers for storing general information (on the eeprom)

##### Analog input registers

Analog value is provided without conversions -

registers hold the input voltage range, 0 to 5 volts is converted to value between 0 and 1023

(*) A4 and A5 are reserved for i2c RTC

110 - A0 ADC converted value (0..1023)

111 - A1 ADC converted value (0..1023)

...

117 - A7 ADC converted value (0..1023)


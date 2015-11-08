/**
 *  Arduino irrigation timer (24 hour) with Modbus serial communication.
 *  (based on modbus-master-slave library example)
 *
 *  Copyright (C) 2015 Yaacov Zamir <kobi.zamir@gmail.com>
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <EEPROM.h>
#include <Wire.h>
#include <Time.h>
#include <DS1307RTC.h>
#include <ModbusRtu.h>

#define ID 1
#define CTRL_PIN 8
#define BAUD_RATE 9600
#define NUM_REGISTERS 118

// This is slave ID using control pin CTRL_PIN for RS485 communication
Modbus slave(ID, 0, CTRL_PIN);
int8_t state = 0;
uint32_t tempus;
uint32_t timeReg;

boolean has_RTC = false;

/** 
 *  Unit time registers
 */
uint16_t au16data[NUM_REGISTERS];

/**
 *  Setup procedure
 */
void setup() {
  io_setup(); // I/O settings

  // check for RTC time
  timeReg = RTC.get();

  /*  if got time from RTC - set unit time and flag
   *  that unit has RTC
   */
  if (timeReg) {
    setTime(timeReg);
    has_RTC = true;
  }

  // set has_RTC register
  au16data[2] = has_RTC;
  
  // start communication
  slave.begin(BAUD_RATE);

  // blink for 100ms
  tempus = millis() + 100;
  digitalWrite(13, HIGH );
}

/**
 *  Loop procedure
 */
void loop() {
  // check timer for I/O pins 11 and 12
  timer_poll(11);
  timer_poll(12);

  // poll messages
  // blink led pin on each valid message
  state = slave.poll(au16data, NUM_REGISTERS);

  if (state > 4) {
    tempus = millis() + 50;
    digitalWrite(13, HIGH);
  }
  if (millis() > tempus) digitalWrite(13, LOW);

  // link the Arduino pins to the Modbus array
  io_poll();
  status_poll();

  // update timers table to disk
  update_eeprom();
}

/**
 * Hardware pin mapping:
 * D11 - relay a
 * D12 - relay b
 *
 * D13 - led indicator
 * 
 * A0 - Thermistor (470k resistor pull up)
 */
void io_setup() {
  int i;
  
  // define i/o
  pinMode(11, OUTPUT); // relay a
  pinMode(12, OUTPUT); // relay b
  pinMode(13, OUTPUT); // led indicator

  // init state register to HIGH
  bitWrite(au16data[5], 11, 1);
  bitWrite(au16data[5], 12, 1);

  // use disk to init override register 
  EEPROM.get(0, au16data[6]);

  /*  read timer table from disk (+7 user data registers)
   *  start at location 2 (after override register)
   *  continue another 7 user defined registers after the 96 timer registers
   *  ( timer registers start at register 7 )
   */
  for (i = 0; i < (96 + 7); i++) {
    EEPROM.get(2 + i * 2, au16data[i + 7]);
  }
}

/**
 *  Link between the Arduino pins and the Modbus array
 */
void io_poll() {
  // set digital I/O pins state
  digitalWrite(11, bitRead(au16data[5], 11));
  digitalWrite(12, bitRead(au16data[5], 12));

  // get analog I/O pins converted values
  au16data[110] = analogRead(0);
}

/**
 *  Update communication status and time registers
 */
void status_poll() {
  uint32_t timeDelta;

  // update time registers
  timeReg = (uint32_t)au16data[0] * 0x10000 + (uint32_t)au16data[1];
  timeDelta = abs((int32_t)timeReg - (int32_t)now());

  /*  if time registers are 1 min (60s) off from unit time
   *  set the unit time using the registers,
   *  o/w set the registers to match unit time.
   */
  if (timeDelta > 60) {
    // sync Arduino clock to the time received
    setTime(timeReg);

    // if unit has RTC, update RTC
    if (has_RTC) {
      RTC.set(timeReg);
    }
  } else {
    // sync time registers to Arduino clock
    timeReg = now();
    au16data[0] = (uint16_t)(timeReg >> 16);
    au16data[1] = (uint16_t)(timeReg & 0xffff);
  }

  // diagnose communication
  au16data[3] = slave.getOutCnt();
  au16data[4] = slave.getErrCnt();
}

/**
 *  Update state register using the timer array
 */
void timer_poll(int i) {
  int part;
  boolean state;

  // check for timer override
  if (bitRead(au16data[6], i)) return;

  // get 24 hour 15min part number
  part = ((now() / 60) % (24 * 60)) / 15;

  // update state
  state = bitRead(au16data[part + 7], i);
  bitWrite(au16data[5], i, state);
}

/**
 *  Update eeprom timer table
 */
void update_eeprom() {
  int i;

  // write override register to disk if changed
  EEPROM.put(0, au16data[6]);

  // write timer table to disk if changed (+7 user data registers)
  for (i = 0; i < (96 + 7); i++) {
    /*  EEPROM.put notes:
     *  1 - check value and write only if data change
     *  2 - write size(data) number of bytes, size(unit_16_t) = 2
     *
     *  timer table registers start at register number 7
     */
    EEPROM.put(2 + i * 2, au16data[i + 7]);
  }
}


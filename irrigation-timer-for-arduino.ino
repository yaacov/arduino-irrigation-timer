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
uint32_t blinkTime;
uint32_t timeReg;
boolean has_RTC = false;

// Unit time registers
uint16_t au16data[NUM_REGISTERS];

/**
 * Setup io pins state
 *
 * io pins mapping:
 * D11 - relay a
 * D12 - relay b
 *
 * D13 - led indicator
 * 
 * A0 - Thermistor (470k resistor pull up)
 */
void io_setup() {
    // define i/o
    pinMode(11, OUTPUT); // relay a
    pinMode(12, OUTPUT); // relay b
    pinMode(13, OUTPUT); // led indicator

    // init i/o's state
    bitWrite(au16data[5], 11, 1);
    bitWrite(au16data[5], 12, 1);
}

/**
 * Read persistent data from eeprom and set registers.
 * 
 * register  6 - overide program timers.
 * registers 7..103 - program timers.
 * registers 104..109 - user persistent data (e.g. location code, install time).
 */
void read_eeprom() {
    int i;

    // use disk to init override register 
    EEPROM.get(0, au16data[6]);
    
    // program timers
    for (i = 0; i < 96; i++) {
        EEPROM.get(2 + i * 2, au16data[i + 7]);
    }
    
    // user persistent data
    for (i = 96; i < (96 + 7); i++) {
        EEPROM.get(2 + i * 2, au16data[i + 7]);
    }
}

/**
 * Update persistent data from eeprom and set registers.
 *
 * register  6 - overide program timers.
 * registers 7..103 - program timers.
 * registers 104..109 - user persistent data (e.g. location code, install time).
 */
void update_eeprom() {
    int i;
    
    // write override register to disk if changed
    EEPROM.put(0, au16data[6]);
    
    // program timers
    for (i = 0; i < 96; i++) {
        EEPROM.put(2 + i * 2, au16data[i + 7]);
    }
    
    // user persistent data
    for (i = 96; i < (96 + 7); i++) {
        EEPROM.put(2 + i * 2, au16data[i + 7]);
    }
}

/**
 * Sync unit time.
 */
void sync_unit_time() {
    setTime(timeReg);

    // if unit has RTC, update RTC
    if (has_RTC)
        RTC.set(timeReg);
}

/**
 * Sync time registers with unit clock.
 * 
 * register 0 - time msb.
 * register 1 - time lsb.
 */
void sync_time_regisers() {
    
    // sync time registers to Arduino clock
    timeReg = now();
    au16data[0] = (uint16_t)(timeReg >> 16);
    au16data[1] = (uint16_t)(timeReg & 0xffff);
}

/**
 * update time registers with unit clock.
 * 
 * register 0 - time msb.
 * register 1 - time lsb.
 */
void update_time_regisers() {
    uint32_t timeDelta;

    // update time registers
    timeReg = (uint32_t)au16data[0] * 0x10000 + (uint32_t)au16data[1];
    timeDelta = abs((int32_t)timeReg - (int32_t)now());

    /*  if time registers are 1 min (60s) off from unit time
    *  set the unit time using the registers,
    *  o/w set the registers to match unit time.
    */
    if (timeDelta > 60) {
        sync_unit_time();
    } else {
        sync_time_regisers();
    }
}

/**
 * Setup time from RTC and set time state variables and time registers.
 * 
 * timeReg - current time variable.
 * has_RTC - unit has DS1307RTC clock.
 */
void setup_time() {
    // if got time from RTC - set unit time and flag that unit has RTC.
    timeReg = RTC.get();
    if (timeReg) {
        setTime(timeReg);
        has_RTC = true;
    }
    
    // set has_RTC register
    au16data[2] = has_RTC;
    sync_time_regisers();
}

/**
 * Setup procedure
 */
void setup() {
    // init io pins and registers.
    io_setup();
    read_eeprom();
    setup_time();
    
    // start communication
    slave.begin(BAUD_RATE);
    
    // blink for 100ms
    blinkTime = millis() + 100;
    digitalWrite(13, HIGH );
}

/**
 *  Run timer logic for one io pin, and update pin state.
 *
 * @param pin the io pin number to check.
 */
void run_timer_logic_pin(int pin) {
    int part;
    boolean state;
    
    // check for timer logic override
    if (bitRead(au16data[6], pin)) return;
    
    // get 24hour / 15min part of the day.
    part = ((now() / 60) % (24 * 60)) / 15;
    
    // update io pin state.
    state = bitRead(au16data[part + 7], pin);
    bitWrite(au16data[5], pin, state);
}

/**
 *  Loop procedure
 */
void loop() {
    // run timer logic and set state of io pins 11 and 12.
    run_timer_logic_pin(11);
    run_timer_logic_pin(12);
    
    // poll modbus messages
    // blink led pin on each valid message
    state = slave.poll(au16data, NUM_REGISTERS);
    if (state > 4) {
        blinkTime = millis() + 50;
        digitalWrite(13, HIGH);
    }
    if (millis() > blinkTime) digitalWrite(13, LOW);
    
    // set digital io pins state
    digitalWrite(11, bitRead(au16data[5], 11));
    digitalWrite(12, bitRead(au16data[5], 12));

    // get analog io pins converted values
    au16data[110] = analogRead(0);
    
    // diagnose communication
    au16data[3] = slave.getOutCnt();
    au16data[4] = slave.getErrCnt();

    // update unit time registers and eeprom.
    update_time_regisers();
    update_eeprom();
}


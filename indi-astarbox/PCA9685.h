/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Name         : PCA9685.h
 * Original Author      : Georgi Todorov
 * Created on   : Nov 22, 2013
 *
 * Edited by    : Tord Wessman
 * Version      :
 *
 * Edited by    : Rodolphe Pineau
 * Date         : Jan. 17, 2024
 *
 * Copyright © 2012 Georgi Todorov  <terahz@geodar.com>
 */

#ifndef _PCA9685_H
#define _PCA9685_H
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <fcntl.h>
//

#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <thread>

// Register Definitions

#define MODE1 0x00            //Mode  register  1
#define MODE2 0x01            //Mode  register  2
#define SUBADR1 0x02        //I2C-bus subaddress 1
#define SUBADR2 0x03        //I2C-bus subaddress 2
#define SUBADR3 0x04        //I2C-bus subaddress 3
#define ALLCALLADR 0x05     //PORT All Call I2C-bus address
#define PORT0 0x6            //PORT0 start register
#define PORT0_ON_L 0x6        //PORT0 output and brightness control byte 0
#define PORT0_ON_H 0x7        //PORT0 output and brightness control byte 1
#define PORT0_OFF_L 0x8        //PORT0 output and brightness control byte 2
#define PORT0_OFF_H 0x9        //PORT0 output and brightness control byte 3
#define PORT_MULTIPLYER 4    // For the other 15 channels
#define ALLPORT_ON_L 0xFA    //load all the PORTn_ON registers, byte 0 (turn 0-7 channels on)
#define ALLPORT_ON_H 0xFB    //load all the PORTn_ON registers, byte 1 (turn 8-15 channels on)
#define ALLPORT_OFF_L 0xFC    //load all the PORTn_OFF registers, byte 0 (turn 0-7 channels off)
#define ALLPORT_OFF_H 0xFD    //load all the PORTn_OFF registers, byte 1 (turn 8-15 channels off)
#define PRE_SCALE 0xFE        //prescaler for output frequency
#define CLOCK_FREQ 25000000.0 //25MHz default osc clock
#define BUFFER_SIZE 0x08  //1 byte buffer

#define MODE1_SLEEP 0x10   // Low power mode. Oscillator off
#define MODE1_AI 0x20      // Auto-Increment enabled
#define MODE1_EXTCLK 0x40  // Use EXTCLK pin clock
#define MODE1_RESTART 0x80 // Restart enabled

#define MODE2_OUTDRV 0x04 // totem pole structure vs open-drain

#define MAX_PCA_VALUE  4096


class PCA9685 {
public:

    PCA9685();
    void init(int bus, int address);
    virtual ~PCA9685();
	bool isPCA9685Present();
    int reset(void);
    int setPWMFreq(int freq);

    int setPWM(uint8_t nPort, int value);
    int getPWM(uint8_t nPort, int &value);

    int setOn(uint8_t nPort);
    int setOff(uint8_t nPort);
    bool isPortOn(uint8_t nPort);

private:

    int setPWM(uint8_t nPort, int on_value, int off_value);
    int getPWM(uint8_t nPort, int &on_value, int &off_value);

    int m_nI2CAddr;
    std::string sBusFile;
    uint8_t dataBuffer[BUFFER_SIZE];
    int read_byte(int fd, uint8_t address, uint8_t &nValue);
    int write_byte(int fd, uint8_t address, uint8_t data);
    int openfd();
};
#endif

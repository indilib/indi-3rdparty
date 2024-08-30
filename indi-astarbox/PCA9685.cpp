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
 * Name        : PCA9685.cpp
 * Original Author      : Georgi Todorov
 * Created on   : Nov 22, 2013
 *
 * Edited by    : Tord Wessman
 * Version      :
 *
 * Edited by    : Rodolphe Pineau
 * Date         : Jan. 17 2024
 *
 * Copyright © 2012 Georgi Todorov  <terahz@geodar.com>
 */

#include "PCA9685.h"

//! Constructor takes bus and address arguments
/*!
 \param bus the bus to use in /dev/i2c-%d.
 \param address the device address on bus
 */

void PCA9685::init(int bus, int address)
{
    std::stringstream ssTmp;
    m_nI2CAddr = address;
    ssTmp << "/dev/i2c-" << bus;
    sBusFile.assign(ssTmp.str());
}

PCA9685::PCA9685()
{

}

PCA9685::~PCA9685()
{
}


bool PCA9685::isPCA9685Present()
{
	int nErr;
	int fd;
	uint8_t nValue;
	bool bPresent;

	bPresent = true;

	fd = openfd();
	if(fd!=-1) {
		nErr = read_byte(fd, PORT0_ON_L + PORT_MULTIPLYER , nValue);
		if(nErr == -1)
			bPresent = false;
		close(fd);
	}
	else {
		bPresent = false;
	}
	return bPresent;
}

//! Sets PCA9685 mode to 00
int PCA9685::reset()
{
    int nErr = 0;
    int fd = openfd();

    if (fd != -1) {
        // Sends a reset command to the PCA9685 chip over I2C
        nErr = write_byte(fd, MODE1, MODE1_RESTART); //Normal mode
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if(nErr) {
            close(fd);
            return -1;
        }
        nErr = write_byte(fd, MODE2, MODE2_OUTDRV); //totem pole
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if(nErr) {
            close(fd);
            return -1;
        }
        close(fd);
        return 0;
    }
    return -1;
}
//! Set the frequency of PWM
/*!
 \param freq desired frequency. 40Hz to 1000Hz using internal 25MHz oscillator.
 */
int PCA9685::setPWMFreq(int freq)
{
    int nErr = 0;
    uint8_t prescale;

    int fd = openfd();
    if (fd != -1) {
        prescale = uint8_t(((CLOCK_FREQ / (freq * 4096.0)) + 0.5) - 1);

        nErr |= write_byte(fd, MODE1, MODE1_SLEEP);        // go to sleep
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        nErr |= write_byte(fd, PRE_SCALE, prescale);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        nErr |= write_byte(fd, MODE1, MODE1_RESTART);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        nErr = write_byte(fd, MODE2, MODE2_OUTDRV); //totem pole
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        close(fd);
        return nErr;
    }
    return -1;
}


//! PWM a single channel
/*!
 \param nPort channel to set PWM value for
 \param value 0-4095 value for PWM
 */
int  PCA9685::setPWM(uint8_t  nPort, int value)
{
    int nErr = 0;

    if(value <= 1) {
        nErr = setOff(nPort);
    }
    else if (value>=4095) {
        nErr = setOn(nPort);
    }
    else {
        nErr = setPWM( nPort, 0, value);
    }
    return nErr;
}

//! Get PWM for a single channel
/*!
 \param nPort channel to getPWM value for
 \param value value of current PWM
 */

int PCA9685::getPWM(uint8_t nPort, int &value)
{
    int nErr = 0;
    int on_value = 0;

    nErr = getPWM(nPort, on_value, value);
    return nErr;
}


int PCA9685::setOn(uint8_t nPort)
{   
    int nErr = 0;

    nErr = setPWM(nPort, MAX_PCA_VALUE, 0);
    return nErr;
}

int PCA9685::setOff(uint8_t nPort)
{
    int nErr = 0;

    nErr = setPWM(nPort, 0, MAX_PCA_VALUE);
    return nErr;
}

bool PCA9685::isPortOn(uint8_t nPort)
{
    int nErr = 0;
    int on_value = 0;
    int off_value = 0;

    nErr = getPWM(nPort, on_value, off_value);
    return (on_value==4096);
}


// private methods

//! PWM a single channel with custom on time
/*!
 \param nPort  channel to set PWM value for
 \param on_value 0-4095 value to turn on the pulse
 \param off_value 0-4095 value to turn off the pulse
 */
int PCA9685::setPWM(uint8_t nPort, int on_value, int off_value)
{
    int nErr = 0;
    int fd = openfd();

    if (fd != -1) {
        nErr |= write_byte(fd, PORT0_ON_L + PORT_MULTIPLYER * nPort, on_value & 0xFF);
        nErr |= write_byte(fd, PORT0_ON_H + PORT_MULTIPLYER * nPort, on_value >> 8);
        nErr |= write_byte(fd, PORT0_OFF_L + PORT_MULTIPLYER * nPort, off_value & 0xFF);
        nErr |= write_byte(fd, PORT0_OFF_H + PORT_MULTIPLYER * nPort, off_value >> 8);
        close(fd);
        return nErr;
    }
    return -1;
}


//! Get PWM from a single channel with custom on time
/*!
 \param nPort  channel to set PWM value for
 \param on_value value the pulse is  on
 \param off_value  value the pulse is off
 */
int PCA9685::getPWM(uint8_t nPort, int &on_value, int &off_value)
{
    int nErr = 0;
    uint8_t nValue = 0;
    int fd = openfd();

    if (fd != -1) {
        nErr = read_byte(fd, PORT0_ON_L + PORT_MULTIPLYER * nPort, nValue);
        if(nErr == -1)
            return nErr;
        on_value = nValue & 0xFF;
        nErr = read_byte(fd, PORT0_ON_H + PORT_MULTIPLYER * nPort, nValue);
        if(nErr == -1)
            return nErr;
        on_value |= (nValue & 0xFF) << 8;

        nErr = read_byte(fd, PORT0_OFF_L + PORT_MULTIPLYER * nPort, nValue);
        if(nErr == -1)
            return nErr;
        off_value = nValue & 0xFF;
        nErr = read_byte(fd, PORT0_OFF_H + PORT_MULTIPLYER * nPort, nValue);
        if(nErr == -1)
            return nErr;
        off_value |= (nValue & 0xFF) << 8;
        close(fd);
        return 0;
    }
    return -1;
}


//! Read a single byte from PCA9685
/*!
 \param fd file descriptor for I/O
 \param address register address to read from
 */
int PCA9685::read_byte(int fd, uint8_t address, uint8_t &nValue)
{
    uint8_t buff[BUFFER_SIZE];

    memset(buff, 0, BUFFER_SIZE);
    memset(dataBuffer, 0, BUFFER_SIZE);
    buff[0] = address;

    if (write(fd, buff, 1) != 1) {
        return (-1);
    } else {
        if (read(fd, dataBuffer, 1) != 1) {
            return (-1);
        }
    }
    nValue = dataBuffer[0];
    return 0;
}
//! Write a single byte from PCA9685
/*!
 \param fd file descriptor for I/O
 \param address register address to write to
 \param data 8 bit data to write
 */
int PCA9685::write_byte(int fd, uint8_t address, uint8_t data)
{
    uint8_t buff[2];

    buff[0] = address;
    buff[1] = data;
    if (write(fd, buff, sizeof(buff)) != 2) {
        return -1;
    }else{
    }
    return 0;
}
//! Open device file for PCA9685 I2C bus
/*!
 \return fd returns the file descriptor number or -1 on error
 */
int PCA9685::openfd() {
    int fd = -1;
    if ((fd = open(sBusFile.c_str(), O_RDWR)) < 0) {
        return -1;
    }
    if (ioctl(fd, I2C_SLAVE, m_nI2CAddr) < 0) {
        return -1;
    }

    return fd;
}


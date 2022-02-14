/*******************************************************************************
  Copyright(c) 2021 Ken Self <ken.kgself AT gmail DOT com>

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#ifndef ASIPOWER_H
#define ASIPOWER_H

#include <string.h>
#include <iostream>
#include <stdio.h>
#include <inditimer.h>

#include <defaultdevice.h>
    static const int max_pwm_duty = 100;
    static const int pwm_freq = 1000;
    static const int n_gpio_pin = 4;
    static const int gpio_pin[n_gpio_pin] = {12, 13, 26, 18};
    static const int n_dev_type = 9;
    static const std::string dev_type[n_dev_type] = {"None","Camera","Focuser","Dew Heater","Flat Panel","Mount","Fan","Other on/off","Other variable"};
    static const bool dev_pwm[n_dev_type] = { false, false, false, true, true, false, true, false, true };
    static const int dslr_pin = 21;
    static const uint32_t max_tick = 4294967295;
    static const int32_t max_timer_ms = 50000;

    static const uint8_t i2c_addr[] = {0x48, 0x49, 0x4b};
    static const int n_sensor = 5;
    static const int n_va = 2;
    static const int n_i2c = 3;
    static const std::string port_name[] = {"Main Power", "Port 1", "Port 2", "Port 3", "Port 4"};


    static const struct {
      int sensor_id;
      int va;
      int i2c_id;
      uint16_t addr;
      double adjust;
    } p_sensors[] = {
        {0, 0, 2, 0x83e6, 21./2000}, {0, 1, 2, 0x83f4, 1./200},   // OUT-0 Main Power
        {1, 0, 0, 0x83c6, 21./2000}, {1, 1, 1, 0x83fa, 1./80},    // OUT-1 Port 1
        {2, 0, 0, 0x83e6, 21./2000}, {2, 1, 1, 0x83da, 1./80},    // OUT-2 Port 2
        {3, 0, 1, 0x83c6, 21./2000}, {3, 1, 0, 0x83fa, 1./80},    // OUT-3 Port 3
        {4, 0, 1, 0x83e6, 21./2000}, {4, 1, 0, 0x83da, 1./80},    // OUT-4 Port 4
    };
    static const struct timespec sensor_read_wait = {0, 2500 * 1000}; // I2C 400KHz
    static const unsigned int sensor_read_interval = 5000;

class IndiAsiPower : public INDI::DefaultDevice
{
public:
    IndiAsiPower();
    virtual ~IndiAsiPower();
    virtual const char *getDefaultName();
    virtual bool initProperties();
    virtual bool updateProperties();
    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
    virtual bool ISSnoopDevice(XMLEle *root);
    virtual void TimerHit() override;
    void IndiTimerCallback();


protected:
    virtual bool saveConfigItems(FILE *fp);
private:
    virtual bool Connect();
    virtual bool Disconnect();
   
    ISwitch DeviceS[n_gpio_pin][n_dev_type];
    ISwitchVectorProperty DeviceSP[n_gpio_pin];
    ISwitch OnOffS[n_gpio_pin][2];
    ISwitchVectorProperty OnOffSP[n_gpio_pin];
    INumber DutyCycleN[n_gpio_pin][1];
    INumberVectorProperty DutyCycleNP[n_gpio_pin];

    int m_type[n_gpio_pin];
    int m_piId;

// DSLR properties: DurationN, DelayN, CountN, StartS, AbortS
    ISwitch DslrS[2];
    ISwitchVectorProperty DslrSP;
    INumber DslrExpN[3];
    INumberVectorProperty DslrExpNP;
    
    std::chrono::time_point<std::chrono::system_clock> dslr_start;
    bool dslr_isexp;
    int dslr_counter;
    void DslrChange(bool isInit=false, bool abort=false);
    INDI::Timer timer;

// Power sensor
    bool have_sensor;
    int i2c_handle[n_i2c];
    INumber PowerSensorN[n_sensor][n_va];
    INumberVectorProperty PowerSensorNP[n_sensor];
    void ReadSensor();
};

#endif

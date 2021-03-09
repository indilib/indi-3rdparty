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
    void DslrTimer(int pi, unsigned user_gpio, unsigned level, uint32_t tick);

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
    
    int dslr_cb;
    uint64_t dslr_end;
    uint32_t dslr_last;
    bool dslr_isexp;
    int dslr_counter;
    void DslrChange(bool isInit=false, bool abort=false);

};

#endif

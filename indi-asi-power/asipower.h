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
    static const int n_gpio_pin =4;
    static const int gpio_pin[n_gpio_pin]={12, 13, 18, 26};
    static const int n_dev_type =5;
    static const std::string dev_type[n_dev_type] = {"None","Camera","Focuser","Dew Heater","Flat Panel"};
    static const bool dev_pwm[n_dev_type] = { false, false, false, true, true };
    
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
protected:
    virtual bool saveConfigItems(FILE *fp);
private:
    virtual bool Connect();
    virtual bool Disconnect();
// We want for each of 4 ports:
// Type (None, Camera, Focuser, Flat Panel(PWM), Dew Heater (PWM))
// On/Off/Duty Cycle
// If not PWM then use gpioWrite to turn on/off
// If PWM then Frequency is always 100?, Range is always 100
// GPIO 12, 13, 26, 18
// Hardware PWM
// GPIO 12/18 are one channel: Should be used for LED light panel
// GPIO 13/19 are another channel (GPIO 19 used by AAP for sound)
// DMA timed PWM
// Any GPIO: Ok for LED light panel
// Software PWM
// Any GPIO: Ok for dew heater

//Device 1 is GPIO 12. 
//    ISwitch Device1S[5];
//    ISwitchVectorProperty Device1SP;
//    ISwitch OnOff1S[2];
//    ISwitchVectorProperty OnOff1SP;
//    INumber DutyCycle1N[1];
//    INumberVectorProperty DutyCycle1NP;
    
    ISwitch DeviceS[4][5];
    ISwitchVectorProperty DeviceSP[4];
    ISwitch OnOffS[4][2];
    ISwitchVectorProperty OnOffSP[4];
    INumber DutyCycleN[4][1];
    INumberVectorProperty DutyCycleNP[4];

//    int m_type1;
    int m_type[4];
    int m_piId;
};

#endif

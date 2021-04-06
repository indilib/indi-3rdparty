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

#ifndef RPIGPIO_H
#define RPIGPIO_H

#include <string.h>
#include <iostream>
#include <stdio.h>
#include <algorithm>

#include <defaultdevice.h>
    static const int max_gpio_pin = 32;

    static const int pi1_ngpio = 18;
    static const int pi1_gpio[pi1_ngpio] = {-1,0,1,4,7,8,9,10,11,14,15,17,18,21,22,23,24,25};
  
    static const int pi2_ngpio = 22;
    static const int pi2_gpio[pi2_ngpio] = {-1,2,3,4,7,8,9,10,11,14,15,17,18,22,23,24,25,27,28,29,30,31};
  
    static const int pi3_ngpio = 27;
    static const int pi3_gpio[pi3_ngpio]={-1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27};
  
    static const int pi4_ngpio = 29;
    static const int pi4_gpio[pi4_ngpio]={-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27};
    
    static const int max_pwm_duty = 100;
    static const int pwm_freq = 1000;
    static const int n_gpio_pin = 5;
    static const int n_dev_type = 4;
    static const std::string dev_type[n_dev_type] = {"None","On/Off","PWM","Timer"};
    static const bool dev_pwm[n_dev_type] = { false, false, true, false };
    static const bool dev_timer[n_dev_type] = { false, false, false, true };
    static const uint32_t max_tick = 4294967295;
    static const int32_t max_timer_ms = 50000;
    static const char PIN_TAB[] = "GPIO Config";
    static const char TIMER_TAB[] = "Timer Config";
    
    
class IndiRpiGpio : public INDI::DefaultDevice
{
public:
    IndiRpiGpio();
    virtual ~IndiRpiGpio();
    virtual const char *getDefaultName();
    virtual bool initProperties();
    virtual bool updateProperties();
    virtual void ISGetProperties(const char *dev);
    virtual bool ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n);
    virtual bool ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n);
    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n);
    virtual bool ISSnoopDevice(XMLEle *root);
    void TimerCallback(int pi, unsigned user_gpio, unsigned level, uint32_t tick);

protected:
    virtual bool saveConfigItems(FILE *fp);
private:
    virtual bool Connect();
    virtual bool Disconnect();

    int n_valid_gpio;
    const int* valid_gpio_pin;

    ISwitch DeviceS[n_gpio_pin][n_dev_type];
    ISwitchVectorProperty DeviceSP[n_gpio_pin];
    ISwitch OnOffS[n_gpio_pin][2];
    ISwitchVectorProperty OnOffSP[n_gpio_pin];
    ISwitch ActiveS[n_gpio_pin][2];
    ISwitchVectorProperty ActiveSP[n_gpio_pin];
    INumber DutyCycleN[n_gpio_pin][1];
    INumberVectorProperty DutyCycleNP[n_gpio_pin];

    int m_gpio_pin[n_gpio_pin];
    int m_type[n_gpio_pin];
    int m_piId;

    ISwitch GpioPinS[n_gpio_pin][max_gpio_pin+1];
    ISwitchVectorProperty GpioPinSP[n_gpio_pin];
    
    IText LabelT[n_gpio_pin][1];
    ITextVectorProperty LabelTP[n_gpio_pin];

// DSLR properties: DurationN, DelayN, CountN, StartS, AbortS
    INumber TimerOnN[n_gpio_pin][3];
    INumberVectorProperty TimerOnNP[n_gpio_pin];

// Need to track all gpio pins with a timer including Timer Change
    int timer_cb[n_gpio_pin];
    uint64_t timer_end[n_gpio_pin];
    uint32_t timer_last[n_gpio_pin];
    bool timer_isexp[n_gpio_pin];
    int timer_counter[n_gpio_pin];
    void TimerChange(unsigned user_gpio, bool isInit=false, bool abort=false);
    int FindPinIndex(unsigned user_gpio);
    int InitPiModel();

};
inline int IndiRpiGpio::FindPinIndex(unsigned user_gpio)
{
    int *found = std::find(std::begin(m_gpio_pin), std::end(m_gpio_pin), static_cast<int>(user_gpio));
    if (found != std::end(m_gpio_pin)) return std::distance(m_gpio_pin, found);
    return -1;
}
#endif

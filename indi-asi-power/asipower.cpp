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

#include <stdio.h>
#include <memory>
#include <string.h>
#include "config.h"

#include "asipower.h"

#include <pigpiod_if2.h>

// We declare an auto pointer to IndiAsiPower
std::unique_ptr<IndiAsiPower> device;

void ISPoll(void *p);

void ISInit()
{
    static int isInit = 0;

    if (isInit == 1)
        return;
    if(device.get() == 0)
    {
        isInit = 1;
        device.reset(new IndiAsiPower());
    }
}
void ISGetProperties(const char *dev)
{
        ISInit();
        device->ISGetProperties(dev);
}
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
        ISInit();
        device->ISNewSwitch(dev, name, states, names, num);
}
void ISNewText( const char *dev, const char *name, char *texts[], char *names[], int num)
{
        ISInit();
        device->ISNewText(dev, name, texts, names, num);
}
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
        ISInit();
        device->ISNewNumber(dev, name, values, names, num);
}
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int num)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(num);
}
void ISSnoopDevice (XMLEle *root)
{
    ISInit();
    device->ISSnoopDevice(root);
}
IndiAsiPower::IndiAsiPower()
{
    setVersion(VERSION_MAJOR,VERSION_MINOR);
    m_type[0] = 0;
    m_type[1] = 0;
    m_type[2] = 0;
    m_type[3] = 0;

}
IndiAsiPower::~IndiAsiPower()
{
   for(int i=0; i<n_gpio_pin;i++)
   {
       deleteProperty(DeviceSP[i].name);
       deleteProperty(OnOffSP[i].name);
       deleteProperty(DutyCycleNP[i].name);
   }
}
bool IndiAsiPower::Connect()
{
    // Init GPIO
    DEBUGF(INDI::Logger::DBG_SESSION, "pigpio version %d.", get_pigpio_version(m_piId));
    DEBUGF(INDI::Logger::DBG_SESSION, "Hardware revision %d.", get_hardware_revision(m_piId));
//   int status = gpioInitialise();
    m_piId = pigpio_start(NULL,NULL);

   if (m_piId < 0)
   {
      DEBUGF(INDI::Logger::DBG_SESSION, "pigpio initialisation failed: %d", m_piId);
      return false;
   }

    // Lock Device 1 setting
//    Device1SP.s=IPS_BUSY;
//    IDSetSwitch(&Device1SP, nullptr);

    DEBUG(INDI::Logger::DBG_SESSION, "ASI Power connected successfully.");
    return true;
}
bool IndiAsiPower::Disconnect()
{
    // Close GPIO
   pigpio_stop(m_piId);

    // Unlock BCM Pins setting
//    Device1SP.s=IPS_IDLE;
//    IDSetSwitch(&Device1SP, nullptr);

    DEBUG(INDI::Logger::DBG_SESSION, "ASI Power disconnected successfully.");
    return true;
}
const char * IndiAsiPower::getDefaultName()
{
        return (char *)"ASI Power";
}
bool IndiAsiPower::initProperties()
{
    // We init parent properties first
    INDI::DefaultDevice::initProperties();
/*
    IUFillSwitch(&Device1S[0], "DEV100", "NONE", ISS_ON);
    IUFillSwitch(&Device1S[1], "DEV101", "Camera", ISS_OFF);
    IUFillSwitch(&Device1S[2], "DEV102", "Focuser", ISS_OFF);
    IUFillSwitch(&Device1S[3], "DEV103", "Flat Panel", ISS_OFF);
    IUFillSwitch(&Device1S[4], "DEV104", "Dew Heater", ISS_OFF);
    IUFillSwitchVector(&Device1SP, Device1S, 5, getDeviceName(), "DEV1", "Port 1", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillSwitch(&OnOff1S[0], "ONOFF1OFF", "Off", ISS_ON);
    IUFillSwitch(&OnOff1S[1], "ONOFF1ON", "On", ISS_OFF);
    IUFillSwitchVector(&OnOff1SP, OnOff1S, 2, getDeviceName(), "ONOFF1", "On/Off", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&DutyCycle1N[0], "DUTYCYCLE1", "Duty Cycle", "%0.0f", 0, 100, 1, 0);
    IUFillNumberVector(&DutyCycle1NP, DutyCycle1N, 1, getDeviceName(), "DUTYCYCLE1", "Duty Cycle", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    defineProperty(&Device1SP);
*/

    std::string dev="DEV";
    std::string port="Port ";
    std::string onoff = "ONOFF";
    std::string dutyc = "DUTYCYCLE";
    for(int i=0; i<n_gpio_pin; i++)
    {
        for(int j=0; j<n_dev_type;j++)
        {
            IUFillSwitch(&DeviceS[i][j], (dev +std::to_string(i) + std::to_string(j)).c_str(), dev_type[j].c_str(), ISS_ON);
        }
        IUFillSwitchVector(&DeviceSP[i], DeviceS[i], n_dev_type, getDeviceName(), (dev +std::to_string(i)).c_str(), (port +std::to_string(i)).c_str(), MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

        IUFillSwitch(&OnOffS[i][0], (onoff + std::to_string(i) +"OFF").c_str(), "Off", ISS_ON);
        IUFillSwitch(&OnOffS[i][1], (onoff + std::to_string(i) +"ON").c_str(), "On", ISS_OFF);
        IUFillSwitchVector(&OnOffSP[i], OnOffS[i], 2, getDeviceName(), (onoff + std::to_string(i)).c_str(), "On/Off", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    
        IUFillNumber(&DutyCycleN[i][0], (dutyc + std::to_string(i)).c_str(), "Duty Cycle", "%0.0f", 0, 100, 1, 0);
        IUFillNumberVector(&DutyCycleNP[i], DutyCycleN[i], 1, getDeviceName(), (dutyc + std::to_string(i)).c_str(), "Duty Cycle", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);
    
        defineProperty(&DeviceSP[i]);
    }
    loadConfig();

    return true;
}
bool IndiAsiPower::updateProperties()
{
    // Call parent update properties first
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // We're connected
//        defineProperty(&OnOff1SP);
//        if(m_type1 > 2)
//            defineProperty(&DutyCycle1NP);
        for(int i=0; i<n_gpio_pin; i++)
        {
            defineProperty(&OnOffSP[i]);
            defineProperty(&DutyCycleNP[i]);
        }
    }
    else
    {
        // We're disconnected
//        deleteProperty(OnOff1SP.name);
//        deleteProperty(DutyCycle1NP.name);
        for(int i=0; i<n_gpio_pin; i++)
        {
            deleteProperty(OnOffSP[i].name);
            deleteProperty(DutyCycleNP[i].name);
        }
    }
    return true;
}

void IndiAsiPower::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);
}

bool IndiAsiPower::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    // first we check if it's for our device
    if(strcmp(dev,getDeviceName())==0)
    {
        // handle Duty Cycle on device i
        for(int i=0; i<n_gpio_pin; i++)
        {
            if (!strcmp(name, DutyCycleNP[i].name))
            {
                // verify a number is a valid Duty Cycle
                if ( values[0] < 0 || values[0] > 100 )
                {
                    DutyCycleNP[i].s=IPS_ALERT;
                    IDSetNumber(&DutyCycleNP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "Value %0.0f is not a valid Duty Cycle!", values[0]);
                    return false;
                }
                IUUpdateNumber(&DutyCycleNP[i],values,names,n);
                DutyCycleNP[i].s=IPS_OK;
                IDSetNumber(&DutyCycleNP[i], nullptr);
                DEBUGF(INDI::Logger::DBG_SESSION, "Device 1 set to duty cycle %0.0f", DutyCycleN[i][0].value);
                return true;
            }
        }
    }
    return INDI::DefaultDevice::ISNewNumber(dev,name,values,names,n);
}
bool IndiAsiPower::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // first we check if it's for our device
    if (!strcmp(dev, getDeviceName()))
    {
        // handle parity
        for(int i=0; i<n_gpio_pin; i++)
        {
            if (!strcmp(name, DeviceSP[i].name))
            {
                if(isConnected()) 
                {
//                    DEBUG(INDI::Logger::DBG_WARNING, "Cannot set port type while device is connected.");
//                    return false;
                }
                IUUpdateSwitch(&DeviceSP[i], states, names, n);
                m_type[i] = IUFindOnSwitchIndex(&DeviceSP[i]);
                DEBUGF(INDI::Logger::DBG_SESSION, "Device 1 New Type %d", m_type[i]);
                if(m_type[i] > 2)
                {
                    set_PWM_frequency(m_piId, gpio_pin[i], 1000);
                    set_PWM_range(m_piId, gpio_pin[i], 100);
                }
            }
            // handle relay 1
            if (!strcmp(name, OnOffSP[i].name))
            {
                IUUpdateSwitch(&OnOffSP[i], states, names, n);
    
                if ( OnOffS[i][0].s == ISS_ON )
                {
                    if(m_type[i] <= 2)
                    {
                    DEBUGF(INDI::Logger::DBG_SESSION, "Device %d Type %d set to OFF", i, m_type[i]);
                        gpio_write(m_piId, gpio_pin[i], PI_LOW);
                    }
                    else
                    {
                    DEBUGF(INDI::Logger::DBG_SESSION, "Device %d Type %d PWM OFF", i, m_type[i]);
                        set_PWM_dutycycle(m_piId, gpio_pin[i], 0);
                    }
                    OnOffSP[i].s = IPS_OK;
                    OnOffS[i][1].s = ISS_OFF;
                    IDSetSwitch(&OnOffSP[i], NULL);
                    return true;
                }
                if ( OnOffS[i][1].s == ISS_ON )
                {
                    if(m_type[i] <= 2)
                    {
                    DEBUGF(INDI::Logger::DBG_SESSION, "Device %d Type %d set to ON", i, m_type[i]);
                        gpio_write(m_piId, gpio_pin[i], PI_HIGH);
                    }
                    else
                    {
                    DEBUGF(INDI::Logger::DBG_SESSION, "Device %d Type %d PWM ON %0.0f\%", i, m_type[i], DutyCycleN[i][0].value);
                        set_PWM_dutycycle(m_piId, gpio_pin[i], DutyCycleN[i][0].value);
                    }
                    OnOffSP[i].s = IPS_IDLE;
                    OnOffS[i][0].s = ISS_OFF;
                    IDSetSwitch(&OnOffSP[i], NULL);
                    return true;
                }
            }
        }
    }
    return INDI::DefaultDevice::ISNewSwitch (dev, name, states, names, n);
}
bool IndiAsiPower::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    return INDI::DefaultDevice::ISNewText (dev, name, texts, names, n);
}
bool IndiAsiPower::ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    return INDI::DefaultDevice::ISNewBLOB (dev, name, sizes, blobsizes, blobs, formats, names, n);
}
bool IndiAsiPower::ISSnoopDevice(XMLEle *root)
{
    return INDI::DefaultDevice::ISSnoopDevice(root);
}
bool IndiAsiPower::saveConfigItems(FILE *fp)
{
    for(int i=0; i<n_gpio_pin; i++)
    {
        IUSaveConfigSwitch(fp, &DeviceSP[i]);
        IUSaveConfigSwitch(fp, &OnOffSP[i]);
        IUSaveConfigNumber(fp, &DutyCycleNP[i]);
    }
    return true;
}

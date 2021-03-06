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
#include <config.h>
#include <pigpiod_if2.h>
#include <asipower.h>

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
    std::fill_n(m_type, n_dev_type, 0);
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
    DEBUGF(INDI::Logger::DBG_SESSION, "pigpiod_if2 version %lu.", pigpiod_if_version());
    m_piId = pigpio_start(NULL,NULL);

    if (m_piId < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "pigpio initialisation failed: %d", m_piId);
        return false;
    }
    DEBUGF(INDI::Logger::DBG_SESSION, "pigpio version %lu.", get_pigpio_version(m_piId));
    DEBUGF(INDI::Logger::DBG_SESSION, "Hardware revision %x.", get_hardware_revision(m_piId));
    for(int i=0; i<n_gpio_pin; i++)
    {
        set_pull_up_down(m_piId, gpio_pin[i], PI_PUD_DOWN);
    }
    DEBUG(INDI::Logger::DBG_SESSION, "ASI Power connected successfully.");
    return true;
}
bool IndiAsiPower::Disconnect()
{
    // Close GPIO
    pigpio_stop(m_piId);
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
    INDI::DefaultDevice::addDebugControl();
    setDriverInterface(AUX_INTERFACE);

    const std::string dev = "DEV";
    const std::string port = "Port ";
    const std::string onoff = "ONOFF";
    const std::string dutyc = "DUTYCYCLE";
    for(int i=0; i<n_gpio_pin; i++)
    {
        for(int j=0; j<n_dev_type;j++)
        {
            IUFillSwitch(&DeviceS[i][j], (dev +std::to_string(i) + std::to_string(j)).c_str(), dev_type[j].c_str(), j==0?ISS_ON:ISS_OFF);
        }
        // Label ports 1-4 using i+1 rather than 0-3
        IUFillSwitchVector(&DeviceSP[i], DeviceS[i], n_dev_type, getDeviceName(), (dev +std::to_string(i)).c_str(), (port +std::to_string(i+1)).c_str(), MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

        IUFillSwitch(&OnOffS[i][0], (onoff + std::to_string(i) +"OFF").c_str(), "Off", ISS_ON);
        IUFillSwitch(&OnOffS[i][1], (onoff + std::to_string(i) +"ON").c_str(), "On", ISS_OFF);
        IUFillSwitchVector(&OnOffSP[i], OnOffS[i], 2, getDeviceName(), (onoff + std::to_string(i)).c_str(), "On/Off", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);
    
        IUFillNumber(&DutyCycleN[i][0], (dutyc + std::to_string(i)).c_str(), "Duty Cycle %", "%0.0f", 0, max_pwm_duty, 1, 0);
        IUFillNumberVector(&DutyCycleNP[i], DutyCycleN[i], 1, getDeviceName(), (dutyc + std::to_string(i)).c_str(), "Duty Cycle", MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);
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
        for(int i=0; i<n_gpio_pin; i++)
        {
            defineProperty(&DeviceSP[i]);
            defineProperty(&OnOffSP[i]);
            defineProperty(&DutyCycleNP[i]);
        }
    }
    else
    {
        // We're disconnected
        for(int i=0; i<n_gpio_pin; i++)
        {
            deleteProperty(DeviceSP[i].name);
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
        for(int i=0; i<n_gpio_pin; i++)
        {
        // handle Duty Cycle on device i
            if (!strcmp(name, DutyCycleNP[i].name))
            {
                // verify a valid device - not of type None
                if(m_type[i] == 0)
                {
                    DutyCycleNP[i].s = IPS_ALERT;
                    IDSetNumber(&DutyCycleNP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s %s is not in use", DeviceSP[i].label, dev_type[m_type[i]].c_str());
                    return false;
                }
                // verify value is a valid Duty Cycle
                if ( values[0] < 0 || values[0] > max_pwm_duty )
                {
                    DutyCycleNP[i].s = IPS_ALERT;
                    IDSetNumber(&DutyCycleNP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "Value %0.0f is not a valid Duty Cycle!", values[0]);
                    return false;
                }
                // cannot alter duty cycle on a non-PWM device -except to maximum
                if(!dev_pwm[m_type[i]] && values[0] != max_pwm_duty)
                {
                    DutyCycleNP[i].s = IPS_ALERT;
                    IDSetNumber(&DutyCycleNP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "Cannot alter duty cycle on %s %s", DeviceSP[i].label, dev_type[m_type[i]].c_str());
                    return false;
                }
                IUUpdateNumber(&DutyCycleNP[i],values,names,n);
                DutyCycleNP[i].s = IPS_OK;
                IDSetNumber(&DutyCycleNP[i], nullptr);
                DEBUGF(INDI::Logger::DBG_SESSION, "%s %s set to duty cycle %0.0f", DeviceSP[i].label, dev_type[m_type[i]].c_str(), DutyCycleN[i][0].value);

                // If the device is ON and it is a PWM device then apply the duty cycle
                if(OnOffS[i][1].s == ISS_ON && dev_pwm[m_type[i]])
                {
                    DEBUGF(INDI::Logger::DBG_SESSION, "%s %s PWM ON %0.0f\%", DeviceSP[i].label, dev_type[m_type[i]].c_str(), DutyCycleN[i][0].value);
                    set_PWM_dutycycle(m_piId, gpio_pin[i], DutyCycleN[i][0].value);
                }
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
        for(int i=0; i<n_gpio_pin; i++)
        {
            // handle device type selection
            if (!strcmp(name, DeviceSP[i].name))
            {
                IUUpdateSwitch(&DeviceSP[i], states, names, n);
                m_type[i] = IUFindOnSwitchIndex(&DeviceSP[i]);
                DeviceSP[i].s = IPS_OK;
                IDSetSwitch(&DeviceSP[i], NULL);
                DEBUGF(INDI::Logger::DBG_SESSION, "%s New Type %s", DeviceSP[i].label, dev_type[m_type[i]].c_str());
                if(dev_pwm[m_type[i]])
                {
                    DutyCycleNP[i].s = IPS_OK;
                    set_PWM_frequency(m_piId, gpio_pin[i], pwm_freq);
                    set_PWM_range(m_piId, gpio_pin[i], max_pwm_duty);
                    IDSetNumber(&DutyCycleNP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_SESSION, "PWM device selected on %s %s", DeviceSP[i].label, dev_type[m_type[i]].c_str() );
                }
                else  // Force duty cycle to 100% for non-PWM. Cosmetic only
                {
                    DutyCycleNP[i].s = IPS_IDLE;
                    DutyCycleN[i][0].value = max_pwm_duty;
                    IDSetNumber(&DutyCycleNP[i], nullptr);
                    if(m_type[i] == 0)
                    {
                        OnOffSP[i].s = IPS_OK;
                        OnOffS[i][0].s = ISS_ON;          // Switch off if type None
                        OnOffS[i][1].s = ISS_OFF;         // Switch off if type None
                        IDSetSwitch(&OnOffSP[i], NULL);
                        gpio_write(m_piId, gpio_pin[i], PI_LOW);
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s %s disabled", DeviceSP[i].label, dev_type[m_type[i]].c_str());
                    }
                    DEBUGF(INDI::Logger::DBG_SESSION, "%d%% duty cycle set on %s %s", max_pwm_duty, DeviceSP[i].label, dev_type[m_type[i]].c_str() );
                }
            }
            // handle on/off for device i
            if (!strcmp(name, OnOffSP[i].name))
            {
                // verify a valid device - not of type None
                if(m_type[i] == 0)
                {
                    OnOffSP[i].s = IPS_ALERT;
                    IDSetSwitch(&OnOffSP[i], NULL);
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s %s is not in use", DeviceSP[i].label, dev_type[m_type[i]].c_str());
                    return false;
                }
                IUUpdateSwitch(&OnOffSP[i], states, names, n);
                // Switch OFF
                if ( OnOffS[i][0].s == ISS_ON )
                {
                    if(!dev_pwm[m_type[i]])
                    {
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s %s set to OFF", DeviceSP[i].label, dev_type[m_type[i]].c_str());
                        gpio_write(m_piId, gpio_pin[i], PI_LOW);
                    }
                    else
                    {
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s %s PWM OFF", DeviceSP[i].label, dev_type[m_type[i]].c_str() );
                        set_PWM_dutycycle(m_piId, gpio_pin[i], 0);
                    }
                    OnOffSP[i].s = IPS_IDLE;
                    IDSetSwitch(&OnOffSP[i], NULL);
                    return true;
                }
                // Switch ON
                if ( OnOffS[i][1].s == ISS_ON )
                {
                    if(!dev_pwm[m_type[i]])
                    {
                    DEBUGF(INDI::Logger::DBG_SESSION, "%s %s set to ON", DeviceSP[i].label, dev_type[m_type[i]].c_str() );
                        gpio_write(m_piId, gpio_pin[i], PI_HIGH);
                    }
                    else
                    {
                    DEBUGF(INDI::Logger::DBG_SESSION, "%s %s PWM ON %0.0f\%", DeviceSP[i].label, dev_type[m_type[i]].c_str(), DutyCycleN[i][0].value);
                        set_PWM_dutycycle(m_piId, gpio_pin[i], DutyCycleN[i][0].value);
                    }
                    OnOffSP[i].s = IPS_OK;
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

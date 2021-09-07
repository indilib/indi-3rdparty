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
#include <math.h>
#include <config.h>
#include <chrono>
#include <pigpiod_if2.h>
#include <asipower.h>

static class Loader
{
public:
    std::unique_ptr<IndiAsiPower> device;
public:
    Loader()
    {
        device.reset(new IndiAsiPower());
    }
} loader;

IndiAsiPower::IndiAsiPower()
{
    setVersion(VERSION_MAJOR,VERSION_MINOR);
    std::fill_n(m_type, n_dev_type, 0);
    dslr_isexp = false;
    dslr_counter = 0;
    timer.callOnTimeout([this](){IndiTimerCallback();});
    timer.setSingleShot(true);
}
IndiAsiPower::~IndiAsiPower()
{
    for(int i=0; i<n_gpio_pin;i++)
    {
        deleteProperty(DeviceSP[i].name);
        deleteProperty(OnOffSP[i].name);
        deleteProperty(DutyCycleNP[i].name);
    }
    deleteProperty(DslrSP.name);
    deleteProperty(DslrExpNP.name);
}
bool IndiAsiPower::Connect()
{
    // Init GPIO
    DEBUGF(INDI::Logger::DBG_DEBUG, "pigpiod_if2 version %lu.", pigpiod_if_version());
    m_piId = pigpio_start(NULL,NULL);

    if (m_piId < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "pigpio initialisation failed: %d", m_piId);
        return false;
    }
    DEBUGF(INDI::Logger::DBG_SESSION, "pigpio version %lu.", get_pigpio_version(m_piId));
    DEBUGF(INDI::Logger::DBG_DEBUG, "Hardware revision %x.", get_hardware_revision(m_piId));
    for(int i=0; i<n_gpio_pin; i++)
    {
        set_pull_up_down(m_piId, gpio_pin[i], PI_PUD_DOWN);
    }
    DEBUG(INDI::Logger::DBG_SESSION, "ASI Power connected successfully.");
    return true;
}
bool IndiAsiPower::Disconnect()
{
    DslrChange(false,true);       // Abort exposures
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
    IUFillSwitch(&DslrS[0], "DSLR_START", "Start", ISS_OFF);
    IUFillSwitch(&DslrS[1], "DSLR_STOP", "Stop", ISS_ON);
    IUFillSwitchVector(&DslrSP, DslrS, 2, getDeviceName(), "DSLR_CTRL", "DSLR ", "DSLR", IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    IUFillNumber(&DslrExpN[0], "DSLR_DUR", "Duration (s)", "%1.1f", 0, 3600, 1, 1);
    IUFillNumber(&DslrExpN[1], "DSLR_COUNT", "Count", "%0.0f", 1, 500, 1, 1);
    IUFillNumber(&DslrExpN[2], "DSLR_DELAY", "Delay (s)", "%1.1f", 0, 60, 1, 0);
    IUFillNumberVector(&DslrExpNP, DslrExpN, 3, getDeviceName(), "DSLR_EXP", "Exposure", "DSLR", IP_RW, 0, IPS_IDLE);
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
        defineProperty(&DslrSP);
        defineProperty(&DslrExpNP);
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
        deleteProperty(DslrSP.name);
        deleteProperty(DslrExpNP.name);
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
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s Duty Cycle %0.0f is not a valid value", DeviceSP[i].label, values[0]);
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
                DEBUGF(INDI::Logger::DBG_SESSION, "%s %s set to duty cycle %0.0f", DeviceSP[i].label, dev_type[m_type[i]].c_str(), DutyCycleN[i][0].value);

                // If the device is ON and it is a PWM device then apply the duty cycle
                if(OnOffS[i][1].s == ISS_ON && dev_pwm[m_type[i]])
                {
                    DEBUGF(INDI::Logger::DBG_SESSION, "%s %s PWM ON %0.0f\%", DeviceSP[i].label, dev_type[m_type[i]].c_str(), DutyCycleN[i][0].value);
                    set_PWM_dutycycle(m_piId, gpio_pin[i], DutyCycleN[i][0].value);
                }
                DutyCycleNP[i].s = IPS_OK;
                IDSetNumber(&DutyCycleNP[i], nullptr);
                return true;
            }
        }
        // handle Exposure settings: Duration, Count, Delay
        if (!strcmp(name, DslrExpNP.name))
        {
            if( DslrS[0].s == ISS_ON )
            {
                DslrExpNP.s = IPS_ALERT;
                IDSetNumber(&DslrExpNP, nullptr);
                DEBUG(INDI::Logger::DBG_ERROR, "DSLR Cannot change settings during an exposure");
                return false;
            }
            double intpart;
            IUUpdateNumber(&DslrExpNP,values,names,n);
            if(DslrExpN[0].value > 5 && modf(DslrExpN[0].value, &intpart) > 0)
            {
                DEBUGF(INDI::Logger::DBG_WARNING, "DSLR Duration %0.2f > 5.0 s rounded to nearest integer", DslrExpN[0].value);
                DslrExpN[0].value = round(DslrExpN[0].value);
            }
            if(DslrExpN[1].value < 1)
            {
                DEBUGF(INDI::Logger::DBG_WARNING, "DSLR Count %0.0f is less than 1", DslrExpN[1].value);
            }
            if(DslrExpN[2].value > 5 && modf(DslrExpN[2].value, &intpart) > 0)
            {
                DEBUGF(INDI::Logger::DBG_WARNING, "DSLR Delay %0.2f > 5.0 rounded to nearest integer", DslrExpN[2].value);
                DslrExpN[2].value = round(DslrExpN[2].value);
            }
            DEBUGF(INDI::Logger::DBG_SESSION, "DSLR Duration %0.2f s Count %0.0f Delay %0.2f s", DslrExpN[0].value, DslrExpN[1].value, DslrExpN[2].value);
            DslrExpNP.s = IPS_OK;
            IDSetNumber(&DslrExpNP, nullptr);
            return true;
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
                DeviceSP[i].s = IPS_OK;
                IDSetSwitch(&DeviceSP[i], NULL);
                return true;
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
                OnOffSP[i].s = IPS_ALERT;
                IDSetSwitch(&OnOffSP[i], nullptr);
                return false;
            }
        }
        // Start and stop exposures
        if (!strcmp(name, DslrSP.name))
        {
            IUUpdateSwitch(&DslrSP, states, names, n);
            if( DslrS[0].s == ISS_ON )
            {
                DslrSP.s = IPS_OK;
                IDSetSwitch(&DslrSP, nullptr);
                DEBUGF(INDI::Logger::DBG_SESSION, "DSLR Start Exposure: Duration %0.2f s Count %0.0f Delay %0.2f s", DslrExpN[0].value, DslrExpN[1].value, DslrExpN[2].value);
                DslrChange(true);
                DslrExpNP.s = IPS_BUSY;
                IDSetNumber(&DslrExpNP, nullptr);
                return true;
            }
            if( DslrS[1].s == ISS_ON )
            {
                DslrSP.s = IPS_IDLE;
                IDSetSwitch(&DslrSP, nullptr);
                DslrChange(false, true);
                DEBUG(INDI::Logger::DBG_SESSION, "DSLR Stop exposure");
                DslrExpNP.s = IPS_IDLE;
                IDSetNumber(&DslrExpNP, nullptr);
                return true;
            }
            DslrSP.s = IPS_ALERT;
            IDSetSwitch(&DslrSP, nullptr);
            return false;
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
    IUSaveConfigNumber(fp, &DslrExpNP);
    return true;
}

void IndiAsiPower::DslrChange(bool isInit, bool abort)
{
    gpio_write(m_piId, dslr_pin, PI_LOW);
    timer.stop();
    auto now = std::chrono::system_clock::now();
    if (isInit)
    {
        dslr_counter = DslrExpN[1].value + 1;
        DEBUGF(INDI::Logger::DBG_DEBUG, "DSLR SEQ INIT: Counter %d", dslr_counter);
        dslr_isexp = true;
    }
    else
    {
    // integral duration: requires duration_cast
        auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - dslr_start);
        DEBUGF(INDI::Logger::DBG_SESSION, "DSLR END: %s timer: Duration %d ms, Counter %d", dslr_isexp ? "Expose":"Delay", int_ms, dslr_counter);
    }
    if (dslr_isexp)
    {
        dslr_counter--;
    }
    if(abort)
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "DSLR SEQ ABORT: %s Counter %d", dslr_isexp ? "Expose":"Delay", dslr_counter);
        dslr_counter = 0;
    }
    dslr_isexp =  ! dslr_isexp;

    if (dslr_counter <= 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "DSLR SEQ END: %s Counter %d", dslr_isexp ? "Expose":"Delay", dslr_counter);
        DslrS[0].s = ISS_OFF;
        DslrS[1].s = ISS_ON;
        DslrSP.s = IPS_IDLE;
        IDSetSwitch(&DslrSP, nullptr);
        DslrExpNP.s = IPS_IDLE;
        IDSetNumber(&DslrExpNP, nullptr);
        return;
    }
    uint32_t l_duration = (dslr_isexp ? DslrExpN[0].value : DslrExpN[2].value)*1000;

    if (l_duration > 0) // non-zero duration
    {
        if(l_duration > max_timer_ms) l_duration = max_timer_ms;
        gpio_write(m_piId, dslr_pin, dslr_isexp ? PI_HIGH: PI_LOW);
        timer.start(l_duration);
        dslr_start = std::chrono::system_clock::now();
        DEBUGF(INDI::Logger::DBG_SESSION, "DSLR START %s timer: Duration %d ms", dslr_isexp ? "Expose":"Delay", l_duration);
    }
    else
    {
        if (dslr_isexp)
        {
            DEBUG(INDI::Logger::DBG_ERROR, "DSLR Zero length exposure requested");
        }
        else
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "DSLR START %s timer: zero length duration %d ms", dslr_isexp ? "Expose":"Delay", l_duration);
            DslrChange();  // Handle a zero length delay
        }
    }
    return;
}

void IndiAsiPower::IndiTimerCallback()
{
    DEBUG(INDI::Logger::DBG_DEBUG, "DSLR callback: Timer ended");
    DslrChange();  // Handle end of timer
    return;
}

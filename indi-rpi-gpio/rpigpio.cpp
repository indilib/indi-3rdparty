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
#include <pigpiod_if2.h>
#include <rpigpio.h>

// We declare an auto pointer to IndiRpiGpio
std::unique_ptr<IndiRpiGpio> device;

void ISPoll(void *p);

void ISInit()
{
    static int isInit = 0;

    if (isInit == 1)
        return;
    if(device.get() == 0)
    {
        isInit = 1;
        device.reset(new IndiRpiGpio());
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

static void TimerCallback(int pi, unsigned user_gpio, unsigned level, uint32_t tick)
{
    device->TimerCallback(pi, user_gpio, level, tick);
}

IndiRpiGpio::IndiRpiGpio()
{
    setVersion(VERSION_MAJOR,VERSION_MINOR);
    std::fill_n(m_gpio_pin, n_gpio_pin, -1);
    std::fill_n(m_type, n_dev_type, 0);
    std::fill_n(timer_end, n_gpio_pin, 0);
    std::fill_n(timer_last, n_gpio_pin, 0);
    std::fill_n(timer_isexp, n_gpio_pin, 0);
    std::fill_n(timer_end, n_gpio_pin, 0);
    std::fill_n(timer_cb, n_gpio_pin, -1);

}

IndiRpiGpio::~IndiRpiGpio()
{
    for(int i=0; i<n_gpio_pin;i++)
    {
        deleteProperty(GpioPinSP[i].name);
        deleteProperty(LabelTP[i].name);
        deleteProperty(DeviceSP[i].name);
        deleteProperty(OnOffSP[i].name);
        deleteProperty(ActiveSP[i].name);
        deleteProperty(DutyCycleNP[i].name);
        deleteProperty(TimerOnNP[i].name);
    }
    for(int i=0; i<n_gpio_pin;i++)
    {
        if(timer_cb[i] >= 0)
        {
            callback_cancel(timer_cb[i]);
            timer_cb[i] = -1;
        }
    }
    pigpio_stop(m_piId);
}

int IndiRpiGpio::InitPiModel()
{
    // Init GPIO
    DEBUGF(INDI::Logger::DBG_DEBUG, "pigpiod_if2 version %lu.", pigpiod_if_version());

    m_piId = pigpio_start(NULL,NULL);

    if (m_piId < 0)
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "pigpio initialisation failed: %d", m_piId);
        return -1;
    }
    uint32_t hw_rev=get_hardware_revision(m_piId);
    DEBUGF(INDI::Logger::DBG_DEBUG, "pigpio version %lu.", get_pigpio_version(m_piId));
    DEBUGF(INDI::Logger::DBG_DEBUG, "Hardware revision %x.", hw_rev);

// Restrict GPIO pins based on hardware revision
    bool new_style = (hw_rev & 0x800000) >> 23;
    uint32_t rpi_type = (hw_rev & 0xff0) >> 4;
    uint32_t rpi_rev = (hw_rev & 0x0f);
    DEBUGF(INDI::Logger::DBG_DEBUG, "New style %d Type %x Rev %x.", new_style, rpi_type, rpi_rev);
    if (!new_style)
    {
        if (rpi_rev >= 2 && rpi_rev <= 3) // RPi 1
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Raspberry Pi original");
            n_valid_gpio = pi1_ngpio;
            valid_gpio_pin = pi1_gpio;
        }
        else if(rpi_rev >=4 && rpi_rev <= 9) // RPi 2
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Raspberry Pi 2");
            n_valid_gpio = pi2_ngpio;
            valid_gpio_pin = pi2_gpio;
        }
        else if(rpi_rev >=13 && rpi_rev <= 15) // RPi 2
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Raspberry Pi 2");
            n_valid_gpio = pi2_ngpio;
            valid_gpio_pin = pi2_gpio;
        }
        else
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Unknown Raspberry Pi model");
            return -1;
        }
    }
    else
    {
        if(/*rpi_type >= 0 &&*/ rpi_type <= 3) // RPi 1
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Raspberry Pi original");
            n_valid_gpio = pi1_ngpio;
            valid_gpio_pin = pi1_gpio;
        }
        else if(rpi_type == 4) // RPi 2
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Raspberry Pi 2");
            n_valid_gpio = pi2_ngpio;
            valid_gpio_pin = pi2_gpio;
        }
        else if(rpi_type == 8) // RPi 3
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Raspberry Pi 3");
            n_valid_gpio = pi3_ngpio;
            valid_gpio_pin = pi3_gpio;
        }
        else if (rpi_type >= 13 && rpi_type <= 14) // RPi 3
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Raspberry Pi 3");
            n_valid_gpio = pi3_ngpio;
            valid_gpio_pin = pi3_gpio;
        }
        else if(rpi_type == 17 || rpi_type == 19) // RPi 4
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Raspberry Pi 4");
            n_valid_gpio = pi4_ngpio;
            valid_gpio_pin = pi4_gpio;
        }
        else if(rpi_type == 9 || rpi_type == 12) // RPi Zero
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Raspberry Pi Zero");
        }
        else
        {
            DEBUG(INDI::Logger::DBG_DEBUG, "Unknown Raspberry Pi model");
            return -1;
        }
    }
    return 0;
}


bool IndiRpiGpio::Connect()
{
    if(m_piId < 0)
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to connect");
        return false;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "RPi GPIO connected successfully.");
    return true;
}
bool IndiRpiGpio::Disconnect()
{
    // Close GPIO
    for(int i=0; i<n_gpio_pin;i++)
    {
        if(timer_cb[i] >= 0)
        {
            callback_cancel(timer_cb[i]);
            timer_cb[i] = -1;
        }
    }
    DEBUG(INDI::Logger::DBG_SESSION, "RPi GPIO disconnected successfully.");
    return true;
}
const char * IndiRpiGpio::getDefaultName()
{
    return (char *)"RPi GPIO";
}
bool IndiRpiGpio::initProperties()
{
    // We init parent properties first
    INDI::DefaultDevice::initProperties();
    INDI::DefaultDevice::addDebugControl();
    setDriverInterface(AUX_INTERFACE);

    if(InitPiModel())
    {
        DEBUG(INDI::Logger::DBG_ERROR, "Failed to initialize Raspberry Pi");
        return false;
    }
    const std::string dev = "DEV";
    const std::string label = "LABEL";
    const std::string port = "Port ";
    const std::string pin = "PIN";
    const std::string gpio = "GPIO#";

    const std::string onoff = "ONOFF";
    const std::string active = "ACTIVE";
    const std::string dutyc = "DUTYCYCLE";
    const std::string duration = "DURATION";
    const std::string timedpulse = "TIMEDPULSE";
    const std::string count = "COUNT";
    const std::string delay = "DELAY";
    for(int i=0; i<n_gpio_pin; i++)
    {
        for(int j=0; j<n_valid_gpio;j++)
        {
            IUFillSwitch(&GpioPinS[i][j], (pin +std::to_string(i) + std::to_string(j)).c_str(), j==0?"Not in use":(gpio + std::to_string(valid_gpio_pin[j])).c_str(), ISS_OFF);
        }
        IUFillSwitchVector(&GpioPinSP[i], GpioPinS[i], n_valid_gpio, getDeviceName(), (pin +std::to_string(i)).c_str(), (port +std::to_string(i+1)).c_str(), PIN_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

        for(int j=0; j<n_dev_type;j++)
        {
            IUFillSwitch(&DeviceS[i][j], (dev +std::to_string(i) + std::to_string(j)).c_str(), dev_type[j].c_str(), j==0?ISS_ON:ISS_OFF);
        }
        // Label ports 1-4 using i+1 rather than 0-3
        IUFillSwitchVector(&DeviceSP[i], DeviceS[i], n_dev_type, getDeviceName(), (dev +std::to_string(i)).c_str(), (port +std::to_string(i+1)).c_str(), PIN_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

        IUFillSwitch(&ActiveS[i][0], (active + std::to_string(i) +"HI").c_str(), "Active High", ISS_ON);
        IUFillSwitch(&ActiveS[i][1], (active + std::to_string(i) +"LO").c_str(), "Active Low", ISS_OFF);
        IUFillSwitchVector(&ActiveSP[i], ActiveS[i], 2, getDeviceName(), (active + std::to_string(i)).c_str(), (port +std::to_string(i+1)).c_str(), PIN_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

        IUFillText(&LabelT[i][0], (label + std::to_string(i)).c_str(), "Label", "Device name");
        IUFillTextVector(&LabelTP[i], LabelT[i], 1, getDeviceName(), (label + std::to_string(i)).c_str(), (port +std::to_string(i+1)).c_str(), MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

        IUFillSwitch(&OnOffS[i][0], (onoff + std::to_string(i) +"OFF").c_str(), "Off", ISS_ON);
        IUFillSwitch(&OnOffS[i][1], (onoff + std::to_string(i) +"ON").c_str(), "On", ISS_OFF);
        IUFillSwitchVector(&OnOffSP[i], OnOffS[i], 2, getDeviceName(), (onoff + std::to_string(i)).c_str(), (port +std::to_string(i+1)).c_str(), MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

        IUFillNumber(&DutyCycleN[i][0], (dutyc + std::to_string(i)).c_str(), "Duty Cycle %", "%0.0f", 0, max_pwm_duty, 1, 0);
        IUFillNumberVector(&DutyCycleNP[i], DutyCycleN[i], 1, getDeviceName(), (dutyc + std::to_string(i)).c_str(), (port +std::to_string(i+1)).c_str(), MAIN_CONTROL_TAB, IP_RW, 0, IPS_IDLE);

        IUFillNumber(&TimerOnN[i][0], (duration + std::to_string(i)).c_str(), "Duration (s)", "%1.1f", 0, 3600, 1, 1);
        IUFillNumber(&TimerOnN[i][1], (count + std::to_string(i)).c_str(), "Count", "%0.0f", 1, 500, 1, 1);
        IUFillNumber(&TimerOnN[i][2], (delay + std::to_string(i)).c_str(), "Delay (s)", "%1.1f", 0, 60, 1, 0);
        IUFillNumberVector(&TimerOnNP[i], TimerOnN[i], 3, getDeviceName(), (timedpulse + std::to_string(i)).c_str(), (port +std::to_string(i+1)).c_str(), TIMER_TAB, IP_RW, 0, IPS_IDLE);
    }
    loadConfig();

    return true;
}
bool IndiRpiGpio::updateProperties()
{
    // Call parent update properties first
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // We're connected
        for(int i=0; i<n_gpio_pin; i++)
        {
            defineProperty(&GpioPinSP[i]);
            defineProperty(&LabelTP[i]);
            defineProperty(&DeviceSP[i]);
            defineProperty(&OnOffSP[i]);
            defineProperty(&DutyCycleNP[i]);
            defineProperty(&ActiveSP[i]);
            defineProperty(&TimerOnNP[i]);
        }
    }
    else
    {
        // We're disconnected
        for(int i=0; i<n_gpio_pin; i++)
        {
            deleteProperty(GpioPinSP[i].name);
            deleteProperty(LabelTP[i].name);
            deleteProperty(DeviceSP[i].name);
            deleteProperty(OnOffSP[i].name);
            deleteProperty(ActiveSP[i].name);
            deleteProperty(DutyCycleNP[i].name);
            deleteProperty(TimerOnNP[i].name);
        }
    }
    return true;
}

void IndiRpiGpio::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);
}

bool IndiRpiGpio::ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    // first we check if it's for our device
    if(strcmp(dev,getDeviceName())==0)
    {
        for(int i=0; i<n_gpio_pin; i++)
        {
        // Port labels
            if (!strcmp(name, LabelTP[i].name))
            {
                // verify texts[0] is a valid label
                if ( strlen(texts[0]) <= 0)
                {
                    LabelTP[i].s = IPS_ALERT;
                    IDSetText(&LabelTP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s has zero length label %s", DeviceSP[i].label, texts[0]);
                    return false;
                }
                IUUpdateText(&LabelTP[i],texts,names,n);
                LabelTP[i].s = IPS_OK;
                IDSetText(&LabelTP[i], nullptr);
                DEBUGF(INDI::Logger::DBG_SESSION, "%s Label set to %s", DeviceSP[i].label, LabelT[i][0].text);
                return true;
            }
        }
    }
    return INDI::DefaultDevice::ISNewText (dev, name, texts, names, n);
}

bool IndiRpiGpio::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    // first we check if it's for our device
    if(strcmp(dev,getDeviceName())==0)
    {
        for(int i=0; i<n_gpio_pin; i++)
        {
        // handle Duty Cycle on device i
            if (!strcmp(name, DutyCycleNP[i].name))
            {
                // If the port is unassigned do not allow changes
                if(m_gpio_pin[i] < 0 )
                {
                    DutyCycleNP[i].s = IPS_ALERT;
                    IDSetNumber(&DutyCycleNP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s type %s Cannot change duty cycle on unused GPIO", DeviceSP[i].label );
                    return false;
                }
                // cannot alter duty cycle on a non-PWM device -except to maximum
                if(!dev_pwm[m_type[i]] && values[0] != max_pwm_duty)
                {
                    DutyCycleNP[i].s = IPS_ALERT;
                    IDSetNumber(&DutyCycleNP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s type %s GPIO# %d Cannot change duty cycle", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i]);
                    return false;
                }
                // verify value is a valid Duty Cycle
                if ( values[0] < 0 || values[0] > max_pwm_duty )
                {
                    DutyCycleNP[i].s = IPS_ALERT;
                    IDSetNumber(&DutyCycleNP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s type %s GPIO# %d %0.0f\% is not a valid duty cycle!", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i], values[0]);
                    return false;
                }
                IUUpdateNumber(&DutyCycleNP[i],values,names,n);
                DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# set to duty cycle %0.0f\%", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i], DutyCycleN[i][0].value);

                // If the device is ON and it is a PWM device then apply the duty cycle
                if(OnOffS[i][1].s == ISS_ON && dev_pwm[m_type[i]])
                {
                    DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# %d PWM ON with duty cycle %0.0f\%", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i], DutyCycleN[i][0].value);
                    // If Active LOW then the duty cycle is the complement of the Active HIGH 
                    set_PWM_dutycycle(m_piId, m_gpio_pin[i], (ActiveS[i][0].s == ISS_ON)? DutyCycleN[i][0].value: max_pwm_duty - DutyCycleN[i][0].value);
                }
                DutyCycleNP[i].s = IPS_OK;
                IDSetNumber(&DutyCycleNP[i], nullptr);
                return true;
            }
            // handle Exposure settings: Duration, Count, Delay
            if (!strcmp(name, TimerOnNP[i].name))
            {
                // If the port is unassigned do not allow changes
                if(m_gpio_pin[i] < 0 )
                {
                    TimerOnNP[i].s = IPS_ALERT;
                    IDSetNumber(&TimerOnNP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s type %s Duration cannot be changed on an unassigned GPIO", DeviceSP[i].label, dev_type[m_type[i]].c_str() );
                    return false;
                }
                // cannot alter exposure on a non-timer device -except to maximum
                //FIXME error only of exposure changes
                if(!dev_timer[m_type[i]] && TimerOnN[i][0].value != values[0])
                {
                    TimerOnNP[i].s = IPS_ALERT;
                    IDSetNumber(&TimerOnNP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s type %s GPIO# %d cannot change duration on untimed port", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i]);
                    return false;
                }
                if( OnOffS[i][1].s == ISS_ON )
                {
                    TimerOnNP[i].s = IPS_ALERT;
                    IDSetNumber(&TimerOnNP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s type %s GPIO# %d Cannot change duration when port is ON", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i] );
                    return false;
                }
                double intpart;
                IUUpdateNumber(&TimerOnNP[i],values,names,n);
                if(TimerOnN[i][0].value > 5 && modf(TimerOnN[i][0].value, &intpart) > 0)
                {
                    DEBUGF(INDI::Logger::DBG_WARNING, "%s type %s GPIO# %d duration %0.2f > 5.0 s rounded to nearest integer", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i], TimerOnN[i][0].value);
                    TimerOnN[i][0].value = round(TimerOnN[i][0].value);
                }
                if(TimerOnN[i][1].value < 1)
                {
                    DEBUGF(INDI::Logger::DBG_WARNING, "%s type %s GPIO# %d count %0.0f is less than 1", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i], TimerOnN[i][1].value);
                }
                if(TimerOnN[i][2].value > 5 && modf(TimerOnN[i][2].value, &intpart) > 0)
                {
                    DEBUGF(INDI::Logger::DBG_WARNING, "%s type %s GPIO# %d delay %0.2f > 5.0 rounded to nearest integer", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i], TimerOnN[i][2].value);
                    TimerOnN[i][2].value = round(TimerOnN[i][2].value);
                }
                TimerOnNP[i].s = IPS_OK;
                IDSetNumber(&TimerOnNP[i], nullptr);
                DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# %d Duration %0.2f s Count %0.0f Delay %0.2f s", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i], TimerOnN[i][0].value, TimerOnN[i][1].value, TimerOnN[i][2].value);
                return true;
            }
        }
    }
    return INDI::DefaultDevice::ISNewNumber(dev,name,values,names,n);
}
bool IndiRpiGpio::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // first we check if it's for our device
    if (!strcmp(dev, getDeviceName()))
    {
        for(int i=0; i<n_gpio_pin; i++)
        {
            // handle GPIO assignment
            if (!strcmp(name, GpioPinSP[i].name))
            {
                // If the device is ON do not allow changes
                if(OnOffS[i][1].s == ISS_ON)
                {
                    GpioPinSP[i].s = IPS_ALERT;
                    IDSetSwitch(&GpioPinSP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s type %s GPIO# %d GPIO cannot be changed while device is ON", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i] );
                    return false;
                }
// Do not allow same GPIO on multiple ports
// Identify the selected GPIO
                int npin=-1;
                for(int k=0; k<n; k++)
                    if(states[k] == ISS_ON)
                        for(int j=0; j<n_valid_gpio;j++)
                            if(!strcmp(GpioPinS[i][j].name, names[k])) npin=j;
                int l_gpio_pin = npin >= 0?valid_gpio_pin[npin] : -1;

// Look for the selected GPIO amongst the assigned GPIOs
                int found = FindPinIndex(l_gpio_pin);
                if (found >= 0 && found != i && l_gpio_pin >= 0)
                {
                    GpioPinSP[i].s = IPS_ALERT;
                    IDSetSwitch(&GpioPinSP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s type %s GPIO# %d is already assigned on port %d", DeviceSP[i].label, dev_type[m_type[i]].c_str(), l_gpio_pin, found );
                    return false;
                }
                IUUpdateSwitch(&GpioPinSP[i], states, names, n);

                // See if the GPIO has changed
                if( m_gpio_pin[i] != l_gpio_pin)
                {
                    if(timer_cb[i] >= 0)    // Cancel the timer on the old pin
                    {
                        callback_cancel(timer_cb[i]);
                        timer_cb[i] = -1;
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# %d timer cancelled", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i] );
                    }
                    if(dev_pwm[m_type[i]])     // Cancel the PWM on the old pin (not really needed if switched off)
                    {
                    // If Active LOW then the duty cycle is the complement of the Active HIGH 
                        set_PWM_dutycycle(m_piId, m_gpio_pin[i], (ActiveS[i][0].s == ISS_ON)? 0: max_pwm_duty);
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# %d PWM disabled", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i] );
                    }
                    if(m_type[i] > 0)          // Switch off the old pin (not really needed if switched off)
                    {
                        gpio_write(m_piId, m_gpio_pin[i], (ActiveS[i][0].s == ISS_ON)? PI_LOW: PI_HIGH);
                    }

                    m_gpio_pin[i] = l_gpio_pin;
                    set_pull_up_down(m_piId, m_gpio_pin[i], PI_PUD_DOWN);  // Ensure Pull Up/Down set to Pull Down
                    gpio_write(m_piId, m_gpio_pin[i], (ActiveS[i][0].s == ISS_ON)? PI_LOW: PI_HIGH);  // Assume OFF
                    if(dev_timer[m_type[i]])   // Create a timer on the new pin
                    {
                        if(m_gpio_pin[i] >= 0 && timer_cb[i] < 0)
                        {
                            timer_cb[i] = callback(m_piId, m_gpio_pin[i], EITHER_EDGE, ::TimerCallback);
                            DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# %d timer callback set", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i] );
                        }
                    }
                    if(dev_pwm[m_type[i]])    // Set up PWM on the new pin
                    {
                        set_PWM_frequency(m_piId, m_gpio_pin[i], pwm_freq);
                        set_PWM_range(m_piId, m_gpio_pin[i], max_pwm_duty);
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# %d set to PWM", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i] );
                    }
                }
                GpioPinSP[i].s = IPS_OK;
                IDSetSwitch(&GpioPinSP[i], NULL);
                DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# %d assigned", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i]);
                return true;
            }
            // handle device type selection
            if (!strcmp(name, DeviceSP[i].name))
            {
                // If the port is unassigned do not allow changes
                if(m_gpio_pin[i] < 0 )
                {
                    DeviceSP[i].s = IPS_ALERT;
                    IDSetSwitch(&DeviceSP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s type %s cannot be changed on an unassigned GPIO", DeviceSP[i].label, dev_type[m_type[i]].c_str() );
                    return false;
                }
                // If the device is ON do not allow changes
                if(OnOffS[i][1].s == ISS_ON)
                {
                    DeviceSP[i].s = IPS_ALERT;
                    IDSetSwitch(&DeviceSP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s type %s GPIO# %d cannot be changed while device is ON", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i] );
                    return false;
                }
                IUUpdateSwitch(&DeviceSP[i], states, names, n);
                int l_type = IUFindOnSwitchIndex(&DeviceSP[i]);
                if(m_type[i] != l_type)
                {
                    if(dev_timer[m_type[i]] && !dev_timer[l_type])    // Cancel the timer
                    {
                        callback_cancel(timer_cb[i]);
                        timer_cb[i] = -1;
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# %d timer cancelled", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i] );
                    }
                    if(dev_pwm[m_type[i]] && !dev_pwm[l_type])         // Cancel the PWM
                    {
                    // If Active LOW then the duty cycle is the complement of the Active HIGH 
                        set_PWM_dutycycle(m_piId, m_gpio_pin[i], (ActiveS[i][0].s == ISS_ON)? 0: max_pwm_duty);
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# %d PWM disabled", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i] );
                    }
                    m_type[i] = l_type;
                    if(dev_timer[m_type[i]] && timer_cb[i] < 0)   // Create a timer for the new type
                    {
                        if(m_gpio_pin[i] >= 0)
                        {
                            timer_cb[i] = callback(m_piId, m_gpio_pin[i], EITHER_EDGE, ::TimerCallback);
                        }
                    }
                    if(dev_pwm[m_type[i]])    // Set up PWM on the new pin
                    {
                        set_PWM_frequency(m_piId, m_gpio_pin[i], pwm_freq);
                        set_PWM_range(m_piId, m_gpio_pin[i], max_pwm_duty);
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# %d PWM enabled", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i] );
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
                            gpio_write(m_piId, m_gpio_pin[i], (ActiveS[i][0].s == ISS_ON)? PI_LOW: PI_HIGH);
                            DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# %d set", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i]);
                        }
                    }
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
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s type %s GPIO# %d cannot switch on when not in use", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i]);
                    return false;
                }
                IUUpdateSwitch(&OnOffSP[i], states, names, n);
                // Switch OFF
                if ( OnOffS[i][0].s == ISS_ON )
                {
                    if(!dev_pwm[m_type[i]])
                    {
                        if(!dev_timer[m_type[i]])
                        {
                            DEBUGF(INDI::Logger::DBG_SESSION, "%s %s GPIO# %d set to OFF (%s)", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i], (ActiveS[i][0].s == ISS_ON)? "LO": "HI");
                            gpio_write(m_piId, m_gpio_pin[i], (ActiveS[i][0].s == ISS_ON)? PI_LOW: PI_HIGH );
                        }
                        else
                        {
                            TimerChange(m_gpio_pin[i], false, true);
                            DEBUG(INDI::Logger::DBG_SESSION, "Timer Stop exposure");
                            TimerOnNP[i].s = IPS_IDLE;
                            IDSetNumber(&TimerOnNP[i], nullptr);
                        }
                    }
                    else
                    {
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# %d PWM OFF", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i] );
                    // If Active LOW then the duty cycle is the complement of the Active HIGH 
                        set_PWM_dutycycle(m_piId, m_gpio_pin[i], (ActiveS[i][0].s == ISS_ON)? 0: max_pwm_duty);
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
                        if(!dev_timer[m_type[i]])
                        {
                            DEBUGF(INDI::Logger::DBG_SESSION, "%s %s GPIO# %d set to ON (%s)", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i], (ActiveS[i][0].s == ISS_ON)? "HI": "LO");
                            gpio_write(m_piId, m_gpio_pin[i], (ActiveS[i][0].s == ISS_ON)? PI_HIGH: PI_LOW);
                        }
                        else
                        {
                            DEBUGF(INDI::Logger::DBG_SESSION, "%s %s GPIO# %d start timer: Duration %0.2f s Count %0.0f Delay %0.2f s", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i], TimerOnN[i][0].value, TimerOnN[i][1].value, TimerOnN[i][2].value);
                            TimerChange(m_gpio_pin[i], true);
                            TimerOnNP[i].s = IPS_BUSY;
                            IDSetNumber(&TimerOnNP[i], nullptr);
                        }
                    }
                    else
                    {
                        DEBUGF(INDI::Logger::DBG_SESSION, "%s %s GPIO# %d PWM ON with duty cycle %0.0f\%", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i], DutyCycleN[i][0].value);
                    // If Active LOW then the duty cycle is the complement of the Active HIGH 
                        set_PWM_dutycycle(m_piId, m_gpio_pin[i], (ActiveS[i][0].s == ISS_ON)? DutyCycleN[i][0].value: max_pwm_duty - DutyCycleN[i][0].value);
                    }
                    OnOffSP[i].s = IPS_OK;
                    IDSetSwitch(&OnOffSP[i], NULL);
                    return true;
                }
            }
            // handle active Hi/Lo for device i
            if (!strcmp(name, ActiveSP[i].name))
            {
                // If the device is ON do not allow changes
                if(OnOffS[i][1].s == ISS_ON)
                {
                    ActiveSP[i].s = IPS_ALERT;
                    IDSetSwitch(&ActiveSP[i], nullptr);
                    DEBUGF(INDI::Logger::DBG_ERROR, "%s type %s GPIO# %d Parity cannot be changed while device is ON", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i] );
                    return false;
                }
//                // verify a valid device - not of type None
//                if(m_type[i] == 0)
//                {
//                    ActiveSP[i].s = IPS_ALERT;
//                    IDSetSwitch(&ActiveSP[i], NULL);
//                    DEBUGF(INDI::Logger::DBG_ERROR, "%s type %s GPIO# %d cannot set active parity when not in use", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i]);
//                    return false;
//                }
                IUUpdateSwitch(&ActiveSP[i], states, names, n);
                
                // Set Active High
                if ( ActiveS[i][0].s == ISS_ON )
                {
//                    set_pull_up_down(m_piId, m_gpio_pin[i], PI_PUD_DOWN);
                    gpio_write(m_piId, m_gpio_pin[i], PI_LOW );    // Assumed in OFF state
                    ActiveSP[i].s = IPS_OK;
                    IDSetSwitch(&ActiveSP[i], NULL);
                    DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# %d parity is active HIGH", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i]);
                    return true;
                }
                // Seet Active Low
                if ( ActiveS[i][1].s == ISS_ON )
                {
//                    set_pull_up_down(m_piId, m_gpio_pin[i], PI_PUD_UP);
                    gpio_write(m_piId, m_gpio_pin[i], PI_HIGH );   // Assumed in OFF state
                    ActiveSP[i].s = IPS_OK;
                    IDSetSwitch(&ActiveSP[i], NULL);
                    DEBUGF(INDI::Logger::DBG_SESSION, "%s type %s GPIO# %d parity is active LOW", DeviceSP[i].label, dev_type[m_type[i]].c_str(), m_gpio_pin[i]);
                    return true;
                }
            }
        }
    }
    return INDI::DefaultDevice::ISNewSwitch (dev, name, states, names, n);
}
bool IndiRpiGpio::ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    return INDI::DefaultDevice::ISNewBLOB (dev, name, sizes, blobsizes, blobs, formats, names, n);
}
bool IndiRpiGpio::ISSnoopDevice(XMLEle *root)
{
    return INDI::DefaultDevice::ISSnoopDevice(root);
}
bool IndiRpiGpio::saveConfigItems(FILE *fp)
{
    for(int i=0; i<n_gpio_pin; i++)
    {
        IUSaveConfigSwitch(fp, &GpioPinSP[i]);
        IUSaveConfigText(fp, &LabelTP[i]);
        IUSaveConfigSwitch(fp, &DeviceSP[i]);
        IUSaveConfigSwitch(fp, &OnOffSP[i]);
        IUSaveConfigSwitch(fp, &ActiveSP[i]);
        IUSaveConfigNumber(fp, &DutyCycleNP[i]);
        IUSaveConfigNumber(fp, &TimerOnNP[i]);
    }
    return true;
}

void IndiRpiGpio::TimerChange(unsigned user_gpio, bool isInit, bool abort)
{
    int i = FindPinIndex(user_gpio);
    gpio_write(m_piId, user_gpio, (ActiveS[i][0].s == ISS_ON)? PI_LOW: PI_HIGH);
    set_watchdog(m_piId, user_gpio, 0);
    if(i < 0 || !dev_timer[m_type[i]])
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "TimerChange: Invalid GPIO or not timed %lu", user_gpio);
    }
    if (isInit)
    {
        timer_counter[i] = TimerOnN[i][1].value + 1;
        DEBUGF(INDI::Logger::DBG_DEBUG, "Timer SEQ INIT: Counter %d", timer_counter[i]);
        timer_isexp[i] = true;
    }
    else
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Timer END: %s timer: Counter %d", timer_isexp ? "Expose":"Delay", timer_counter[i]);
    }
    if (timer_isexp[i])
    {
        timer_counter[i]--;
    }
    if(abort)
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "Timer SEQ ABORT: %s Counter %d", timer_isexp ? "Expose":"Delay", timer_counter[i]);
        timer_counter[i] = 0;
    }
    timer_isexp[i] =  ! timer_isexp[i];

    if (timer_counter[i] <= 0)
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Timer SEQ END: %s Counter %d", timer_isexp[i] ? "Expose":"Delay", timer_counter[i]);
        OnOffS[i][0].s = ISS_ON;
        OnOffS[i][1].s = ISS_OFF;
        OnOffSP[i].s = IPS_IDLE;
        IDSetSwitch(&OnOffSP[i], nullptr);
        TimerOnNP[i].s = IPS_IDLE;
        IDSetNumber(&TimerOnNP[i], nullptr);
        return;
    }
    uint32_t l_duration = (timer_isexp[i] ? TimerOnN[i][0].value : TimerOnN[i][2].value)*1000;

    timer_last[i] = get_current_tick(m_piId);
    timer_end[i] = static_cast<uint64_t>(timer_last[i]) + static_cast<uint64_t>(l_duration*1000);

    if (l_duration > 0) // non-zero duration
    {
        if(l_duration > max_timer_ms) l_duration = max_timer_ms;
        gpio_write(m_piId, user_gpio, timer_isexp[i] ? ((ActiveS[i][0].s == ISS_ON)? PI_HIGH: PI_LOW): ((ActiveS[i][0].s == ISS_ON)? PI_LOW: PI_HIGH));
        set_watchdog(m_piId, user_gpio, l_duration);
        DEBUGF(INDI::Logger::DBG_DEBUG, "Timer START %s timer: Last tick %lu ms End tick %lu ms", timer_isexp[i] ? "Expose":"Delay", timer_last[i]/1000, timer_end[i]/1000);
        DEBUGF(INDI::Logger::DBG_SESSION, "Timer START %s timer: Duration %d ms", timer_isexp[i] ? "Expose":"Delay", l_duration);
    }
    else
    {
        if (timer_isexp[i])
        {
            DEBUG(INDI::Logger::DBG_ERROR, "Timer Zero length exposure requested");
        }
        else
        {
            DEBUGF(INDI::Logger::DBG_SESSION, "Timer START %s timer: zero length duration %d ms", timer_isexp[i] ? "Expose":"Delay", l_duration);
            TimerChange(user_gpio);  // Handle a zero length delay
        }
    }
    return;
}

void IndiRpiGpio::TimerCallback(int pi, unsigned user_gpio, unsigned level, uint32_t tick)
{
    int i = FindPinIndex(user_gpio);
    if(pi != m_piId || i < 0 || level != PI_TIMEOUT)
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "Timer callback: Other Callback received for Id %d GPIO %d level %d", pi, user_gpio, level);
        return;
    }
    int32_t tick_ms = tick/1000;
    int32_t last_ms = timer_last[i]/1000;
    int64_t end_ms = timer_end[i]/1000;

// If tick < timer_last then the timer has wrapped around. So subtract max_tick from timer_end.
    if(tick < timer_last[i])
    {
            timer_end[i] -= max_tick;
            end_ms = timer_end[i]/1000;
            DEBUGF(INDI::Logger::DBG_DEBUG, "Timer callback: Timer wrapped around. This tick %d ms Last tick %d ms => New End tick %d ms", tick_ms, last_ms, end_ms);
    }

    int32_t left_ms = end_ms - tick_ms;
    if(left_ms <= 1) // Within 1ms
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "Timer callback: Timer ended with overshoot %d ms", -left_ms);
        TimerChange(user_gpio);  // Handle end of timer
        return;
    }
    if(left_ms < max_timer_ms) // Reset to the time remaining
    {
        set_watchdog(m_piId, user_gpio, left_ms);
        DEBUGF(INDI::Logger::DBG_DEBUG, "Timer callback: Reset timer to %d ms", left_ms);
    }

    DEBUGF(INDI::Logger::DBG_DEBUG, "Timer callback: This tick %d ms Last tick %d ms End tick %d ms Left %d ms", tick_ms, last_ms, end_ms, left_ms);
    timer_last[i] = tick;
}

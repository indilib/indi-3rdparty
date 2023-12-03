/*
    Avalon Unified Driver Aux

    Copyright (C) 2020,2023

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "config.h"

#include "indicom.h"

#include <cstring>
#include <termios.h>
#include <stdarg.h>
#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif
#include <memory>
#include <time.h>
#include <chrono>
#include <pthread.h>
#include <zmq.h>

#include "indi_avalonud_aux.h"


using json = nlohmann::json;

const int IPport = 5450;

static char device_str[MAXINDIDEVICE] = "AvalonUD AUX";

const char *STATUS_TAB = "Status";


static std::unique_ptr<AUDAUX> aux(new AUDAUX());


void ISGetProperties(const char *dev)
{
    aux->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    aux->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    aux->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    aux->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

void ISSnoopDevice(XMLEle *root)
{
    aux->ISSnoopDevice(root);
}

AUDAUX::AUDAUX()
{
    setVersion(AVALONUD_VERSION_MAJOR,AVALONUD_VERSION_MINOR);

    features = 0;

    context = zmq_ctx_new();
}

AUDAUX::~AUDAUX()
{
//    zmq_ctx_term(context);
}

bool AUDAUX::initProperties()
{
    INDI::DefaultDevice::initProperties();

    ConfigTP[0].fill("ADDRESS", "Address", "127.0.0.1");
    ConfigTP.fill(getDeviceName(), "DEVICE_ADDRESS", "Server", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    HWTypeTP[0].fill("HW_TYPE", "Controller Type", "");
    HWTypeTP.fill(getDeviceName(), "HW_TYPE_INFO", "Type", INFO_TAB, IP_RO, 60, IPS_IDLE);

    HWIdentifierTP[0].fill("HW_IDENTIFIER", "HW Identifier", "");
    HWIdentifierTP.fill(getDeviceName(), "HW_IDENTIFIER_INFO", "Identifier", INFO_TAB, IP_RO, 60, IPS_IDLE);

    LowLevelSWTP[LLSW_NAME].fill("LLSW_NAME", "Name", "");
    LowLevelSWTP[LLSW_VERSION].fill("LLSW_VERSION", "Version", "--");
    LowLevelSWTP.fill(getDeviceName(), "LLSW_INFO", "LowLevel SW", INFO_TAB, IP_RO, 60, IPS_IDLE);

    SystemManagementSP[SMNT_SHUTDOWN].fill("SHUTDOWN","Shutdown",ISS_OFF);
    SystemManagementSP[SMNT_REBOOT].fill("REBOOT","Reboot",ISS_OFF);
    SystemManagementSP.fill(getDeviceName(), "SYSTEM_MANAGEMENT", "System Mngm", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    OUTPort1SP[POWER_ON].fill("POWER_ON","On",ISS_OFF);
    OUTPort1SP[POWER_OFF].fill("POWER_OFF","Off",ISS_OFF);
    OUTPort1SP.fill(getDeviceName(), "OUT_PORT1", "OUT Port #1", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    OUTPort2SP[POWER_ON].fill("POWER_ON","On",ISS_OFF);
    OUTPort2SP[POWER_OFF].fill("POWER_OFF","Off",ISS_OFF);
    OUTPort2SP.fill(getDeviceName(), "OUT_PORT2", "OUT Port #2", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    OUTPortPWMDUTYCYCLENP[0].fill("DUTYCYCLE", "Output [%]", "%.f", 40.0, 100.0, 1.0, 50.0);
    OUTPortPWMDUTYCYCLENP.fill(getDeviceName(), "OUT_PORTPWM_DUTYCYCLE", "OUT Port PWM", MAIN_CONTROL_TAB, IP_RW, 60, IPS_OK);

    OUTPortPWMSP[POWER_ON].fill("POWER_ON","On",ISS_OFF);
    OUTPortPWMSP[POWER_OFF].fill("POWER_OFF","Off",ISS_OFF);
    OUTPortPWMSP.fill(getDeviceName(), "OUT_PORTPWM", "OUT Port PWM", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    USBPort1SP[POWER_ON].fill("POWER_ON","On",ISS_OFF);
    USBPort1SP[POWER_OFF].fill("POWER_OFF","Off",ISS_OFF);
    USBPort1SP.fill(getDeviceName(), "USB_PORT1", "USB3 Port #1", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    USBPort2SP[POWER_ON].fill("POWER_ON","On",ISS_OFF);
    USBPort2SP[POWER_OFF].fill("POWER_OFF","Off",ISS_OFF);
    USBPort2SP.fill(getDeviceName(), "USB_PORT2", "USB3 Port #2", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    USBPort3SP[POWER_ON].fill("POWER_ON","On",ISS_OFF);
    USBPort3SP[POWER_OFF].fill("POWER_OFF","Off",ISS_OFF);
    USBPort3SP.fill(getDeviceName(), "USB_PORT3", "USB2 Port #3", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    USBPort4SP[POWER_ON].fill("POWER_ON","On",ISS_OFF);
    USBPort4SP[POWER_OFF].fill("POWER_OFF","Off",ISS_OFF);
    USBPort4SP.fill(getDeviceName(), "USB_PORT4", "USB2 Port #4", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    PSUNP[PSU_VOLTAGE].fill("VOLTAGE","Voltage (V)","%.2f",0,0,0,0);
    PSUNP[PSU_CURRENT].fill("CURRENT","Current (A)","%.2f",0,0,0,0);
    PSUNP[PSU_POWER].fill("POWER","Power (W)","%.2f",0,0,0,0);
    PSUNP[PSU_CHARGE].fill("CHARGE","Charge (Ah)","%.3f",0,0,0,0);
    PSUNP.fill(getDeviceName(), "PSU","Power Supply", STATUS_TAB, IP_RO, 60, IPS_IDLE);

    SMNP[SM_FEEDTIME].fill("FEEDTIME","Feed Time (%)","%.1f",0,0,0,0);
    SMNP[SM_BUFFERLOAD].fill("BUFFERLOAD","Buffer Load (%)","%.1f",0,0,0,0);
    SMNP[SM_UPTIME].fill("UPTIME","Up Time (s)","%.0f",0,0,0,0);
    SMNP.fill(getDeviceName(), "STEPMACHINE","stepMachine", STATUS_TAB, IP_RO, 60, IPS_IDLE);

    CPUNP[0].fill("TEMPERATURE","Temperature (Cel)","%.1f",0,0,0,0);
    CPUNP.fill(getDeviceName(), "CPU","CPU", STATUS_TAB, IP_RO, 60, IPS_IDLE);

    addDebugControl();
    setDefaultPollingPeriod(5000);
    addPollPeriodControl();

    pthread_mutex_init( &connectionmutex, NULL );

    return true;
}

void AUDAUX::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    defineProperty(ConfigTP);
    loadConfig(true,ConfigTP.getName());
}

bool AUDAUX::updateProperties()
{
    if (isConnected())
    {
        readStatus();
    }

    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Settings
        defineProperty(SystemManagementSP);
        if ( features & 0x0010 ) {
            defineProperty(OUTPort1SP);
        }
        if ( features & 0x0020 ) {
            defineProperty(OUTPort2SP);
        }
        if ( features & 0x0040 ) {
            defineProperty(OUTPortPWMDUTYCYCLENP);
            defineProperty(OUTPortPWMSP);
        }
        defineProperty(USBPort1SP);
        defineProperty(USBPort2SP);
        defineProperty(USBPort3SP);
        defineProperty(USBPort4SP);
        defineProperty(HWTypeTP);
        defineProperty(HWIdentifierTP);
        defineProperty(LowLevelSWTP);
        if ( features & 0x0004 ) {
            defineProperty(PSUNP);
        }
        defineProperty(SMNP);
        defineProperty(CPUNP);

        LOG_INFO("AUX is ready");
    }
    else
    {
        deleteProperty(SystemManagementSP);
        if ( features & 0x0010 ) {
            deleteProperty(OUTPort1SP);
        }
        if ( features & 0x0020 ) {
            deleteProperty(OUTPort2SP);
        }
        if ( features & 0x0040 ) {
            deleteProperty(OUTPortPWMDUTYCYCLENP);
            deleteProperty(OUTPortPWMSP);
        }
        deleteProperty(USBPort1SP);
        deleteProperty(USBPort2SP);
        deleteProperty(USBPort3SP);
        deleteProperty(USBPort4SP);
        deleteProperty(HWTypeTP);
        deleteProperty(HWIdentifierTP);
        deleteProperty(LowLevelSWTP);
        if ( features & 0x0004 ) {
            deleteProperty(PSUNP);
        }
        deleteProperty(SMNP);
        deleteProperty(CPUNP);
    }

    return true;
}

bool AUDAUX::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp(dev,getDeviceName()))
    {
        // TCP Server settings
        if (ConfigTP.isNameMatch(name))
        {
            if ( isConnected() && strcmp(IPaddress,texts[0]) ) {
                DEBUG(INDI::Logger::DBG_WARNING, "Please Disconnect before changing IP address");
                return false;
            }
            ConfigTP.update(texts, names, n);
            ConfigTP.setState(IPS_OK);
            ConfigTP.apply();
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool AUDAUX::ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n)
{
    char *answer;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Shutdown/Reboot
        if (SystemManagementSP.isNameMatch(name))
        {
            SystemManagementSP.update(states, names, n);
            int index = SystemManagementSP.findOnSwitchIndex();

            SystemManagementSP.setState(IPS_BUSY);
            SystemManagementSP.reset();
            SystemManagementSP.apply();

            if ( isConnected() )
            {
                switch (index) {
                case SMNT_SHUTDOWN :
                    if ( time(NULL) - shutdown_time <= 10 ) {
                        sendCommand("SHUTDOWN");
                        SystemManagementSP.setState(IPS_ALERT);
                    } else {
                        DEBUG(INDI::Logger::DBG_WARNING, "Are you sure you want to shutdown?");
                        DEBUG(INDI::Logger::DBG_WARNING, "After shutdown only power cycling could restart the controller!");
                        DEBUG(INDI::Logger::DBG_WARNING, "To proceed press again within 10 seconds...");
                        SystemManagementSP.setState(IPS_BUSY);
                    }
                    reboot_time = 0;
                    shutdown_time = time(NULL);
                    break;
                case SMNT_REBOOT :
                    if ( time(NULL) - reboot_time <= 10 ) {
                        sendCommand("REBOOT");
                        SystemManagementSP.setState(IPS_ALERT);
                    } else {
                        DEBUG(INDI::Logger::DBG_WARNING, "Are you sure you want to reboot?");
                        DEBUG(INDI::Logger::DBG_WARNING, "To proceed press again within 10 seconds...");
                        SystemManagementSP.setState(IPS_BUSY);
                    }
                    reboot_time = time(NULL);
                    shutdown_time = 0;
                    break;
                }
            }
            SystemManagementSP.apply();
            return true;
        }

        // AUX Ports power
        if (OUTPort1SP.isNameMatch(name))
        {
            OUTPort1SP.update(states, names, n);
            int index = OUTPort1SP.findOnSwitchIndex();

            if ( index != -1 ) {
                if ( isConnected() ) {
                    OUTPort1SP.setState(IPS_BUSY);
                    OUTPort1SP.apply();
                    answer = sendCommand("SETPARAM POWER_PORT_OUT1 %d", (1-index) );
                    if ( !answer ) {
                        DEBUG(INDI::Logger::DBG_SESSION, "Port OUT1 switch completed");
                        OUTPort1SP.setState(IPS_OK);
                    } else {
                        DEBUGF(INDI::Logger::DBG_WARNING, "Port OUT1 switch failed due to %s",answer);
                        free(answer);
                        OUTPort1SP.setState(IPS_ALERT);
                    }
                    OUTPort1SP.apply();
                }
            }
            return true;
        }
        if (OUTPort2SP.isNameMatch(name))
        {
            OUTPort2SP.update(states, names, n);
            int index = OUTPort2SP.findOnSwitchIndex();

            if ( index != -1 ) {
                if ( isConnected() ) {
                    OUTPort2SP.setState(IPS_BUSY);
                    OUTPort2SP.apply();
                    answer = sendCommand("SETPARAM POWER_PORT_OUT2 %d", (1-index) );
                    if ( !answer ) {
                        DEBUG(INDI::Logger::DBG_SESSION, "Port OUT2 switch completed");
                        OUTPort2SP.setState(IPS_OK);
                    } else {
                        DEBUGF(INDI::Logger::DBG_WARNING, "Port OUT2 switch failed due to %s",answer);
                        free(answer);
                        OUTPort2SP.setState(IPS_ALERT);
                    }
                    OUTPort2SP.apply();
                }
            }
            return true;
        }
        if (OUTPortPWMSP.isNameMatch(name))
        {
            OUTPortPWMSP.update(states, names, n);
            int index = OUTPortPWMSP.findOnSwitchIndex();

            if ( index != -1 ) {
                if ( isConnected() ) {
                    OUTPortPWMSP.setState(IPS_BUSY);
                    OUTPortPWMSP.apply();
                    answer = sendCommand("SETPARAM POWER_PORT_OUTPWM %d", (1-index) );
                    if ( !answer ) {
                        DEBUG(INDI::Logger::DBG_SESSION, "Port OUTPWM switch completed");
                        OUTPortPWMSP.setState(IPS_OK);
                    } else {
                        DEBUGF(INDI::Logger::DBG_WARNING, "Port OUTPWM switch failed due to %s",answer);
                        free(answer);
                        OUTPortPWMSP.setState(IPS_ALERT);
                    }
                    OUTPortPWMSP.apply();
                }
            }
            return true;
        }

        // USB Ports power
        if (USBPort1SP.isNameMatch(name))
        {
            USBPort1SP.update(states, names, n);
            int index = USBPort1SP.findOnSwitchIndex();

            if ( index != -1 ) {
                if ( isConnected() ) {
                    USBPort1SP.setState(IPS_BUSY);
                    USBPort1SP.apply();
                    answer = sendCommand("SETPARAM POWER_PORT_USB1 %d", (1-index) );
                    if ( !answer ) {
                        DEBUG(INDI::Logger::DBG_SESSION, "Port USB #1 switch completed");
                        USBPort1SP.setState(IPS_OK);
                    } else {
                        DEBUGF(INDI::Logger::DBG_WARNING, "Port USB #1 switch failed due to %s",answer);
                        free(answer);
                        USBPort1SP.setState(IPS_ALERT);
                    }
                    USBPort1SP.apply();
                }
            }
            return true;
        }
        if (USBPort2SP.isNameMatch(name))
        {
            USBPort2SP.update(states, names, n);
            int index = USBPort2SP.findOnSwitchIndex();

            if ( index != -1 ) {
                if ( isConnected() ) {
                    USBPort2SP.setState(IPS_BUSY);
                    USBPort2SP.apply();
                    answer = sendCommand("SETPARAM POWER_PORT_USB2 %d", (1-index) );
                    if ( !answer ) {
                        DEBUG(INDI::Logger::DBG_SESSION, "Port USB #2 switch completed");
                        USBPort2SP.setState(IPS_OK);
                    } else {
                        DEBUGF(INDI::Logger::DBG_WARNING, "Port USB #2 switch failed due to %s",answer);
                        free(answer);
                        USBPort2SP.setState(IPS_ALERT);
                    }
                    USBPort2SP.apply();
                }
            }
            return true;
        }
        if (USBPort3SP.isNameMatch(name))
        {
            USBPort3SP.update(states, names, n);
            int index = USBPort3SP.findOnSwitchIndex();

            if ( index != -1 ) {
                if ( isConnected() ) {
                    USBPort3SP.setState(IPS_BUSY);
                    USBPort3SP.apply();
                    answer = sendCommand("SETPARAM POWER_PORT_USB3 %d", (1-index) );
                    if ( !answer ) {
                        DEBUG(INDI::Logger::DBG_SESSION, "Port USB #3 switch completed");
                        USBPort3SP.setState(IPS_OK);
                    } else {
                        DEBUGF(INDI::Logger::DBG_WARNING, "Port USB #3 switch failed due to %s",answer);
                        free(answer);
                        USBPort3SP.setState(IPS_ALERT);
                    }
                    USBPort3SP.apply();
                }
            }
            return true;
        }
        if (USBPort4SP.isNameMatch(name))
        {
            USBPort4SP.update(states, names, n);
            int index = USBPort4SP.findOnSwitchIndex();

            if ( index != -1 ) {
                if ( isConnected() ) {
                    USBPort4SP.setState(IPS_BUSY);
                    USBPort4SP.apply();
                    answer = sendCommand("SETPARAM POWER_PORT_USB4 %d", (1-index) );
                    if ( !answer ) {
                        DEBUG(INDI::Logger::DBG_SESSION, "Port USB #4 switch completed");
                        USBPort4SP.setState(IPS_OK);
                    } else {
                        DEBUGF(INDI::Logger::DBG_WARNING, "Port USB #4 switch failed due to %s",answer);
                        free(answer);
                        USBPort4SP.setState(IPS_ALERT);
                    }
                    USBPort4SP.apply();
                }
            }
            return true;
        }

        // this is executed at any command except reboot and shutdown
        reboot_time = 0;
        shutdown_time = 0;
    }

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool AUDAUX::ISNewNumber(const char * dev, const char * name, double values[], char * names[], int n)
{
    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (OUTPortPWMDUTYCYCLENP.isNameMatch(name))
        {
            OUTPortPWMDUTYCYCLENP.update(values, names, n);

            OUTPortPWMDUTYCYCLENP.setState(IPS_BUSY);
            if ( isConnected() )
                sendCommand("SETPARAM POWER_PORT_OUTPWM_DUTYCYCLE %.0f", OUTPortPWMDUTYCYCLENP[0].value/100.0*255.0 );
            OUTPortPWMDUTYCYCLENP.setState(IPS_OK);

            OUTPortPWMDUTYCYCLENP.apply();
            return true;
        }
    }

    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}


bool AUDAUX::Connect()
{
    char addr[1024], *answer;
    int timeout = 500;

    if (isConnected())
        return true;

    IPaddress = strdup(ConfigTP[0].text);

    DEBUGF(INDI::Logger::DBG_SESSION, "Attempting to connect %s aux...",IPaddress);

    requester = zmq_socket(context, ZMQ_REQ);
    zmq_setsockopt(requester, ZMQ_RCVTIMEO, &timeout, sizeof(timeout) );
    snprintf( addr, sizeof(addr), "tcp://%s:%d", IPaddress, IPport );
    zmq_connect(requester, addr);

    answer = sendRequest("DISCOVER");
    if ( answer ) {
        if ( !strcmp(answer,"stepMachine") ) {
            free(answer);
            answer = sendRequest("INFOALL");
            if ( answer ) {
                json j;
                std::string sHWt,sHWi,sFWv;

                j = json::parse(answer,nullptr,false);
                free(answer);
                if ( j.is_discarded() ||
                        !j.contains("HWType") ||
                        !j.contains("HWFeatures") ||
                        !j.contains("HWIdentifier") ||
                        !j.contains("firmwareVersion") )
                {
                    zmq_close(requester);
                    DEBUGF(INDI::Logger::DBG_ERROR, "Communication with %s AUX failed",IPaddress);
                    free(IPaddress);
                    return false;
                }

                j["HWType"].get_to(sHWt);
                HWTypeTP[0].setText(sHWt);
                HWTypeTP.apply();
                j["HWFeatures"].get_to(features);
                j["HWIdentifier"].get_to(sHWi);
                HWIdentifierTP[0].setText(sHWi);
                HWIdentifierTP.apply();
                LowLevelSWTP[LLSW_NAME].setText("stepMachine");
                j["firmwareVersion"].get_to(sFWv);
                LowLevelSWTP[LLSW_VERSION].setText(sFWv);
                LowLevelSWTP.apply();
            }
            if ( !(features & 0x0074) ) {
                zmq_close(requester);
                DEBUGF(INDI::Logger::DBG_ERROR, "AUX features not supported by %s hardware",IPaddress);
                free(IPaddress);
                return false;
            }
        } else {
            zmq_close(requester);
            DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect %s aux",IPaddress);
            free(answer);
            free(IPaddress);
            return false;
        }
    } else {
        zmq_close(requester);
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect %s aux",IPaddress);
        free(IPaddress);
        return false;
    }

    tid = SetTimer(getCurrentPollingPeriod());

    DEBUGF(INDI::Logger::DBG_SESSION, "Successfully connected %s aux",IPaddress);
    return true;
}

bool AUDAUX::Disconnect()
{
    if (!isConnected()) {
        DEBUG(INDI::Logger::DBG_WARNING, "Disconnect called before driver connection");
        return true;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Attempting to disconnect aux...");

    zmq_close(requester);

    RemoveTimer( tid );

    free(IPaddress);

    DEBUG(INDI::Logger::DBG_SESSION, "Successfully disconnected aux");

    return true;
}

void AUDAUX::TimerHit()
{
    if (isConnected() == false)
        return;

    // Read the current status
    readStatus();

    if ( SystemManagementSP.getState() == IPS_BUSY ) {
        if ( ( time(NULL) - reboot_time > 10 ) && ( time(NULL) - shutdown_time > 10 ) ) {
            SystemManagementSP.setState(IPS_OK);
            SystemManagementSP.apply();
            DEBUG(INDI::Logger::DBG_SESSION, "Reboot/Shutdown command cleared");
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool AUDAUX::readStatus()
{
    char *answer;

    answer = sendRequest("HOUSEKEEPINGS");
    if ( answer ) {
        json j;
        int value;

        j = json::parse(answer,nullptr,false);
        free(answer);

        if ( j.is_discarded() )
            return false;

        if ( features & 0x0004 ) {
            if ( j.contains("voltage_V") )
                j["voltage_V"].get_to(PSUNP[PSU_VOLTAGE].value);
            if ( j.contains("current_A") )
                j["current_A"].get_to(PSUNP[PSU_CURRENT].value);
            if ( j.contains("power_W") )
                j["power_W"].get_to(PSUNP[PSU_POWER].value);
            if ( j.contains("charge_Ah") )
                j["charge_Ah"].get_to(PSUNP[PSU_CHARGE].value);
            PSUNP.apply();
        }
        if ( j.contains("feedtime_perc") )
            j["feedtime_perc"].get_to(SMNP[SM_FEEDTIME].value);
        if ( j.contains("bufferload_perc") )
            j["bufferload_perc"].get_to(SMNP[SM_BUFFERLOAD].value);
        if ( j.contains("uptime_sec") )
            j["uptime_sec"].get_to(SMNP[SM_UPTIME].value);
        SMNP.apply();
        if ( j.contains("cputemp_celsius") )
            j["cputemp_celsius"].get_to(CPUNP[0].value);
        CPUNP.apply();

        if ( features & 0x0010 ) {
            if ( j.contains("POWER_PORT_OUT1") ) {
                j["POWER_PORT_OUT1"].get_to(value);
                OUTPort1SP[POWER_ON].setState((value?ISS_ON:ISS_OFF));
                OUTPort1SP[POWER_OFF].setState((value?ISS_OFF:ISS_ON));
                OUTPort1SP.apply();
            }
        }
        if ( features & 0x0020 ) {
            if ( j.contains("POWER_PORT_OUT2") ) {
                j["POWER_PORT_OUT2"].get_to(value);
                OUTPort2SP[POWER_ON].setState((value?ISS_ON:ISS_OFF));
                OUTPort2SP[POWER_OFF].setState((value?ISS_OFF:ISS_ON));
                OUTPort2SP.apply();
            }
        }
        if ( features & 0x0040 ) {
            if ( j.contains("POWER_PORT_OUTPWM_DUTYCYCLE") ) {
                j["POWER_PORT_OUTPWM_DUTYCYCLE"].get_to(OUTPortPWMDUTYCYCLENP[0].value);
                OUTPortPWMDUTYCYCLENP[0].value *= 100.0 / 255.0;
                OUTPortPWMDUTYCYCLENP.apply();
            }
            if ( j.contains("POWER_PORT_OUTPWM") ) {
                j["POWER_PORT_OUTPWM"].get_to(value);
                OUTPortPWMSP[POWER_ON].setState((value?ISS_ON:ISS_OFF));
                OUTPortPWMSP[POWER_OFF].setState((value?ISS_OFF:ISS_ON));
                OUTPortPWMSP.apply();
            }
        }

        if ( j.contains("POWER_PORT_USB1") ) {
            j["POWER_PORT_USB1"].get_to(value);
            USBPort1SP[POWER_ON].setState((value?ISS_ON:ISS_OFF));
            USBPort1SP[POWER_OFF].setState((value?ISS_OFF:ISS_ON));
            USBPort1SP.apply();
        }
        if ( j.contains("POWER_PORT_USB2") ) {
            j["POWER_PORT_USB2"].get_to(value);
            USBPort2SP[POWER_ON].setState((value?ISS_ON:ISS_OFF));
            USBPort2SP[POWER_OFF].setState((value?ISS_OFF:ISS_ON));
            USBPort2SP.apply();
        }
        if ( j.contains("POWER_PORT_USB3") ) {
            j["POWER_PORT_USB3"].get_to(value);
            USBPort3SP[POWER_ON].setState((value?ISS_ON:ISS_OFF));
            USBPort3SP[POWER_OFF].setState((value?ISS_OFF:ISS_ON));
            USBPort3SP.apply();
        }
        if ( j.contains("POWER_PORT_USB4") ) {
            j["POWER_PORT_USB4"].get_to(value);
            USBPort4SP[POWER_ON].setState((value?ISS_ON:ISS_OFF));
            USBPort4SP[POWER_OFF].setState((value?ISS_OFF:ISS_ON));
            USBPort4SP.apply();
        }
        return true;
    }

    return false;
}

bool AUDAUX::saveConfigItems(FILE *fp)
{
    // We need to reserve and save address mode
    // so that the next time the driver is loaded, it is remembered and applied.
    ConfigTP.save(fp);

    return INDI::DefaultDevice::saveConfigItems(fp);
}

const char *AUDAUX::getDefaultName()
{
    return device_str;
}

char* AUDAUX::sendCommand(const char *fmt, ... )
{
    va_list ap;
    char buffer[4096], answer[4096], addr[1024];
    int rc,retries;
    zmq_pollitem_t item;

    va_start( ap, fmt );
    vsnprintf( buffer, sizeof(buffer), fmt, ap );
    va_end( ap );

    pthread_mutex_lock( &connectionmutex );
    retries = 3;
    do {
        zmq_send(requester, buffer, strlen(buffer), 0);
        item = { requester, 0, ZMQ_POLLIN, 0 };
        rc = zmq_poll( &item, 1, 500 ); // ms
        if ( ( rc >= 0 ) && ( item.revents & ZMQ_POLLIN ) ) {
            // communication succeeded
            rc = zmq_recv(requester, answer, sizeof(answer), 0);
            if ( rc >= 0 ) {
                pthread_mutex_unlock( &connectionmutex );
                answer[MIN(rc,(int)sizeof(answer)-1)] = '\0';
                if ( !strncmp(answer,"OK",2) )
                    return NULL;
                if ( !strncmp(answer,"ERROR:",6) )
                    return strdup(answer+6);
                return strdup("SYNTAXERROR");
            }
        }
        zmq_close(requester);
        requester = zmq_socket(context, ZMQ_REQ);
        snprintf( addr, sizeof(addr), "tcp://%s:%d", IPaddress, IPport );
        zmq_connect(requester, addr);
    } while ( --retries );
    pthread_mutex_unlock( &connectionmutex );
    DEBUG(INDI::Logger::DBG_WARNING, "No answer from driver");
    return strdup("COMMUNICATIONERROR");
}

char* AUDAUX::sendRequest(const char *fmt, ... )
{
    va_list ap;
    char buffer[4096], answer[4096], addr[1024];
    int rc,retries;
    zmq_pollitem_t item;

    va_start( ap, fmt );
    vsnprintf( buffer, sizeof(buffer), fmt, ap );
    va_end( ap );

    pthread_mutex_lock( &connectionmutex );
    retries = 3;
    do {
        zmq_send(requester, buffer, strlen(buffer), 0);
        item = { requester, 0, ZMQ_POLLIN, 0 };
        rc = zmq_poll( &item, 1, 500 ); // ms
        if ( ( rc >= 0 ) && ( item.revents & ZMQ_POLLIN ) ) {
            // communication succeeded
            rc = zmq_recv(requester, answer, sizeof(answer), 0);
            if ( rc >= 0 ) {
                pthread_mutex_unlock( &connectionmutex );
                answer[MIN(rc,(int)sizeof(answer)-1)] = '\0';
                return strdup(answer);
            }
        }
        zmq_close(requester);
        requester = zmq_socket(context, ZMQ_REQ);
        snprintf( addr, sizeof(addr), "tcp://%s:%d", IPaddress, IPport );
        zmq_connect(requester, addr);
    } while ( --retries );
    pthread_mutex_unlock( &connectionmutex );
    DEBUG(INDI::Logger::DBG_WARNING, "No answer from driver");
    return strdup("COMMUNICATIONERROR");
}

/*
  Avalon Unified Driver AUX

  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

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
#include <json.h>
#include <memory>
#include <time.h>
#include <chrono>
#include <pthread.h>
#include <zmq.h>

#include "indi_avalonud_aux.h"


using json = nlohmann::json;

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

    IUFillText(&ConfigT[0], "ADDRESS", "Address", "127.0.0.1");
    IUFillTextVector(&ConfigTP, ConfigT, 1, getDeviceName(), "DEVICE_ADDRESS", "Server", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    IUFillText(&HWTypeT[0], "HW_TYPE", "Controller Type", "");
    IUFillTextVector(&HWTypeTP, HWTypeT, 1, getDeviceName(), "HW_TYPE_INFO", "Type", INFO_TAB, IP_RO, 60, IPS_IDLE);

    IUFillText(&HWIdentifierT[0], "HW_IDENTIFIER", "HW Identifier", "");
    IUFillTextVector(&HWIdentifierTP, HWIdentifierT, 1, getDeviceName(), "HW_IDENTIFIER_INFO", "Identifier", INFO_TAB, IP_RO, 60, IPS_IDLE);

    IUFillText(&LowLevelSWT[0], "LLSW_NAME", "Name", "");
    IUFillText(&LowLevelSWT[1], "LLSW_VERSION", "Version", "--");
    IUFillTextVector(&LowLevelSWTP, LowLevelSWT, 2, getDeviceName(), "LLSW_INFO", "LowLevel SW", INFO_TAB, IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&SystemManagementS[0],"SHUTDOWN","Shutdown",ISS_OFF);
    IUFillSwitch(&SystemManagementS[1],"REBOOT","Reboot",ISS_OFF);
    IUFillSwitchVector(&SystemManagementSP, SystemManagementS, 2, getDeviceName(), "SYSTEM_MANAGEMENT", "System Mngm", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    IUFillSwitch(&OUTPort1S[0],"POWER_ON","On",ISS_OFF);
    IUFillSwitch(&OUTPort1S[1],"POWER_OFF","Off",ISS_OFF);
    IUFillSwitchVector(&OUTPort1SP, OUTPort1S, 2, getDeviceName(), "OUT_PORT1", "OUT Port #1", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    IUFillSwitch(&OUTPort2S[0],"POWER_ON","On",ISS_OFF);
    IUFillSwitch(&OUTPort2S[1],"POWER_OFF","Off",ISS_OFF);
    IUFillSwitchVector(&OUTPort2SP, OUTPort2S, 2, getDeviceName(), "OUT_PORT2", "OUT Port #2", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    IUFillNumber(&OUTPortPWMDUTYCYCLEN[0], "DUTYCYCLE", "Output [%]", "%.f", 40.0, 100.0, 1.0, 50.0);
    IUFillNumberVector(&OUTPortPWMDUTYCYCLENP, OUTPortPWMDUTYCYCLEN, 1, getDeviceName(), "OUT_PORTPWM_DUTYCYCLE", "OUT Port PWM", MAIN_CONTROL_TAB, IP_RW, 60, IPS_OK);

    IUFillSwitch(&OUTPortPWMS[0],"POWER_ON","On",ISS_OFF);
    IUFillSwitch(&OUTPortPWMS[1],"POWER_OFF","Off",ISS_OFF);
    IUFillSwitchVector(&OUTPortPWMSP, OUTPortPWMS, 2, getDeviceName(), "OUT_PORTPWM", "OUT Port PWM", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    IUFillSwitch(&USBPort1S[0],"POWER_ON","On",ISS_OFF);
    IUFillSwitch(&USBPort1S[1],"POWER_OFF","Off",ISS_OFF);
    IUFillSwitchVector(&USBPort1SP, USBPort1S, 2, getDeviceName(), "USB_PORT1", "USB3 Port #1", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    IUFillSwitch(&USBPort2S[0],"POWER_ON","On",ISS_OFF);
    IUFillSwitch(&USBPort2S[1],"POWER_OFF","Off",ISS_OFF);
    IUFillSwitchVector(&USBPort2SP, USBPort2S, 2, getDeviceName(), "USB_PORT2", "USB3 Port #2", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    IUFillSwitch(&USBPort3S[0],"POWER_ON","On",ISS_OFF);
    IUFillSwitch(&USBPort3S[1],"POWER_OFF","Off",ISS_OFF);
    IUFillSwitchVector(&USBPort3SP, USBPort3S, 2, getDeviceName(), "USB_PORT3", "USB2 Port #3", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    IUFillSwitch(&USBPort4S[0],"POWER_ON","On",ISS_OFF);
    IUFillSwitch(&USBPort4S[1],"POWER_OFF","Off",ISS_OFF);
    IUFillSwitchVector(&USBPort4SP, USBPort4S, 2, getDeviceName(), "USB_PORT4", "USB2 Port #4", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_OK);

    IUFillNumber(&PSUN[0],"VOLTAGE","Voltage (V)","%.2f",0,0,0,0);
    IUFillNumber(&PSUN[1],"CURRENT","Current (A)","%.2f",0,0,0,0);
    IUFillNumber(&PSUN[2],"POWER","Power (W)","%.2f",0,0,0,0);
    IUFillNumber(&PSUN[3],"CHARGE","Charge (Ah)","%.3f",0,0,0,0);
    IUFillNumberVector(&PSUNP, PSUN, 4, getDeviceName(), "PSU","Power Supply", STATUS_TAB, IP_RO, 60, IPS_IDLE);

    IUFillNumber(&SMN[0],"FEEDTIME","Feed Time (%)","%.1f",0,0,0,0);
    IUFillNumber(&SMN[1],"BUFFERLOAD","Buffer Load (%)","%.1f",0,0,0,0);
    IUFillNumber(&SMN[2],"UPTIME","Up Time (s)","%.0f",0,0,0,0);
    IUFillNumberVector(&SMNP, SMN, 3, getDeviceName(), "STEPMACHINE","stepMachine", STATUS_TAB, IP_RO, 60, IPS_IDLE);

    IUFillNumber(&CPUN[0],"TEMPERATURE","Temperature (Cel)","%.1f",0,0,0,0);
    IUFillNumberVector(&CPUNP, CPUN, 1, getDeviceName(), "CPU","CPU", STATUS_TAB, IP_RO, 60, IPS_IDLE);

    addDebugControl();
    setDefaultPollingPeriod(5000);
    addPollPeriodControl();

    pthread_mutex_init( &connectionmutex, NULL );

    return true;
}

void AUDAUX::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    defineProperty(&ConfigTP);
    loadConfig(true,ConfigTP.name);
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
        defineProperty(&SystemManagementSP);
        if ( features & 0x0010 ) {
            defineProperty(&OUTPort1SP);
        }
        if ( features & 0x0020 ) {
            defineProperty(&OUTPort2SP);
        }
        if ( features & 0x0040 ) {
            defineProperty(&OUTPortPWMDUTYCYCLENP);
            defineProperty(&OUTPortPWMSP);
        }
        defineProperty(&USBPort1SP);
        defineProperty(&USBPort2SP);
        defineProperty(&USBPort3SP);
        defineProperty(&USBPort4SP);
        defineProperty(&HWTypeTP);
        defineProperty(&HWIdentifierTP);
        defineProperty(&LowLevelSWTP);
        if ( features & 0x0004 ) {
            defineProperty(&PSUNP);
        }
        defineProperty(&SMNP);
        defineProperty(&CPUNP);

        LOG_INFO("AUX is ready");
    }
    else
    {
        deleteProperty(SystemManagementSP.name);
        if ( features & 0x0010 ) {
            deleteProperty(OUTPort1SP.name);
        }
        if ( features & 0x0020 ) {
            deleteProperty(OUTPort2SP.name);
        }
        if ( features & 0x0040 ) {
            deleteProperty(OUTPortPWMDUTYCYCLENP.name);
            deleteProperty(OUTPortPWMSP.name);
        }
        deleteProperty(USBPort1SP.name);
        deleteProperty(USBPort2SP.name);
        deleteProperty(USBPort3SP.name);
        deleteProperty(USBPort4SP.name);
        deleteProperty(HWTypeTP.name);
        deleteProperty(HWIdentifierTP.name);
        deleteProperty(LowLevelSWTP.name);
        if ( features & 0x0004 ) {
            deleteProperty(PSUNP.name);
        }
        deleteProperty(SMNP.name);
        deleteProperty(CPUNP.name);
    }

    return true;
}

bool AUDAUX::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp(dev,getDeviceName()))
    {
        // TCP Server settings
        if (!strcmp(name, ConfigTP.name))
        {
            IUUpdateText(&ConfigTP, texts, names, n);
            ConfigTP.s = IPS_OK;
            IDSetText(&ConfigTP, nullptr);
            if (isConnected() && strcmp(IPaddress,ConfigT[0].text) )
                DEBUG(INDI::Logger::DBG_WARNING, "Disconnect and reconnect to make IP address change effective!");
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
        if (!strcmp(name, SystemManagementSP.name))
        {
            IUUpdateSwitch(&SystemManagementSP, states, names, n);
            int index = IUFindOnSwitchIndex(&SystemManagementSP);

            SystemManagementSP.s = IPS_BUSY;
            IUResetSwitch(&SystemManagementSP);

            if ( isConnected() )
            {
                switch (index) {
                case 0 :
                    if ( time(NULL) - shutdown_time <= 10 ) {
                        sendCommand("SHUTDOWN");
                        SystemManagementSP.s = IPS_ALERT;
                    } else {
                        DEBUG(INDI::Logger::DBG_WARNING, "Are you sure you want to shutdown?");
                        DEBUG(INDI::Logger::DBG_WARNING, "After shutdown only power cycling could restart the controller!");
                        DEBUG(INDI::Logger::DBG_WARNING, "To proceed press again within 10 seconds...");
                        SystemManagementSP.s = IPS_BUSY;
                    }
                    reboot_time = 0;
                    shutdown_time = time(NULL);
                    break;
                case 1 :
                    if ( time(NULL) - reboot_time <= 10 ) {
                        sendCommand("REBOOT");
                        SystemManagementSP.s = IPS_ALERT;
                    } else {
                        DEBUG(INDI::Logger::DBG_WARNING, "Are you sure you want to reboot?");
                        DEBUG(INDI::Logger::DBG_WARNING, "To proceed press again within 10 seconds...");
                        SystemManagementSP.s = IPS_BUSY;
                    }
                    reboot_time = time(NULL);
                    shutdown_time = 0;
                    break;
                }
            }
            IDSetSwitch(&SystemManagementSP,NULL);
            return true;
        }

        // AUX Ports power
        if (!strcmp(name, OUTPort1SP.name))
        {
            IUUpdateSwitch(&OUTPort1SP, states, names, n);
            int index = IUFindOnSwitchIndex(&OUTPort1SP);

            if ( index != -1 ) {
                OUTPort1SP.s = IPS_BUSY;
                if ( isConnected() ) {
                    answer = sendCommand("SETPARAM POWER_PORT_OUT1 %d", (1-index) );
                    if ( !answer ) {
                        DEBUG(INDI::Logger::DBG_SESSION, "Port OUT1 switch completed");
                        OUTPort1SP.s = IPS_OK;
                    } else {
                        DEBUGF(INDI::Logger::DBG_WARNING, "Port OUT1 switch failed due to %s",answer);
                        free(answer);
                        OUTPort1SP.s = IPS_ALERT;
                    }
                }
                IDSetSwitch(&OUTPort1SP,NULL);
            }
            return true;
        }
        if (!strcmp(name, OUTPort2SP.name))
        {
            IUUpdateSwitch(&OUTPort2SP, states, names, n);
            int index = IUFindOnSwitchIndex(&OUTPort2SP);

            if ( index != -1 ) {
                OUTPort2SP.s = IPS_BUSY;
                if ( isConnected() ) {
                    answer = sendCommand("SETPARAM POWER_PORT_OUT2 %d", (1-index) );
                    if ( !answer ) {
                        DEBUG(INDI::Logger::DBG_SESSION, "Port OUT2 switch completed");
                        OUTPort2SP.s = IPS_OK;
                    } else {
                        DEBUGF(INDI::Logger::DBG_WARNING, "Port OUT2 switch failed due to %s",answer);
                        free(answer);
                        OUTPort2SP.s = IPS_ALERT;
                    }
                }
                IDSetSwitch(&OUTPort2SP,NULL);
            }
            return true;
        }
        if (!strcmp(name, OUTPortPWMSP.name))
        {
            IUUpdateSwitch(&OUTPortPWMSP, states, names, n);
            int index = IUFindOnSwitchIndex(&OUTPortPWMSP);

            if ( index != -1 ) {
                OUTPortPWMSP.s = IPS_BUSY;
                if ( isConnected() ) {
                    answer = sendCommand("SETPARAM POWER_PORT_OUTPWM %d", (1-index) );
                    if ( !answer ) {
                        DEBUG(INDI::Logger::DBG_SESSION, "Port OUTPWM switch completed");
                        OUTPortPWMSP.s = IPS_OK;
                    } else {
                        DEBUGF(INDI::Logger::DBG_WARNING, "Port OUTPWM switch failed due to %s",answer);
                        free(answer);
                        OUTPortPWMSP.s = IPS_ALERT;
                    }
                }
                IDSetSwitch(&OUTPortPWMSP,NULL);
            }
            return true;
        }

        // USB Ports power
        if (!strcmp(name, USBPort1SP.name))
        {
            IUUpdateSwitch(&USBPort1SP, states, names, n);
            int index = IUFindOnSwitchIndex(&USBPort1SP);

            if ( index != -1 ) {
                USBPort1SP.s = IPS_BUSY;
                if ( isConnected() ) {
                    answer = sendCommand("SETPARAM POWER_PORT_USB1 %d", (1-index) );
                    if ( !answer ) {
                        DEBUG(INDI::Logger::DBG_SESSION, "Port USB #1 switch completed");
                        USBPort1SP.s = IPS_OK;
                    } else {
                        DEBUGF(INDI::Logger::DBG_WARNING, "Port USB #1 switch failed due to %s",answer);
                        free(answer);
                        USBPort1SP.s = IPS_ALERT;
                    }
                }
                IDSetSwitch(&USBPort1SP,NULL);
            }
            return true;
        }
        if (!strcmp(name, USBPort2SP.name))
        {
            IUUpdateSwitch(&USBPort2SP, states, names, n);
            int index = IUFindOnSwitchIndex(&USBPort2SP);

            if ( index != -1 ) {
                USBPort2SP.s = IPS_BUSY;
                if ( isConnected() ) {
                    answer = sendCommand("SETPARAM POWER_PORT_USB2 %d", (1-index) );
                    if ( !answer ) {
                        DEBUG(INDI::Logger::DBG_SESSION, "Port USB #2 switch completed");
                        USBPort2SP.s = IPS_OK;
                    } else {
                        DEBUGF(INDI::Logger::DBG_WARNING, "Port USB #2 switch failed due to %s",answer);
                        free(answer);
                        USBPort2SP.s = IPS_ALERT;
                    }
                }
                IDSetSwitch(&USBPort2SP,NULL);
            }
            return true;
        }
        if (!strcmp(name, USBPort3SP.name))
        {
            IUUpdateSwitch(&USBPort3SP, states, names, n);
            int index = IUFindOnSwitchIndex(&USBPort3SP);

            if ( index != -1 ) {
                USBPort3SP.s = IPS_BUSY;
                if ( isConnected() ) {
                    answer = sendCommand("SETPARAM POWER_PORT_USB3 %d", (1-index) );
                    if ( !answer ) {
                        DEBUG(INDI::Logger::DBG_SESSION, "Port USB #3 switch completed");
                        USBPort3SP.s = IPS_OK;
                    } else {
                        DEBUGF(INDI::Logger::DBG_WARNING, "Port USB #3 switch failed due to %s",answer);
                        free(answer);
                        USBPort3SP.s = IPS_ALERT;
                    }
                }
                IDSetSwitch(&USBPort3SP,NULL);
            }
            return true;
        }
        if (!strcmp(name, USBPort4SP.name))
        {
            IUUpdateSwitch(&USBPort4SP, states, names, n);
            int index = IUFindOnSwitchIndex(&USBPort4SP);

            if ( index != -1 ) {
                USBPort4SP.s = IPS_BUSY;
                if ( isConnected() ) {
                    answer = sendCommand("SETPARAM POWER_PORT_USB4 %d", (1-index) );
                    if ( !answer ) {
                        DEBUG(INDI::Logger::DBG_SESSION, "Port USB #4 switch completed");
                        USBPort4SP.s = IPS_OK;
                    } else {
                        DEBUGF(INDI::Logger::DBG_WARNING, "Port USB #4 switch failed due to %s",answer);
                        free(answer);
                        USBPort4SP.s = IPS_ALERT;
                    }
                }
                IDSetSwitch(&USBPort4SP,NULL);
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
        if (!strcmp(name, OUTPortPWMDUTYCYCLENP.name))
        {
            IUUpdateNumber(&OUTPortPWMDUTYCYCLENP, values, names, n);

            OUTPortPWMDUTYCYCLENP.s = IPS_BUSY;
            if ( isConnected() )
                sendCommand("SETPARAM POWER_PORT_OUTPWM_DUTYCYCLE %.0f", OUTPortPWMDUTYCYCLEN[0].value/100.0*255.0 );
            OUTPortPWMDUTYCYCLENP.s = IPS_OK;

            IDSetNumber(&OUTPortPWMDUTYCYCLENP,NULL);
            return true;
        }
    }

    return DefaultDevice::ISNewNumber(dev, name, values, names, n);
}


bool AUDAUX::Handshake()
{
    return aux->Connect();
}

bool AUDAUX::Connect()
{
    char addr[1024], *answer;
    int timeout = 500;

    if (isConnected())
        return true;

    IPaddress = strdup(ConfigT[0].text);

    DEBUGF(INDI::Logger::DBG_SESSION, "Attempting to connect %s aux...",IPaddress);

    requester = zmq_socket(context, ZMQ_REQ);
    zmq_setsockopt(requester, ZMQ_RCVTIMEO, &timeout, sizeof(timeout) );
    snprintf( addr, sizeof(addr), "tcp://%s:5450", IPaddress);
    zmq_connect(requester, addr);

    answer = sendRequest("DISCOVER");
    if ( answer ) {
        if ( !strcmp(answer,"stepMachine") ) {
            free(answer);
            answer = sendRequest("INFOALL");
            if ( answer ) {
                json j;
                std::string sHWt,sHWi,sFW;

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
                IUSaveText(&HWTypeT[0], sHWt.c_str());
                IDSetText(&HWTypeTP, nullptr);
                j["HWFeatures"].get_to(features);
                j["HWIdentifier"].get_to(sHWi);
                IUSaveText(&HWIdentifierT[0], sHWi.c_str());
                IDSetText(&HWIdentifierTP, nullptr);
                IUSaveText(&LowLevelSWT[0], "stepMachine");
                j["firmwareVersion"].get_to(sFW);
                IUSaveText(&LowLevelSWT[1], sFW.c_str());
                IDSetText(&LowLevelSWTP, nullptr);
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

    if ( SystemManagementSP.s == IPS_BUSY ) {
        if ( ( time(NULL) - reboot_time > 10 ) && ( time(NULL) - shutdown_time > 10 ) ) {
            SystemManagementSP.s = IPS_OK;
            IDSetSwitch(&SystemManagementSP,NULL);
            DEBUG(INDI::Logger::DBG_SESSION, "Reboot/Shutdown command cleared");
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

bool AUDAUX::readStatus()
{
    char *answer;
    int value;

    answer = sendRequest("HOUSEKEEPINGS");
    if ( answer ) {
        json j;

        j = json::parse(answer,nullptr,false);
        free(answer);

        if ( j.is_discarded() )
            return false;

        if ( features & 0x0004 ) {
            if ( j.contains("voltage_V") )
                j["voltage_V"].get_to(PSUN[0].value);
            if ( j.contains("current_A") )
                j["current_A"].get_to(PSUN[1].value);
            if ( j.contains("power_W") )
                j["power_W"].get_to(PSUN[2].value);
            if ( j.contains("charge_Ah") )
                j["charge_Ah"].get_to(PSUN[3].value);
            IDSetNumber(&PSUNP, nullptr);
        }
        if ( j.contains("feedtime_perc") )
            j["feedtime_perc"].get_to(SMN[0].value);
        if ( j.contains("bufferload_perc") )
            j["bufferload_perc"].get_to(SMN[1].value);
        if ( j.contains("uptime_sec") )
            j["uptime_sec"].get_to(SMN[2].value);
        IDSetNumber(&SMNP, nullptr);
        if ( j.contains("cputemp_celsius") )
            j["cputemp_celsius"].get_to(CPUN[0].value);
        IDSetNumber(&CPUNP, nullptr);

        if ( features & 0x0010 ) {
            if ( j.contains("POWER_PORT_OUT1") ) {
                j["POWER_PORT_OUT1"].get_to(value);
                OUTPort1S[0].s = (value?ISS_ON:ISS_OFF);
                OUTPort1S[1].s = (value?ISS_OFF:ISS_ON);
                IDSetSwitch(&OUTPort1SP, nullptr);
            }
        }
        if ( features & 0x0020 ) {
            if ( j.contains("POWER_PORT_OUT2") ) {
                j["POWER_PORT_OUT2"].get_to(value);
                OUTPort2S[0].s = (value?ISS_ON:ISS_OFF);
                OUTPort2S[1].s = (value?ISS_OFF:ISS_ON);
                IDSetSwitch(&OUTPort2SP, nullptr);
            }
        }
        if ( features & 0x0040 ) {
            if ( j.contains("POWER_PORT_OUTPWM_DUTYCYCLE") ) {
                j["POWER_PORT_OUTPWM_DUTYCYCLE"].get_to(OUTPortPWMDUTYCYCLEN[0].value);
                OUTPortPWMDUTYCYCLEN[0].value *= 100.0 / 255.0;
                IDSetNumber(&OUTPortPWMDUTYCYCLENP, nullptr);
            }
            if ( j.contains("POWER_PORT_OUTPWM") ) {
                j["POWER_PORT_OUTPWM"].get_to(value);
                OUTPortPWMS[0].s = (value?ISS_ON:ISS_OFF);
                OUTPortPWMS[1].s = (value?ISS_OFF:ISS_ON);
                IDSetSwitch(&OUTPortPWMSP, nullptr);
            }
        }

        if ( j.contains("POWER_PORT_USB1") ) {
            j["POWER_PORT_USB1"].get_to(value);
            USBPort1S[0].s = (value?ISS_ON:ISS_OFF);
            USBPort1S[1].s = (value?ISS_OFF:ISS_ON);
            IDSetSwitch(&USBPort1SP, nullptr);
        }
        if ( j.contains("POWER_PORT_USB2") ) {
            j["POWER_PORT_USB2"].get_to(value);
            USBPort2S[0].s = (value?ISS_ON:ISS_OFF);
            USBPort2S[1].s = (value?ISS_OFF:ISS_ON);
            IDSetSwitch(&USBPort2SP, nullptr);
        }
        if ( j.contains("POWER_PORT_USB3") ) {
            j["POWER_PORT_USB3"].get_to(value);
            USBPort3S[0].s = (value?ISS_ON:ISS_OFF);
            USBPort3S[1].s = (value?ISS_OFF:ISS_ON);
            IDSetSwitch(&USBPort3SP, nullptr);
        }
        if ( j.contains("POWER_PORT_USB4") ) {
            j["POWER_PORT_USB4"].get_to(value);
            USBPort4S[0].s = (value?ISS_ON:ISS_OFF);
            USBPort4S[1].s = (value?ISS_OFF:ISS_ON);
            IDSetSwitch(&USBPort4SP, nullptr);
        }
        return true;
    }

    return false;
}

bool AUDAUX::saveConfigItems(FILE *fp)
{
    // We need to reserve and save address mode
    // so that the next time the driver is loaded, it is remembered and applied.
    IUSaveConfigText(fp, &ConfigTP);

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
        snprintf( addr, sizeof(addr), "tcp://%s:5451", IPaddress );
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
        snprintf( addr, sizeof(addr), "tcp://%s:5451", IPaddress );
        zmq_connect(requester, addr);
    } while ( --retries );
    pthread_mutex_unlock( &connectionmutex );
    DEBUG(INDI::Logger::DBG_WARNING, "No answer from driver");
    return strdup("COMMUNICATIONERROR");
}

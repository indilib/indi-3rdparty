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

#pragma once

#include <time.h>
#include <pthread.h>
#include "defaultdevice.h"

#define MIN(a,b) (((a)<=(b))?(a):(b))

class AUDAUX : public INDI::DefaultDevice
{
public:
    AUDAUX();
    ~AUDAUX();

    virtual bool Connect() override;
    virtual bool Disconnect() override;

    const char *getDefaultName() override;

    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual void ISGetProperties(const char *dev) override;

    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

protected:
    virtual void TimerHit() override;

    virtual bool saveConfigItems(FILE *fp) override;

private:

    int tid;
    unsigned int features;

    INDI::PropertyText ConfigTP {1};
    enum {
        LLSW_NAME,
        LLSW_VERSION,
        LLSW_N
    };
    INDI::PropertyText LowLevelSWTP {LLSW_N};
    INDI::PropertyText HWTypeTP {1};
    INDI::PropertyText HWIdentifierTP {1};
    enum {
        SMNT_SHUTDOWN,
        SMNT_REBOOT,
        SMNT_N
    };
    INDI::PropertySwitch SystemManagementSP {SMNT_N};
    INDI::PropertyNumber OUTPortPWMDUTYCYCLENP {1};
    enum {
        POWER_ON,
        POWER_OFF,
        POWER_N
    };
    INDI::PropertySwitch OUTPortPWMSP {POWER_N};
    INDI::PropertySwitch OUTPort1SP {POWER_N};
    INDI::PropertySwitch OUTPort2SP {POWER_N};
    INDI::PropertySwitch USBPort1SP {POWER_N};
    INDI::PropertySwitch USBPort2SP {POWER_N};
    INDI::PropertySwitch USBPort3SP {POWER_N};
    INDI::PropertySwitch USBPort4SP {POWER_N};
    enum {
        PSU_VOLTAGE,
        PSU_CURRENT,
        PSU_POWER,
        PSU_CHARGE,
        PSU_N
    };
    INDI::PropertyNumber PSUNP {PSU_N};
    enum {
        SM_FEEDTIME,
        SM_BUFFERLOAD,
        SM_UPTIME,
        SM_N
    };
    INDI::PropertyNumber SMNP {SM_N};
    INDI::PropertyNumber CPUNP {1};

    bool readStatus();

    char* IPaddress;
    char* sendCommand(const char*,...);
    char* sendRequest(const char*,...);

    void *context,*requester;
    time_t reboot_time,shutdown_time;

    pthread_mutex_t connectionmutex;
};

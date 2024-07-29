/*
    Avalon Unified Driver Focuser

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

#include <pthread.h>
#include "indifocuser.h"

#define MIN(a,b) (((a)<=(b))?(a):(b))

class AUDFOCUSER : public INDI::Focuser
{
public:
    AUDFOCUSER();
    ~AUDFOCUSER();

    virtual bool Connect() override;
    virtual bool Disconnect() override;

    const char *getDefaultName() override;

    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual void ISGetProperties(const char *dev) override;

    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n) override;
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

protected:
    virtual void TimerHit() override;

    virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks) override;
    virtual IPState MoveAbsFocuser(uint32_t targetTicks) override;
    virtual bool SyncFocuser(uint32_t ticks) override;
    virtual bool AbortFocuser() override;

    virtual bool saveConfigItems(FILE *fp) override;

private:
    enum STATUSCODE { UNRESPONSIVE, FAILED, UPPERLIMIT, LOWERLIMIT, SLEWING, TRACKING, STILL, OFF };

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

    bool readPosition();

    char* IPaddress;
    char* sendCommand(const char*,...);
    char* sendRequest(const char*,...);

    void *context,*requester;
    int64_t currentPosition;
    int statusCode;

    pthread_mutex_t connectionmutex;
};

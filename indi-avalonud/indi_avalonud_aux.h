/*
  Skeleton Focuser Driver

  Modify this driver when developing new absolute position
  based focusers. This driver uses serial communication by default
  but it can be changed to use networked TCP/UDP connection as well.

  Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

  Thanks to Rigel Systems, especially Gene Nolan and Leon Palmer,
  for their support in writing this driver.

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

    virtual bool Handshake();

    virtual bool Connect();
    virtual bool Disconnect();

    const char *getDefaultName() override;

    virtual bool initProperties() override;
    virtual bool updateProperties() override;
    virtual void ISGetProperties(const char *dev) override;

    virtual bool ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n);
    virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

protected:
    virtual void TimerHit() override;

    virtual bool saveConfigItems(FILE *fp) override;

private:

    int tid;
    unsigned int features;

    IText ConfigT[1];
    ITextVectorProperty ConfigTP;

    IText LowLevelSWT[2];
    ITextVectorProperty LowLevelSWTP;

    IText HWTypeT[1];
    ITextVectorProperty HWTypeTP;

    IText HWIdentifierT[1];
    ITextVectorProperty HWIdentifierTP;

    ISwitch SystemManagementS[2];
    ISwitchVectorProperty SystemManagementSP;

    INumber OUTPortPWMDUTYCYCLEN[1];
    INumberVectorProperty OUTPortPWMDUTYCYCLENP;

    ISwitch OUTPortPWMS[2];
    ISwitchVectorProperty OUTPortPWMSP;

    ISwitch OUTPort1S[2];
    ISwitchVectorProperty OUTPort1SP;

    ISwitch OUTPort2S[2];
    ISwitchVectorProperty OUTPort2SP;

    ISwitch USBPort1S[2];
    ISwitchVectorProperty USBPort1SP;

    ISwitch USBPort2S[2];
    ISwitchVectorProperty USBPort2SP;

    ISwitch USBPort3S[2];
    ISwitchVectorProperty USBPort3SP;

    ISwitch USBPort4S[2];
    ISwitchVectorProperty USBPort4SP;

    INumber PSUN[4];
    INumberVectorProperty PSUNP;

    INumber SMN[3];
    INumberVectorProperty SMNP;

    INumber CPUN[1];
    INumberVectorProperty CPUNP;

    bool readStatus();

    char* IPaddress;
    char* sendCommand(const char*,...);
    char* sendRequest(const char*,...);

    void *context,*requester;
    int outlets;
    time_t reboot_time,shutdown_time;

    pthread_mutex_t connectionmutex;
};

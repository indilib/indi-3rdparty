/*******************************************************************************
  Copyright(c) 2017 Simon Holmbo. All rights reserved.
  Copyright(c) 2018 Jean-Baptiste Butet. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#ifndef SHELYAKSPOX_SPECTROGRAPH_H
#define SHELYAKSPOX_SPECTROGRAPH_H
//On serial connection
//11\n : calib lamp on
//21\n : flat lamp on
//if 11 and 21 : dark on
//(and the opposite, 10, 20)
//00\n : shut off all

//

#include <indiapi.h>
#include "defaultdevice.h"

class ShelyakSpox : public INDI::DefaultDevice
{
public:
    ShelyakSpox();
    ~ShelyakSpox();

    void ISGetProperties(const char *dev) override;
    bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
    bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) override;

protected:
    const char *getDefaultName() override;

    bool initProperties() override;
    bool updateProperties() override;

    bool Connect() override;
    bool Disconnect() override;

private:
    int PortFD; // file descriptor for serial port

    // Main Control
    ISwitchVectorProperty LampSP;
    ISwitch LampS[4];

    // Options
    ITextVectorProperty PortTP;
    IText PortT[1]{};

#if 0 // unused
    // Spectrograph Settings
    INumberVectorProperty SettingsNP;
    INumber SettingsN[2];
#endif

    bool calibrationUnitCommand(char command, char parameter);
    bool resetLamps();
};

#endif // SHELYAKSPOX_SPECTROGRAPH_H

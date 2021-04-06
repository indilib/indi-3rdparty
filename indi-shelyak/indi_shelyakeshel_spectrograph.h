/*******************************************************************************
  Copyright(c) 2017 Simon Holmbo. All rights reserved.

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

#ifndef SHELYAKESHEL_SPECTROGRAPH_H
#define SHELYAKESHEL_SPECTROGRAPH_H


#include "defaultdevice.h"

class ShelyakEshel : public INDI::DefaultDevice
{
public:
    ShelyakEshel();
    ~ShelyakEshel();

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
    ISwitch LampS[3];
    ISwitchVectorProperty MirrorSP;
    ISwitch MirrorS[2];

    // Options
    ITextVectorProperty PortTP;
    IText PortT[1]{};

    // Spectrograph Settings
    INumberVectorProperty SettingsNP;
    INumber SettingsN[5];

    bool calibrationUnitCommand(char command, char parameter);
};

#endif // SHELYAKESHEL_SPECTROGRAPH_H

/*
    Astroasis Oasis Filter Wheel
    Copyright (C) 2013-2019 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2023 Frank Chen (frank.chen@astroasis.com)

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
    USA

*/

#pragma once

#include "indifilterwheel.h"
#include "OasisFilterWheel.h"

class OasisFilterWheel : public INDI::FilterWheel
{
    public:
        OasisFilterWheel();
        virtual ~OasisFilterWheel() override = default;

        const char * getDefaultName() override;
        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool ISNewSwitch(const char * dev, const char * name, ISState * states, char * names[], int n) override;

    protected:
        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual bool GetFilterNames() override;
        virtual bool SetFilterNames() override;
        virtual int QueryFilter() override;
        virtual bool SelectFilter(int) override;
        virtual void TimerHit() override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        uint8_t mID;

        bool GetConfig(OFWConfig *config);

        // Speed
        ISwitchVectorProperty SpeedSP;
        ISwitch SpeedS[3];

        // Auto run on power up
        ISwitchVectorProperty AutoRunSP;
        ISwitch AutoRunS[2];

        // Factory Reset
        ISwitchVectorProperty FactoryResetSP;
        ISwitch FactoryResetS[1];

        // Calibrate
        ISwitchVectorProperty CalibrateSP;
        ISwitch CalibrateS[1];
};

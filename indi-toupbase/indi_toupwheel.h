/*
 Toupcam & oem Filter Wheel Driver

 Copyright (C) 2018-2024 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <indifilterwheel.h>
#include <indipropertytext.h>
#include "libtoupbase.h"

class ToupWheel : public INDI::FilterWheel
{
    public:
        explicit ToupWheel(const XP(DeviceV2) *instance, const char *name);

        virtual const char *getDefaultName() override;

        virtual void ISGetProperties(const char *dev) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;

    protected:
        virtual void TimerHit() override;
        virtual bool SelectFilter(int targetFilter) override;
        virtual int QueryFilter() override;

        // Save config
        virtual bool saveConfigItems(FILE *fp) override;

    private:
        INDI::PropertyText VersionTP {4};
        enum
        {
            TC_FW_VERSION,
            TC_HW_VERSION,
            TC_REV,
            TC_SDK
        };

        INDI::PropertySwitch SlotsSP {3};
        enum
        {
            SLOTS_5,
            SLOTS_7,
            SLOTS_8
        };

        INDI::PropertySwitch SpinningDirectionSP {2};
        enum
        {
            TCFW_SD_CLOCKWISE,
            TCFW_SD_AUTO
        };
        int SpinningDirection = 0;

        THAND m_Handle { nullptr };
        const XP(DeviceV2) *m_Instance;
};

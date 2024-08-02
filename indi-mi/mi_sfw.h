/*
 Moravian INDI Driver

 Copyright (C) 2024 Moravian Instruments (info@gxccd.com)

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

#include <gxccd.h>

#include <indifilterwheel.h>

class MISFW : public INDI::FilterWheel
{
    public:
        MISFW(int wheelId, bool eth = false);
        virtual ~MISFW();

        virtual const char *getDefaultName() override;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual bool Connect() override;
        virtual bool Disconnect() override;

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual bool SelectFilter(int position) override;
        virtual int QueryFilter() override;

        ISwitch ReinitS[1];
        ISwitchVectorProperty ReinitSP;

        IText InfoT[3];
        ITextVectorProperty InfoTP;

    private:
        char name[MAXINDIDEVICE];

        int wheelId;
        fwheel_t *wheelHandle;
        bool isEth;

        int numFilters;

        bool updateFilterProperties();
};

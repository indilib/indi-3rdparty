/*
 PlayerOne Phenix Filter Wheel INDI Driver

 Copyright (c) 2023 Hiroshi Saito (hiro3110g@gmail.com)

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along with
 this program; if not, write to the Free Software Foundation, Inc., 59
 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 The full GNU General Public License is included in this distribution in the
 file called LICENSE.
 */

#pragma once

#include <PlayerOnePW.h>

#include <indifilterwheel.h>

#define EFW_IS_MOVING -1
#define POA_EFW_TIMEOUT 20000		// millisecond

class POAWHEEL : public INDI::FilterWheel
{
    public:
        POAWHEEL(const PWProperties &info, const char *name);
        ~POAWHEEL();

        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual bool Connect() override;
        virtual bool Disconnect() override;
        virtual const char *getDefaultName() override;

        virtual bool initProperties() override;
        virtual bool updateProperties() override;

        virtual int QueryFilter() override;
        virtual bool SelectFilter(int) override;
        virtual void TimerHit() override;

        // Save config
        virtual bool saveConfigItems(FILE *fp) override;

    private:

        // Unidirectional
        ISwitchVectorProperty UniDirectionalSP;
        ISwitch UniDirectionalS[2];

        // Calibrate
        static void TimerHelperCalibrate(void *context);
        void TimerCalibrate();
        ISwitchVectorProperty CalibrateSP;
        ISwitch CalibrateS[1];

    private:
        int fw_id = -1;
};

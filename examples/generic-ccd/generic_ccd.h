/*
 Generic CCD
 CCD Template for INDI Developers
 Copyright (C) 2021 Jasem Mutlaq (mutlaqja@ikarustech.com)

 Multiple device support Copyright (C) 2013 Peter Polakovic (peter.polakovic@cloudmakers.eu)

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

#include <indiccd.h>
#include <iostream>
#include <indielapsedtimer.h>

using namespace std;

#define DEVICE struct usb_device *

class GenericCCD : public INDI::CCD
{
    public:
        GenericCCD(DEVICE device, const char *name);
        virtual ~GenericCCD();

        const char *getDefaultName() override;

        bool initProperties() override;
        bool updateProperties() override;

        bool Connect() override;
        bool Disconnect() override;

        int SetTemperature(double temperature) override;
        bool StartExposure(float duration) override;
        bool AbortExposure() override;

    protected:
        void TimerHit() override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;
        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
        virtual bool UpdateCCDBin(int binx, int biny) override;
        virtual bool UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType) override;

        // Guide Port
        virtual IPState GuideNorth(uint32_t ms) override;
        virtual IPState GuideSouth(uint32_t ms) override;
        virtual IPState GuideEast(uint32_t ms) override;
        virtual IPState GuideWest(uint32_t ms) override;

    private:
        DEVICE device;
        char m_Name[MAXINDINAME];

        INDI::PropertySwitch CrashSP {1};

        INDI::ElapsedTimer m_ElapsedTimer;
        double ExposureRequest;
        double TemperatureRequest;

        int downloadImage();
        bool setupParams();

        static constexpr double TEMP_THRESHOLD {0.01};


};

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

        const char *getDefaultName();

        bool initProperties();
        bool updateProperties();

        bool Connect();
        bool Disconnect();

        int SetTemperature(double temperature);
        bool StartExposure(float duration);
        bool AbortExposure();

    protected:
        void TimerHit();
        virtual bool UpdateCCDFrame(int x, int y, int w, int h);
        virtual bool UpdateCCDBin(int binx, int biny);
        virtual bool UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType);

        // Guide Port
        virtual IPState GuideNorth(uint32_t ms);
        virtual IPState GuideSouth(uint32_t ms);
        virtual IPState GuideEast(uint32_t ms);
        virtual IPState GuideWest(uint32_t ms);

    private:
        DEVICE device;
        char m_Name[MAXINDINAME];

        INDI::ElapsedTimer m_ElapsedTimer;
        double ExposureRequest;
        double TemperatureRequest;

        int downloadImage();
        bool setupParams();

        static constexpr double TEMP_THRESHOLD {0.01};


};


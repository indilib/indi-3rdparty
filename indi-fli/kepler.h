/*
    INDI Driver for Kepler sCMOS camera.
    Copyright (C) 2022 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <libflipro.h>
#include <indiccd.h>
#include <indipropertyswitch.h>

#include <inditimer.h>
#include <indisinglethreadpool.h>

class Kepler : public INDI::CCD
{
    public:
        Kepler(const FPRODEVICEINFO &info, std::wstring name);
        virtual ~Kepler() override;

        const char *getDefaultName() override;

        bool initProperties() override;
        void ISGetProperties(const char *dev) override;
        bool updateProperties() override;

        bool Connect() override;
        bool Disconnect() override;

        bool StartExposure(float duration) override;
        bool AbortExposure() override;

        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;
        virtual bool ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) override;

    protected:
        virtual void TimerHit() override;
        virtual int SetTemperature(double temperature) override;
        virtual bool UpdateCCDFrame(int x, int y, int w, int h) override;
        virtual bool UpdateCCDBin(int binx, int biny) override;
        virtual bool UpdateCCDFrameType(INDI::CCDChip::CCD_FRAME fType) override;

        virtual void debugTriggered(bool enable) override;
        virtual bool saveConfigItems(FILE *fp) override;

    private:

        //****************************************************************************************
        // INDI Properties
        //****************************************************************************************

        INDI::PropertySwitch CommunicationMethod {2};

        //****************************************************************************************
        // Communication Functions
        //****************************************************************************************
        bool establishConnection();
        bool setup();
        bool grabImage();

        //****************************************************************************************
        // Workers
        //****************************************************************************************
        void workerStreamVideo(const std::atomic_bool &isAboutToQuit);
        void workerExposure(const std::atomic_bool &isAboutToQuit, float duration);

};

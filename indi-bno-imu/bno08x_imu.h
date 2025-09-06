/*
    BNO085 IMU Driver
    Copyright (C) 2025 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include <indiimu.h>
#include <connectionplugins/connectioni2c.h>
#include <BNO08x.h>
#include <cmath>

class BNO08X : public INDI::IMU
{
    public:
        BNO08X();
        virtual ~BNO08X() = default;

        virtual const char *getDefaultName() override
        {
            return "BNO08X";
        }

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool Handshake() override;
        virtual void TimerHit() override;

    protected:
        // Implement virtual functions from IMUInterface
        virtual bool SetCalibrationStatus(int sys, int gyro, int accel, int mag) override;
        virtual bool StartCalibration() override;
        virtual bool SaveCalibrationData() override;
        virtual bool LoadCalibrationData() override;
        virtual bool ResetCalibration() override;
        virtual bool SetPowerMode(const std::string &mode) override;
        virtual bool SetOperationMode(const std::string &mode) override;
        virtual bool SetDistanceUnits(bool metric) override;
        virtual bool SetAngularUnits(bool degrees) override;
        virtual bool SetUpdateRate(double rate) override;
        virtual bool SetOffsets(double x, double y, double z) override;
        virtual bool SetDeviceInfo(const std::string &chipID, const std::string &firmwareVersion,
                                   const std::string &sensorStatus) override;
    private:
        BNO08x bno08x; // BNO08x sensor object
        bool readSensorData();
};

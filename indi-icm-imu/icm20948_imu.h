/*
    ICM-20948 IMU Driver
    Copyright (C) 2026 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
#include <ICM20948.h>
#include <cmath>

class ICM20948IMU : public INDI::IMU
{
    public:
        ICM20948IMU();
        virtual ~ICM20948IMU() = default;

        virtual const char *getDefaultName() override
        {
            return "ICM20948";
        }

        virtual bool initProperties() override;
        virtual bool updateProperties() override;
        virtual bool Handshake() override;
        virtual void TimerHit() override;
        virtual bool ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) override;

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
        virtual bool SetDeviceInfo(const std::string &chipID, const std::string &firmwareVersion,
                                   const std::string &sensorStatus) override;
    private:
        ICM20948 icm20948; // ICM20948 sensor object
        bool readSensorData();

        // Calibration offsets
        struct CalibrationOffsets
        {
            double accel[3] = {0.0, 0.0, 0.0};  // Accelerometer offsets (m/s²)
            double gyro[3] = {0.0, 0.0, 0.0};   // Gyroscope offsets (deg/s)
            double mag[3] = {0.0, 0.0, 0.0};    // Magnetometer offsets (µT)
        };

        CalibrationOffsets m_Offsets;

        // Calibration state
        enum CalibrationState
        {
            CAL_IDLE,
            CAL_ACCEL_COLLECTING,
            CAL_GYRO_COLLECTING,
            CAL_MAG_COLLECTING,
            CAL_COMPLETE
        };

        CalibrationState m_CalibrationState = CAL_IDLE;
        int m_CalibrationSamples = 0;
        static constexpr int CALIBRATION_SAMPLES = 100;

        // Temporary storage during calibration
        double m_CalibrationSum[3] = {0.0, 0.0, 0.0};

        // Calibration properties
        INDI::PropertyNumber AccelOffsetNP{3};
        INDI::PropertyNumber GyroOffsetNP{3};
        INDI::PropertyNumber MagOffsetNP{3};

        // Helper methods
        void defineCalibrationProperties();
        void deleteCalibrationProperties();
        bool collectCalibrationSample();
        void updateCalibrationStatus();
};

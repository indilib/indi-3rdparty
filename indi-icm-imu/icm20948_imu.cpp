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

#include "icm20948_imu.h"
#include <indicom.h>
#include <indilogger.h>
#include <cmath>
#include <memory>

std::unique_ptr<ICM20948IMU> imu(new ICM20948IMU());

ICM20948IMU::ICM20948IMU()
{
    // ICM-20948 provides raw sensor data only (no sensor fusion/orientation)
    SetCapability(IMU_HAS_ACCELERATION | IMU_HAS_GYROSCOPE | IMU_HAS_MAGNETOMETER | IMU_HAS_CALIBRATION);

    setSupportedConnections(INDI::IMU::CONNECTION_I2C);
    setDriverInterface(IMU_INTERFACE);
}

bool ICM20948IMU::initProperties()
{
    IMU::initProperties();
    addDebugControl();
    addPollPeriodControl();

    // Define calibration offset properties
    defineCalibrationProperties();

    return true;
}

bool ICM20948IMU::updateProperties()
{
    IMU::updateProperties();

    if (isConnected())
    {
        // Define calibration properties when connected
        defineProperty(AccelOffsetNP);
        defineProperty(GyroOffsetNP);
        defineProperty(MagOffsetNP);
    }
    else
    {
        // Delete calibration properties when disconnected
        deleteProperty(AccelOffsetNP);
        deleteProperty(GyroOffsetNP);
        deleteProperty(MagOffsetNP);
    }

    return true;
}

bool ICM20948IMU::Handshake()
{
    try
    {
        // Initialize the ICM20948 sensor with the I2C file descriptor
        icm20948.begin_i2c(PortFD);

        std::string chipID = "ICM-20948";
        std::string firmwareVersion = "N/A";
        std::string sensorStatus = "Operational";

        // Set device information
        SetDeviceInfo(chipID, firmwareVersion, sensorStatus);

        // Read initial sensor data to verify communication
        icm20948_agmt_t agmt;
        icm20948_status_e status = icm20948.readSensor(&agmt);
        if (status != ICM_20948_STAT_OK)
        {
            LOGF_ERROR("ICM20948: Failed to read sensor data, status: %d", static_cast<int>(status));
            return false;
        }

        LOG_INFO("ICM20948 initialized successfully.");

        // Load calibration data if available
        LoadCalibrationData();

        return true;
    }
    catch (const ICM20948_exception &e)
    {
        LOGF_ERROR("ICM20948 initialization failed: %s", e.what());
        return false;
    }
}

void ICM20948IMU::TimerHit()
{
    if (!isConnected())
        return;

    // Update calibration if in progress
    if (m_CalibrationState != CAL_IDLE)
    {
        collectCalibrationSample();
    }

    readSensorData();

    SetTimer(getPollingPeriod());
}

bool ICM20948IMU::readSensorData()
{
    icm20948_agmt_t agmt;
    icm20948_status_e status = icm20948.readSensor(&agmt);

    if (status == ICM_20948_STAT_OK)
    {
        // Apply calibration offsets and update INDI properties
        double accelX = icm20948.getAccelX_mss() - m_Offsets.accel[0];
        double accelY = icm20948.getAccelY_mss() - m_Offsets.accel[1];
        double accelZ = icm20948.getAccelZ_mss() - m_Offsets.accel[2];
        SetAccelerationData(accelX, accelY, accelZ);

        double gyroX = icm20948.getGyroX_dps() - m_Offsets.gyro[0];
        double gyroY = icm20948.getGyroY_dps() - m_Offsets.gyro[1];
        double gyroZ = icm20948.getGyroZ_dps() - m_Offsets.gyro[2];
        SetGyroscopeData(gyroX, gyroY, gyroZ);

        double magX = icm20948.getMagX_uT() - m_Offsets.mag[0];
        double magY = icm20948.getMagY_uT() - m_Offsets.mag[1];
        double magZ = icm20948.getMagZ_uT() - m_Offsets.mag[2];
        SetMagnetometerData(magX, magY, magZ);

        return true;
    }
    else
    {
        LOGF_ERROR("ICM20948: Failed to read sensor data, status: %d", static_cast<int>(status));
        return false;
    }
}

void ICM20948IMU::defineCalibrationProperties()
{
    // Accelerometer offsets
    AccelOffsetNP[0].fill("ACCEL_X_OFFSET", "X Offset (m/s²)", "%.4f", -50.0, 50.0, 0.01, 0.0);
    AccelOffsetNP[1].fill("ACCEL_Y_OFFSET", "Y Offset (m/s²)", "%.4f", -50.0, 50.0, 0.01, 0.0);
    AccelOffsetNP[2].fill("ACCEL_Z_OFFSET", "Z Offset (m/s²)", "%.4f", -50.0, 50.0, 0.01, 0.0);
    AccelOffsetNP.fill(getDeviceName(), "ACCEL_OFFSETS", "Accel Offsets", "Calibration", IP_RW, 60, IPS_IDLE);

    // Gyroscope offsets
    GyroOffsetNP[0].fill("GYRO_X_OFFSET", "X Offset (°/s)", "%.4f", -500.0, 500.0, 0.01, 0.0);
    GyroOffsetNP[1].fill("GYRO_Y_OFFSET", "Y Offset (°/s)", "%.4f", -500.0, 500.0, 0.01, 0.0);
    GyroOffsetNP[2].fill("GYRO_Z_OFFSET", "Z Offset (°/s)", "%.4f", -500.0, 500.0, 0.01, 0.0);
    GyroOffsetNP.fill(getDeviceName(), "GYRO_OFFSETS", "Gyro Offsets", "Calibration", IP_RW, 60, IPS_IDLE);

    // Magnetometer offsets
    MagOffsetNP[0].fill("MAG_X_OFFSET", "X Offset (µT)", "%.4f", -500.0, 500.0, 0.1, 0.0);
    MagOffsetNP[1].fill("MAG_Y_OFFSET", "Y Offset (µT)", "%.4f", -500.0, 500.0, 0.1, 0.0);
    MagOffsetNP[2].fill("MAG_Z_OFFSET", "Z Offset (µT)", "%.4f", -500.0, 500.0, 0.1, 0.0);
    MagOffsetNP.fill(getDeviceName(), "MAG_OFFSETS", "Mag Offsets", "Calibration", IP_RW, 60, IPS_IDLE);
}

bool ICM20948IMU::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // Handle accelerometer offset changes
        if (AccelOffsetNP.isNameMatch(name))
        {
            AccelOffsetNP.update(values, names, n);
            m_Offsets.accel[0] = AccelOffsetNP[0].getValue();
            m_Offsets.accel[1] = AccelOffsetNP[1].getValue();
            m_Offsets.accel[2] = AccelOffsetNP[2].getValue();
            AccelOffsetNP.setState(IPS_OK);
            AccelOffsetNP.apply();
            LOG_INFO("Accelerometer offsets updated.");
            return true;
        }

        // Handle gyroscope offset changes
        if (GyroOffsetNP.isNameMatch(name))
        {
            GyroOffsetNP.update(values, names, n);
            m_Offsets.gyro[0] = GyroOffsetNP[0].getValue();
            m_Offsets.gyro[1] = GyroOffsetNP[1].getValue();
            m_Offsets.gyro[2] = GyroOffsetNP[2].getValue();
            GyroOffsetNP.setState(IPS_OK);
            GyroOffsetNP.apply();
            LOG_INFO("Gyroscope offsets updated.");
            return true;
        }

        // Handle magnetometer offset changes
        if (MagOffsetNP.isNameMatch(name))
        {
            MagOffsetNP.update(values, names, n);
            m_Offsets.mag[0] = MagOffsetNP[0].getValue();
            m_Offsets.mag[1] = MagOffsetNP[1].getValue();
            m_Offsets.mag[2] = MagOffsetNP[2].getValue();
            MagOffsetNP.setState(IPS_OK);
            MagOffsetNP.apply();
            LOG_INFO("Magnetometer offsets updated.");
            return true;
        }
    }

    return IMU::ISNewNumber(dev, name, values, names, n);
}

bool ICM20948IMU::collectCalibrationSample()
{
    icm20948_agmt_t agmt;
    icm20948_status_e status = icm20948.readSensor(&agmt);

    if (status != ICM_20948_STAT_OK)
        return false;

    switch (m_CalibrationState)
    {
        case CAL_GYRO_COLLECTING:
            // Collect gyroscope samples (device should be stationary)
            m_CalibrationSum[0] += icm20948.getGyroX_dps();
            m_CalibrationSum[1] += icm20948.getGyroY_dps();
            m_CalibrationSum[2] += icm20948.getGyroZ_dps();
            m_CalibrationSamples++;

            if (m_CalibrationSamples >= CALIBRATION_SAMPLES)
            {
                // Calculate average offset
                m_Offsets.gyro[0] = m_CalibrationSum[0] / CALIBRATION_SAMPLES;
                m_Offsets.gyro[1] = m_CalibrationSum[1] / CALIBRATION_SAMPLES;
                m_Offsets.gyro[2] = m_CalibrationSum[2] / CALIBRATION_SAMPLES;

                // Update properties
                GyroOffsetNP[0].setValue(m_Offsets.gyro[0]);
                GyroOffsetNP[1].setValue(m_Offsets.gyro[1]);
                GyroOffsetNP[2].setValue(m_Offsets.gyro[2]);
                GyroOffsetNP.setState(IPS_OK);
                GyroOffsetNP.apply();

                LOG_INFO("Gyroscope calibration complete.");
                LOGF_INFO("Gyro offsets: X=%.4f, Y=%.4f, Z=%.4f deg/s",
                          m_Offsets.gyro[0], m_Offsets.gyro[1], m_Offsets.gyro[2]);

                // Move to accelerometer calibration
                m_CalibrationState = CAL_ACCEL_COLLECTING;
                m_CalibrationSamples = 0;
                m_CalibrationSum[0] = m_CalibrationSum[1] = m_CalibrationSum[2] = 0.0;

                LOG_INFO("Starting accelerometer calibration...");
                LOG_INFO("Place device flat on a level surface and keep it still.");
            }
            break;

        case CAL_ACCEL_COLLECTING:
            // Collect accelerometer samples (device should be flat on level surface)
            m_CalibrationSum[0] += icm20948.getAccelX_mss();
            m_CalibrationSum[1] += icm20948.getAccelY_mss();
            m_CalibrationSum[2] += icm20948.getAccelZ_mss();
            m_CalibrationSamples++;

            if (m_CalibrationSamples >= CALIBRATION_SAMPLES)
            {
                // Calculate offsets (Z-axis should read ~9.81 m/s² when level)
                m_Offsets.accel[0] = m_CalibrationSum[0] / CALIBRATION_SAMPLES;
                m_Offsets.accel[1] = m_CalibrationSum[1] / CALIBRATION_SAMPLES;
                m_Offsets.accel[2] = (m_CalibrationSum[2] / CALIBRATION_SAMPLES) - 9.80665;

                // Update properties
                AccelOffsetNP[0].setValue(m_Offsets.accel[0]);
                AccelOffsetNP[1].setValue(m_Offsets.accel[1]);
                AccelOffsetNP[2].setValue(m_Offsets.accel[2]);
                AccelOffsetNP.setState(IPS_OK);
                AccelOffsetNP.apply();

                LOG_INFO("Accelerometer calibration complete.");
                LOGF_INFO("Accel offsets: X=%.4f, Y=%.4f, Z=%.4f m/s²",
                          m_Offsets.accel[0], m_Offsets.accel[1], m_Offsets.accel[2]);

                // Move to magnetometer calibration
                m_CalibrationState = CAL_MAG_COLLECTING;
                m_CalibrationSamples = 0;
                m_CalibrationSum[0] = m_CalibrationSum[1] = m_CalibrationSum[2] = 0.0;

                LOG_INFO("Starting magnetometer calibration...");
                LOG_INFO("Slowly rotate the device in figure-8 patterns for 10 seconds.");
            }
            break;

        case CAL_MAG_COLLECTING:
        {
            // Collect min/max values for hard iron offset calibration
            static double magMin[3] = {999999.0, 999999.0, 999999.0};
            static double magMax[3] = {-999999.0, -999999.0, -999999.0};

            double magX = icm20948.getMagX_uT();
            double magY = icm20948.getMagY_uT();
            double magZ = icm20948.getMagZ_uT();

            magMin[0] = std::min(magMin[0], magX);
            magMin[1] = std::min(magMin[1], magY);
            magMin[2] = std::min(magMin[2], magZ);

            magMax[0] = std::max(magMax[0], magX);
            magMax[1] = std::max(magMax[1], magY);
            magMax[2] = std::max(magMax[2], magZ);

            m_CalibrationSamples++;

            if (m_CalibrationSamples >= CALIBRATION_SAMPLES * 2) // More samples for mag
            {
                // Calculate hard iron offset (center of min/max sphere)
                m_Offsets.mag[0] = (magMax[0] + magMin[0]) / 2.0;
                m_Offsets.mag[1] = (magMax[1] + magMin[1]) / 2.0;
                m_Offsets.mag[2] = (magMax[2] + magMin[2]) / 2.0;

                // Update properties
                MagOffsetNP[0].setValue(m_Offsets.mag[0]);
                MagOffsetNP[1].setValue(m_Offsets.mag[1]);
                MagOffsetNP[2].setValue(m_Offsets.mag[2]);
                MagOffsetNP.setState(IPS_OK);
                MagOffsetNP.apply();

                LOG_INFO("Magnetometer calibration complete.");
                LOGF_INFO("Mag offsets: X=%.4f, Y=%.4f, Z=%.4f µT",
                          m_Offsets.mag[0], m_Offsets.mag[1], m_Offsets.mag[2]);

                // Reset static variables for next calibration
                magMin[0] = magMin[1] = magMin[2] = 999999.0;
                magMax[0] = magMax[1] = magMax[2] = -999999.0;

                // Calibration complete
                m_CalibrationState = CAL_COMPLETE;
                updateCalibrationStatus();

                LOG_INFO("All calibrations complete! You can now save the calibration data.");
            }
        }
        break;

        default:
            break;
    }

    return true;
}

void ICM20948IMU::updateCalibrationStatus()
{
    // Update calibration status based on offsets
    auto getCalibrationState = [](double offset) -> IPState
    {
        if (std::abs(offset) < 0.01)
            return IPS_ALERT;   // Not calibrated
        else
            return IPS_OK;      // Calibrated
    };

    // For now, use simple logic: if offsets are non-zero, consider calibrated
    bool accelCal = (std::abs(m_Offsets.accel[0]) > 0.01 ||
                     std::abs(m_Offsets.accel[1]) > 0.01 ||
                     std::abs(m_Offsets.accel[2]) > 0.01);
    bool gyroCal = (std::abs(m_Offsets.gyro[0]) > 0.01 ||
                    std::abs(m_Offsets.gyro[1]) > 0.01 ||
                    std::abs(m_Offsets.gyro[2]) > 0.01);
    bool magCal = (std::abs(m_Offsets.mag[0]) > 0.01 ||
                   std::abs(m_Offsets.mag[1]) > 0.01 ||
                   std::abs(m_Offsets.mag[2]) > 0.01);

    SetCalibrationStatus(
        (accelCal && gyroCal && magCal) ? 3 : 0,  // System
        gyroCal ? 3 : 0,                           // Gyro
        accelCal ? 3 : 0,                          // Accel
        magCal ? 3 : 0                             // Mag
    );
}

bool ICM20948IMU::SetCalibrationStatus(int sys, int gyro, int accel, int mag)
{
    // Convert calibration values (0-3) to light states
    auto getCalibrationState = [](int value) -> IPState
    {
        switch (value)
        {
            case 0:
                return IPS_ALERT;   // Not calibrated
            case 1:
                return IPS_BUSY;    // Partially calibrated
            case 2:
                return IPS_BUSY;    // Mostly calibrated
            case 3:
                return IPS_OK;      // Fully calibrated
            default:
                return IPS_IDLE;
        }
    };

    CalibrationStatusLP[0].setState(getCalibrationState(sys));
    CalibrationStatusLP[1].setState(getCalibrationState(gyro));
    CalibrationStatusLP[2].setState(getCalibrationState(accel));
    CalibrationStatusLP[3].setState(getCalibrationState(mag));
    CalibrationStatusLP.apply();
    return true;
}

bool ICM20948IMU::StartCalibration()
{
    LOG_INFO("ICM20948: Starting calibration sequence.");
    LOG_INFO("Step 1: Gyroscope calibration - Place device on a stable, stationary surface.");
    LOG_INFO("Keep the device perfectly still for the next 10 seconds...");

    // Start with gyroscope calibration
    m_CalibrationState = CAL_GYRO_COLLECTING;
    m_CalibrationSamples = 0;
    m_CalibrationSum[0] = m_CalibrationSum[1] = m_CalibrationSum[2] = 0.0;

    return true;
}

bool ICM20948IMU::SaveCalibrationData()
{
    LOG_INFO("ICM20948: Saving calibration data to config file.");

    // Save to INDI config file
    saveConfig(AccelOffsetNP);
    saveConfig(GyroOffsetNP);
    saveConfig(MagOffsetNP);

    LOG_INFO("ICM20948: Calibration data saved successfully.");
    return true;
}

bool ICM20948IMU::LoadCalibrationData()
{
    LOG_INFO("ICM20948: Loading calibration data from config file.");

    // Load from INDI config file
    loadConfig(AccelOffsetNP);
    loadConfig(GyroOffsetNP);
    loadConfig(MagOffsetNP);

    // Update internal offsets
    m_Offsets.accel[0] = AccelOffsetNP[0].getValue();
    m_Offsets.accel[1] = AccelOffsetNP[1].getValue();
    m_Offsets.accel[2] = AccelOffsetNP[2].getValue();

    m_Offsets.gyro[0] = GyroOffsetNP[0].getValue();
    m_Offsets.gyro[1] = GyroOffsetNP[1].getValue();
    m_Offsets.gyro[2] = GyroOffsetNP[2].getValue();

    m_Offsets.mag[0] = MagOffsetNP[0].getValue();
    m_Offsets.mag[1] = MagOffsetNP[1].getValue();
    m_Offsets.mag[2] = MagOffsetNP[2].getValue();

    updateCalibrationStatus();

    LOG_INFO("ICM20948: Calibration data loaded successfully.");
    LOGF_INFO("Accel offsets: X=%.4f, Y=%.4f, Z=%.4f m/s²",
              m_Offsets.accel[0], m_Offsets.accel[1], m_Offsets.accel[2]);
    LOGF_INFO("Gyro offsets: X=%.4f, Y=%.4f, Z=%.4f deg/s",
              m_Offsets.gyro[0], m_Offsets.gyro[1], m_Offsets.gyro[2]);
    LOGF_INFO("Mag offsets: X=%.4f, Y=%.4f, Z=%.4f µT",
              m_Offsets.mag[0], m_Offsets.mag[1], m_Offsets.mag[2]);

    return true;
}

bool ICM20948IMU::ResetCalibration()
{
    LOG_INFO("ICM20948: Resetting calibration data.");

    // Reset all offsets to zero
    m_Offsets.accel[0] = m_Offsets.accel[1] = m_Offsets.accel[2] = 0.0;
    m_Offsets.gyro[0] = m_Offsets.gyro[1] = m_Offsets.gyro[2] = 0.0;
    m_Offsets.mag[0] = m_Offsets.mag[1] = m_Offsets.mag[2] = 0.0;

    // Update properties
    AccelOffsetNP[0].setValue(0.0);
    AccelOffsetNP[1].setValue(0.0);
    AccelOffsetNP[2].setValue(0.0);
    AccelOffsetNP.setState(IPS_IDLE);
    AccelOffsetNP.apply();

    GyroOffsetNP[0].setValue(0.0);
    GyroOffsetNP[1].setValue(0.0);
    GyroOffsetNP[2].setValue(0.0);
    GyroOffsetNP.setState(IPS_IDLE);
    GyroOffsetNP.apply();

    MagOffsetNP[0].setValue(0.0);
    MagOffsetNP[1].setValue(0.0);
    MagOffsetNP[2].setValue(0.0);
    MagOffsetNP.setState(IPS_IDLE);
    MagOffsetNP.apply();

    // Reset calibration state
    m_CalibrationState = CAL_IDLE;
    updateCalibrationStatus();

    LOG_INFO("ICM20948: Calibration data reset complete.");
    return true;
}

bool ICM20948IMU::SetPowerMode(const std::string &mode)
{
    LOGF_INFO("ICM20948: Power mode setting not implemented (requested: %s).", mode.c_str());
    return true;
}

bool ICM20948IMU::SetOperationMode(const std::string &mode)
{
    LOGF_INFO("ICM20948: Operation mode setting not implemented (requested: %s).", mode.c_str());
    return true;
}

bool ICM20948IMU::SetDistanceUnits(bool metric)
{
    LOGF_INFO("ICM20948: Distance units are always metric (requested metric: %d).", metric);
    return true;
}

bool ICM20948IMU::SetAngularUnits(bool degrees)
{
    LOGF_INFO("ICM20948: Angular units are always degrees (requested degrees: %d).", degrees);
    return true;
}

bool ICM20948IMU::SetUpdateRate(double rate)
{
    LOGF_INFO("ICM20948: Update rate controlled by polling period (requested: %.2f Hz).", rate);
    return true;
}

bool ICM20948IMU::SetDeviceInfo(const std::string &chipID, const std::string &firmwareVersion,
                                const std::string &sensorStatus)
{
    DeviceInfoTP[0].setText(chipID);
    DeviceInfoTP[1].setText(firmwareVersion);
    DeviceInfoTP[2].setText(sensorStatus);
    DeviceInfoTP.setState(IPS_OK);
    DeviceInfoTP.apply();
    return true;
}

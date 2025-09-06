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

#include "bno08x_imu.h"
#include <indicom.h>
#include <indilogger.h>
#include <sh2_SensorValue.h>
#include <sh2.h>
#include <sh2_err.h>
#include <cmath>

std::unique_ptr<BNO08X> imu(new BNO08X());

BNO08X::BNO08X()
{
    SetCapability(IMU_HAS_ORIENTATION | IMU_HAS_ACCELERATION | IMU_HAS_GYROSCOPE | IMU_HAS_MAGNETOMETER |
                  IMU_HAS_CALIBRATION);

    setSupportedConnections(INDI::IMU::CONNECTION_I2C);
    setDriverInterface(IMU_INTERFACE);
}

bool BNO08X::initProperties()
{
    IMU::initProperties();
    addDebugControl();
    addPollPeriodControl();
    return true;
}

bool BNO08X::updateProperties()
{
    IMU::updateProperties();
    return true;
}

bool BNO08X::Handshake()
{
    try
    {
        // Initialize the BNO08x sensor with the I2C file descriptor
        bno08x.begin_i2c(PortFD);

        std::string chipID = "N/A";
        std::string firmwareVersion = "N/A";
        std::string sensorStatus = "N/A";

        if (bno08x.wasReset())
        {
            LOG_INFO("BNO08X sensor reset detected - normal startup behavior.");
            sensorStatus = "Reset Detected";
        }
        else
        {
            LOG_INFO("BNO08X: No sensor reset detected.");
            sensorStatus = "Operational";
        }

        // Retrieve product ID information
        if (bno08x.prodIds.numEntries > 0)
        {
            const sh2_ProductId_t &entry = bno08x.prodIds.entry[0];
            chipID = std::to_string(entry.swPartNumber);
            firmwareVersion = std::to_string(entry.swVersionMajor) + "." +
                              std::to_string(entry.swVersionMinor) + "." +
                              std::to_string(entry.swVersionPatch);
        }

        // Set device information
        SetDeviceInfo(chipID, firmwareVersion, sensorStatus);

        // Enable desired reports
        if (!bno08x.enableReport(SH2_ROTATION_VECTOR, 10000)) // 10ms update rate
        {
            LOG_ERROR("BNO08X: Failed to enable Rotation Vector report.");
            return false;
        }
        if (!bno08x.enableReport(SH2_ACCELEROMETER, 10000)) // 10ms update rate
        {
            LOG_ERROR("BNO08X: Failed to enable Accelerometer report.");
            return false;
        }
        if (!bno08x.enableReport(SH2_GYROSCOPE_CALIBRATED, 10000)) // 10ms update rate
        {
            LOG_ERROR("BNO08X: Failed to enable Gyroscope report.");
            return false;
        }
        if (!bno08x.enableReport(SH2_MAGNETIC_FIELD_CALIBRATED, 10000)) // 10ms update rate
        {
            LOG_ERROR("BNO08X: Failed to enable Magnetometer report.");
            return false;
        }
        if (!bno08x.enableReport(SH2_GEOMAGNETIC_ROTATION_VECTOR, 10000)) // 10ms update rate
        {
            LOG_ERROR("BNO08X: Failed to enable Geomagnetic Rotation Vector report.");
            return false;
        }
        if (!bno08x.enableReport(SH2_LINEAR_ACCELERATION, 10000)) // 10ms update rate
        {
            LOG_ERROR("BNO08X: Failed to enable Linear Acceleration report.");
            return false;
        }
        if (!bno08x.enableReport(SH2_GRAVITY, 10000)) // 10ms update rate
        {
            LOG_ERROR("BNO08X: Failed to enable Gravity report.");
            return false;
        }
        if (!bno08x.enableReport(SH2_TAP_DETECTOR, 10000)) // 10ms update rate
        {
            LOG_ERROR("BNO08X: Failed to enable Tap Detector report.");
            return false;
        }
        if (!bno08x.enableReport(SH2_STEP_DETECTOR, 10000)) // 10ms update rate
        {
            LOG_ERROR("BNO08X: Failed to enable Step Detector report.");
            return false;
        }
        if (!bno08x.enableReport(SH2_STABILITY_CLASSIFIER, 10000)) // 10ms update rate
        {
            LOG_ERROR("BNO08X: Failed to enable Stability Classifier report.");
            return false;
        }
        if (!bno08x.enableReport(SH2_RAW_ACCELEROMETER, 10000)) // 10ms update rate
        {
            LOG_ERROR("BNO08X: Failed to enable Raw Accelerometer report.");
            return false;
        }
        if (!bno08x.enableReport(SH2_RAW_GYROSCOPE, 10000)) // 10ms update rate
        {
            LOG_ERROR("BNO08X: Failed to enable Raw Gyroscope report.");
            return false;
        }
        if (!bno08x.enableReport(SH2_RAW_MAGNETOMETER, 10000)) // 10ms update rate
        {
            LOG_ERROR("BNO08X: Failed to enable Raw Magnetometer report.");
            return false;
        }

        LOG_INFO("BNO08X initialized and reports enabled successfully.");
        return true;
    }
    catch (const BNO08x_exception &e)
    {
        LOGF_ERROR("BNO08X initialization failed: %s", e.what());
        return false;
    }
}

void BNO08X::TimerHit()
{
    if (!isConnected())
        return;

    readSensorData();

    SetTimer(getPollingPeriod());
}

bool BNO08X::readSensorData()
{
    sh2_SensorValue_t sensorValue;
    if (bno08x.getSensorEvent(&sensorValue))
    {
        switch (sensorValue.sensorId)
        {
            case SH2_ROTATION_VECTOR:
            case SH2_GAME_ROTATION_VECTOR:
            case SH2_GEOMAGNETIC_ROTATION_VECTOR:
                SetOrientationData(sensorValue.un.rotationVector.i,
                                   sensorValue.un.rotationVector.j,
                                   sensorValue.un.rotationVector.k,
                                   sensorValue.un.rotationVector.real);
                break;

            case SH2_ACCELEROMETER:
            case SH2_LINEAR_ACCELERATION:
            case SH2_GRAVITY:
                SetAccelerationData(sensorValue.un.accelerometer.x,
                                    sensorValue.un.accelerometer.y,
                                    sensorValue.un.accelerometer.z);
                break;

            case SH2_GYROSCOPE_CALIBRATED:
            case SH2_GYROSCOPE_UNCALIBRATED:
                SetGyroscopeData(sensorValue.un.gyroscope.x,
                                 sensorValue.un.gyroscope.y,
                                 sensorValue.un.gyroscope.z);
                break;

            case SH2_MAGNETIC_FIELD_CALIBRATED:
            case SH2_MAGNETIC_FIELD_UNCALIBRATED:
                SetMagnetometerData(sensorValue.un.magneticField.x,
                                    sensorValue.un.magneticField.y,
                                    sensorValue.un.magneticField.z);
                break;

            case SH2_TAP_DETECTOR:
                LOGF_INFO("BNO08X: Tap detected! Flags: %d", sensorValue.un.tapDetector.flags);
                break;

            case SH2_STEP_DETECTOR:
                LOGF_DEBUG("BNO08X: Step detected! Latency: %d us", sensorValue.un.stepDetector.latency);
                break;

            case SH2_STABILITY_CLASSIFIER:
                LOGF_DEBUG("BNO08X: Stability Classifier: %d", sensorValue.un.stabilityClassifier.classification);
                break;

            case SH2_RAW_ACCELEROMETER:
                // Handle raw data if needed, but usually calibrated data is preferred
                break;
            case SH2_RAW_GYROSCOPE:
                // Handle raw data if needed
                break;
            case SH2_RAW_MAGNETOMETER:
                // Handle raw data if needed
                break;

            default:
                LOGF_DEBUG("BNO08X: Unhandled sensor event ID: %d", sensorValue.sensorId);
                break;
        }

        // Update calibration status if available
        SetCalibrationStatus(sensorValue.status & 0x03, // System calibration
                             (sensorValue.status >> 2) & 0x03, // Gyro calibration
                             (sensorValue.status >> 4) & 0x03, // Accelerometer calibration
                             (sensorValue.status >> 6) & 0x03); // Magnetometer calibration

        return true;
    }
    return false;
}

bool BNO08X::SetCalibrationStatus(int sys, int gyro, int accel, int mag)
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

bool BNO08X::StartCalibration()
{
    LOG_INFO("BNO08X: Starting calibration. Please move the device as follows:");
    LOG_INFO("  - Accelerometer (3D): Move into 4-6 unique orientations, hold each for ~1s.");
    LOG_INFO("  - Accelerometer (planar): Rotate around Z-axis by at least 180 degrees.");
    LOG_INFO("  - Gyroscope: Place on a stationary surface for 2-3 seconds.");
    LOG_INFO("  - Magnetometer: Rotate 180 degrees and back in each axis (roll, pitch, yaw) for ~2s per axis.");

    // Enable dynamic calibration for Accel, Gyro, Mag, Planar Accel, On Table Cal
    uint8_t sensorsToCalibrate = SH2_CAL_ACCEL | SH2_CAL_GYRO | SH2_CAL_MAG | SH2_CAL_PLANAR;
    int status = sh2_setCalConfig(sensorsToCalibrate);

    if (status != SH2_OK)
    {
        LOGF_ERROR("BNO08X: Failed to enable ME Calibration, status: %d", status);
        return false;
    }

    LOG_INFO("BNO08X: ME Calibration enabled. Sensor will self-calibrate with motion.");
    return true;
}

bool BNO08X::SaveCalibrationData()
{
    LOG_INFO("BNO08X: Saving calibration data to FRS.");

    int status = sh2_saveDcdNow();
    if (status != SH2_OK)
    {
        LOGF_ERROR("BNO08X: Failed to save calibration data, status: %d", status);
        return false;
    }

    LOG_INFO("BNO08X: Calibration data save command sent. Data should persist across non-power-up resets.");
    return true;
}

bool BNO08X::LoadCalibrationData()
{
    // This is a complex operation involving FRS read/write.
    // For now, we'll log a message indicating it's not fully implemented.
    // The BNO08x automatically loads DCD from FRS on non-power-up resets.
    LOG_INFO("BNO08X: Loading calibration data is a complex FRS operation and is not fully implemented in this driver.");
    LOG_INFO("BNO08X: Dynamic Calibration Data (DCD) is automatically loaded from FRS on non-power-up resets.");
    return false; // Indicate not fully implemented
}

bool BNO08X::ResetCalibration()
{
    LOG_INFO("BNO08X: Resetting calibration data and performing a soft reset.");

    int status = sh2_clearDcdAndReset();
    if (status != SH2_OK)
    {
        LOGF_ERROR("BNO08X: Failed to reset calibration data, status: %d", status);
        return false;
    }

    LOG_INFO("BNO08X: Calibration data cleared and sensor reset. Calibration will restart from scratch.");
    return true;
}

bool BNO08X::SetPowerMode(const std::string &mode)
{
    // TODO: Implement BNO08X power mode setting
    LOGF_INFO("BNO08X: Setting power mode to %s.", mode.c_str());
    return true;
}

bool BNO08X::SetOperationMode(const std::string &mode)
{
    // TODO: Implement BNO08X operation mode setting
    LOGF_INFO("BNO08X: Setting operation mode to %s.", mode.c_str());
    return true;
}

bool BNO08X::SetDistanceUnits(bool metric)
{
    // TODO: Implement BNO08X distance unit setting
    LOGF_INFO("BNO08X: Setting distance units (metric: %d).", metric);
    return true;
}

bool BNO08X::SetAngularUnits(bool degrees)
{
    // TODO: Implement BNO08X angular unit setting
    LOGF_INFO("BNO08X: Setting angular units (degrees: %d).", degrees);
    return true;
}

bool BNO08X::SetUpdateRate(double rate)
{
    // TODO: Implement BNO08X update rate setting
    LOGF_INFO("BNO08X: Setting update rate to %f Hz.", rate);
    return true;
}

bool BNO08X::SetOffsets(double x, double y, double z)
{
    // TODO: Implement BNO08X offset setting
    LOGF_INFO("BNO08X: Setting offsets (x: %f, y: %f, z: %f).", x, y, z);
    return true;
}

bool BNO08X::SetDeviceInfo(const std::string &chipID, const std::string &firmwareVersion, const std::string &sensorStatus)
{
    DeviceInfoTP[0].setText(chipID);
    DeviceInfoTP[1].setText(firmwareVersion);
    DeviceInfoTP[2].setText(sensorStatus);
    DeviceInfoTP.setState(IPS_OK);
    DeviceInfoTP.apply();
    return true;
}

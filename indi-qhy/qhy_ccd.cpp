/*
 QHY INDI Driver

 Copyright (C) 2014 Jasem Mutlaq (mutlaqja@ikarustech.com)
 Copyright (C) 2014 Zhirong Li (lzr@qhyccd.com)
 Copyright (C) 2015 Peter Polakovic (peter.polakovic@cloudmakers.eu)

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

#include "qhy_ccd.h"
#include "config.h"
#include <stream/streammanager.h>

#include <libnova/julian_day.h>
#include <algorithm>
#include <map>
#include <math.h>
#include <memory>
#include <deque>

#define UPDATE_THRESHOLD       0.05   /* Differential temperature threshold (C)*/

//NB Disable for real driver
//#define USE_SIMULATION

static class Loader
{
        std::deque<std::unique_ptr<QHYCCD>> cameras;
    public:
        Loader()
        {
#if !defined(USE_SIMULATION)
            int ret = InitQHYCCDResource();

            if (ret != QHYCCD_SUCCESS)
            {
                IDLog("Init QHYCCD SDK failed (%d)\n", ret);
                return;
            }
#endif

            //#if defined(__APPLE__)
            //    char driverSupportPath[128];
            //    if (getenv("INDIPREFIX") != nullptr)
            //        sprintf(driverSupportPath, "%s/Contents/Resources", getenv("INDIPREFIX"));
            //    else
            //        strncpy(driverSupportPath, "/usr/local/lib/indi", 128);
            //    strncat(driverSupportPath, "/DriverSupport/qhy/firmware", 128);
            //    IDLog("QHY firmware path: %s\n", driverSupportPath);
            //    OSXInitQHYCCDFirmware(driverSupportPath);
            //#endif

            // JM 2019-03-07: Use OSXInitQHYCCDFirmwareArray as recommended by QHY
#if defined(__APPLE__)
            OSXInitQHYCCDFirmwareArray();
            // Wait a bit before calling GetDeviceIDs on MacOS
            usleep(2000000);
#endif

            for (const auto &deviceId : GetDevicesIDs())
            {
                cameras.push_back(std::unique_ptr<QHYCCD>(new QHYCCD(deviceId.c_str())));
            }
        }

        ~Loader()
        {
            ReleaseQHYCCDResource();
        }

    public:

        // Scan for the available devices
        std::vector<std::string> GetDevicesIDs()
        {
            char camid[MAXINDIDEVICE];
            int deviceCount = 0;
            std::vector<std::string> devices;

#if defined(USE_SIMULATION)
            deviceCount = 2;
#else
            deviceCount = ScanQHYCCD();
#endif

            for (int i = 0; i < deviceCount; i++)
            {
#if defined(USE_SIMULATION)
                int ret = QHYCCD_SUCCESS;
                snprintf(camid, MAXINDIDEVICE, "Model %d", i + 1);
#else
                int ret = GetQHYCCDId(i, camid);
#endif
                if (ret == QHYCCD_SUCCESS)
                {
                    devices.push_back(std::string(camid));
                }
                else
                {
                    IDLog("#%d GetQHYCCDId error (%d)\n", i, ret);
                }
            }

            return devices;
        }
} loader;

QHYCCD::QHYCCD(const char *name) : FilterInterface(this)
{
    HasUSBTraffic = false;
    HasUSBSpeed   = false;
    HasGain       = false;
    HasOffset     = false;
    HasFilters    = false;

    snprintf(this->m_Name, MAXINDINAME, "QHY CCD %.15s", name);
    snprintf(this->m_CamID, MAXINDINAME, "%s", name);
    setDeviceName(this->m_Name);

    setVersion(INDI_QHY_VERSION_MAJOR, INDI_QHY_VERSION_MINOR);

    m_QHYLogCallback = [this](const std::string & message)
    {
        logQHYMessages(message);
    };

    // We only want to log to the function above
    EnableQHYCCDLogFile(false);
    EnableQHYCCDMessage(false);

    // Set verbose level to Error/Fatal only by default
    SetQHYCCDLogLevel(2);
}

const char *QHYCCD::getDefaultName()
{
    return "QHY CCD";
}

bool QHYCCD::initProperties()
{
    INDI::CCD::initProperties();
    INDI::FilterInterface::initProperties(FILTER_TAB);

    FilterSlotN[0].min = 1;
    FilterSlotN[0].max = 9;

    // QHY SDK Version
    IUFillText(&SDKVersionT[0], "VERSION", "Version", "NA");
    IUFillTextVector(&SDKVersionTP, SDKVersionT, 1, getDeviceName(), "SDK_VERSION", "SDK", "General Info", IP_RO, 60, IPS_OK);

    // CCD Cooler Switch
    IUFillSwitch(&CoolerS[0], "COOLER_ON", "On", ISS_OFF);
    IUFillSwitch(&CoolerS[1], "COOLER_OFF", "Off", ISS_ON);
    IUFillSwitchVector(&CoolerSP, CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // CCD Regulation power
    IUFillNumber(&CoolerN[0], "CCD_COOLER_VALUE", "Cooling Power (%)", "%+06.2f", 0., 100., 5, 0.0);
    IUFillNumberVector(&CoolerNP, CoolerN, 1, getDeviceName(), "CCD_COOLER_POWER", "Cooling Power", MAIN_CONTROL_TAB,
                       IP_RO, 60, IPS_IDLE);

    // CCD Gain
    IUFillNumber(&GainN[0], "GAIN", "Gain", "%.f", 0, 100, 1, 11);
    IUFillNumberVector(&GainNP, GainN, 1, getDeviceName(), "CCD_GAIN", "Gain", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // CCD Offset
    IUFillNumber(&OffsetN[0], "OFFSET", "Offset", "%.f", 0, 0, 1, 0);
    IUFillNumberVector(&OffsetNP, OffsetN, 1, getDeviceName(), "CCD_OFFSET", "Offset", MAIN_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);

    // USB Speed
    IUFillNumber(&SpeedN[0], "SPEED", "Speed", "%.f", 0, 0, 1, 0);
    IUFillNumberVector(&SpeedNP, SpeedN, 1, getDeviceName(), "USB_SPEED", "USB Speed", MAIN_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);

    // Read Modes (initial support for QHY42Pro)
    IUFillNumber(&ReadModeN[0], "MODE", "Mode", "%.f", 0, 1, 1, 0);
    IUFillNumberVector(&ReadModeNP, ReadModeN, 1, getDeviceName(), "READ_MODE", "Read Mode", MAIN_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);

    // USB Traffic
    IUFillNumber(&USBTrafficN[0], "TRAFFIC", "Speed", "%.f", 0, 0, 1, 0);
    IUFillNumberVector(&USBTrafficNP, USBTrafficN, 1, getDeviceName(), "USB_TRAFFIC", "USB Traffic", MAIN_CONTROL_TAB,
                       IP_RW, 60, IPS_IDLE);

    // USB Buffer
    IUFillNumber(&USBBufferN[0], "BUFFER", "Bytes", "%.f", 512, 4096, 512, 512);
    IUFillNumberVector(&USBBufferNP, USBBufferN, 1, getDeviceName(), "USB_BUFFER", "USB Buffer", MAIN_CONTROL_TAB,
                       IP_RW, 60, IPS_IDLE);

    // Humidity
    IUFillNumber(&HumidityN[0], "HUMIDITY", "%", "%.2f", -100, 1000, 0.1, 0);
    IUFillNumberVector(&HumidityNP, HumidityN, 1, getDeviceName(), "CCD_HUMIDITY", "Humidity", MAIN_CONTROL_TAB,
                       IP_RO, 60, IPS_IDLE);

    // Cooler Mode
    IUFillSwitch(&CoolerModeS[COOLER_AUTOMATIC], "COOLER_AUTOMATIC", "Auto", ISS_ON);
    IUFillSwitch(&CoolerModeS[COOLER_MANUAL], "COOLER_MANUAL", "Manual", ISS_OFF);
    IUFillSwitchVector(&CoolerModeSP, CoolerModeS, 2, getDeviceName(), "CCD_COOLER_MODE", "Cooler Mode", MAIN_CONTROL_TAB,
                       IP_RO, ISR_1OFMANY, 0, IPS_IDLE);


    //NEW CODE - Add support for overscan/calibration area
    IUFillSwitch(&OverscanAreaS[0], "INDI_ENABLED", "Include", ISS_OFF);
    IUFillSwitch(&OverscanAreaS[1], "INDI_DISABLED", "Ignore", ISS_ON);
    IUFillSwitchVector(&OverscanAreaSP, OverscanAreaS, 2, getDeviceName(), "OVERSCAN_MODE", "Overscan Area", MAIN_CONTROL_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /////////////////////////////////////////////////////////////////////////////
    /// Properties: Utility Controls
    /////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&AMPGlowS[AMP_AUTO], "AMP_AUTO", "Auto", ISS_ON);
    IUFillSwitch(&AMPGlowS[AMP_ON], "AMP_ON", "On", ISS_OFF);
    IUFillSwitch(&AMPGlowS[AMP_OFF], "AMP_OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&AMPGlowSP, AMPGlowS, 3, getDeviceName(), "CCD_AMP_GLOW", "Amp Glow", MAIN_CONTROL_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /////////////////////////////////////////////////////////////////////////////
    /// Properties: GPS Controls
    /////////////////////////////////////////////////////////////////////////////

    // Slaving Mode
    IUFillSwitch(&GPSSlavingS[SLAVING_MASTER], "SLAVING_MASTER", "Master", ISS_ON);
    IUFillSwitch(&GPSSlavingS[SLAVING_SLAVE], "SLAVING_SLAVE", "Slave", ISS_OFF);
    IUFillSwitchVector(&GPSSlavingSP, GPSSlavingS, 2, getDeviceName(), "SLAVING_MODE", "Slaving", GPS_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // Slaving Params (for slaves only)
    IUFillNumber(&GPSSlavingParamN[PARAM_TARGET_SEC], "PARAM_TARGET_SEC", "Target sec", "%.f", 0, 1e9, 0, 0);
    IUFillNumber(&GPSSlavingParamN[PARAM_TARGET_USEC], "PARAM_TARGET_USEC", "Target us", "%.f", 0, 1e9, 0, 0);
    IUFillNumber(&GPSSlavingParamN[PARAM_DELTAT_SEC], "PARAM_DELTAT_SEC", "Delta sec", "%.f", 0, 1e9, 0, 0);
    IUFillNumber(&GPSSlavingParamN[PARAM_DELTAT_USEC], "PARAM_DELTAT_USEC", "Delta us", "%.f", 0, 1e9, 0, 0);
    IUFillNumber(&GPSSlavingParamN[PARAM_EXP_TIME], "PARAM_EXP_TIME", "Exp sec", "%.6f", 0.000001, 3600, 0, 0);
    IUFillNumberVector(&GPSSlavingParamNP, GPSSlavingParamN, 5, getDeviceName(), "GPS_SLAVING_PARAMS", "Params",
                       GPS_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // VCOX Frequency
    IUFillNumber(&VCOXFreqN[0], "FREQUENCY", "Freq", "%.f", 0, 4095, 100, 0);
    IUFillNumberVector(&VCOXFreqNP, VCOXFreqN, 1, getDeviceName(), "VCOX_FREQUENCY", "VCOX", GPS_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);

    // LED Calibration
    IUFillSwitch(&GPSLEDCalibrationS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&GPSLEDCalibrationS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&GPSLEDCalibrationSP, GPSLEDCalibrationS, 2, getDeviceName(), "LED_CALIBRATION", "Calibration LED",
                       GPS_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // LED Pulse Position for Starting/Stopping Exposure
    IUFillNumber(&GPSLEDStartPosN[LED_PULSE_POSITION], "LED_PULSE_POSITION", "Pos", "%.f", 2850, 999999, 1000, 0);
    IUFillNumber(&GPSLEDStartPosN[LED_PULSE_WIDTH], "LED_PULSE_WIDTH", "DT", "%.f", 10, 255, 10, 100);
    IUFillNumberVector(&GPSLEDStartPosNP, GPSLEDStartPosN, 2, getDeviceName(), "LED_START_POS", "LED Start", GPS_CONTROL_TAB,
                       IP_RW, 60, IPS_IDLE);
    IUFillNumber(&GPSLEDEndPosN[LED_PULSE_POSITION], "LED_PULSE_POSITION", "Pos", "%.f", 2850, 999999, 1000, 0);
    IUFillNumber(&GPSLEDEndPosN[LED_PULSE_WIDTH], "LED_PULSE_WIDTH", "DT", "%.f", 10, 255, 10, 100);
    IUFillNumberVector(&GPSLEDEndPosNP, GPSLEDEndPosN, 2, getDeviceName(), "LED_END_POS", "LED End", GPS_CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);

    // GPS header On/Off
    IUFillSwitch(&GPSControlS[INDI_ENABLED], "INDI_ENABLED", "Enable", ISS_OFF);
    IUFillSwitch(&GPSControlS[INDI_DISABLED], "INDI_DISABLED", "Disable", ISS_ON);
    IUFillSwitchVector(&GPSControlSP, GPSControlS, 2, getDeviceName(), "GPS_CONTROL", "GPS Header", GPS_CONTROL_TAB,
                       IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    /////////////////////////////////////////////////////////////////////////////
    /// Properties: GPS Data
    /////////////////////////////////////////////////////////////////////////////

    // GPS State
    IUFillLight(&GPSStateL[GPS_ON], "GPS_ON", "Powered", IPS_OK);
    IUFillLight(&GPSStateL[GPS_SEARCHING], "GPS_SEARCHING", "Searching", IPS_IDLE);
    IUFillLight(&GPSStateL[GPS_LOCKING], "GPS_LOCKING", "Locking", IPS_IDLE);
    IUFillLight(&GPSStateL[GPS_LOCKED], "GPS_LOCKED", "Locked", IPS_IDLE);
    IUFillLightVector(&GPSStateLP, GPSStateL, 4, getDeviceName(), "GPS_STATE", "GPS", GPS_DATA_TAB, IPS_IDLE);

    // RAW Data Header
    IUFillText(&GPSDataHeaderT[GPS_DATA_SEQ_NUMBER], "GPS_DATA_SEQ_NUMBER", "Seq #", "NA");
    IUFillText(&GPSDataHeaderT[GPS_DATA_WIDTH], "GPS_DATA_WIDTH", "Width", "NA");
    IUFillText(&GPSDataHeaderT[GPS_DATA_HEIGHT], "GPS_DATA_HEIGHT", "Height", "NA");
    IUFillText(&GPSDataHeaderT[GPS_DATA_LONGITUDE], "GPS_DATA_LONGITUDE", "Longitude", "NA");
    IUFillText(&GPSDataHeaderT[GPS_DATA_LATITUDE], "GPS_DATA_LATITUDE", "Latitude", "NA");
    IUFillText(&GPSDataHeaderT[GPS_DATA_MAX_CLOCK], "GPS_DATA_MAX_CLOCK", "Max Clock", "NA");
    IUFillTextVector(&GPSDataHeaderTP, GPSDataHeaderT, 6, getDeviceName(), "GPS_DATA_HEADER", "Header", GPS_DATA_TAB, IP_RO, 60,
                     IPS_IDLE);

    // RAW Data Start
    IUFillText(&GPSDataStartT[GPS_DATA_START_FLAG], "GPS_DATA_START_FLAG", "Flag", "NA");
    IUFillText(&GPSDataStartT[GPS_DATA_START_SEC], "GPS_DATA_START_SEC", "Seconds", "NA");
    IUFillText(&GPSDataStartT[GPS_DATA_START_USEC], "GPS_DATA_START_USEC", "Microseconds", "NA");
    IUFillText(&GPSDataStartT[GPS_DATA_START_TS], "GPS_DATA_START_TS", "TS", "NA");
    IUFillTextVector(&GPSDataStartTP, GPSDataStartT, 4, getDeviceName(), "GPS_DATA_START", "Start", GPS_DATA_TAB, IP_RO, 60,
                     IPS_IDLE);

    // RAW Data End
    IUFillText(&GPSDataEndT[GPS_DATA_END_FLAG], "GPS_DATA_END_FLAG", "Flag", "NA");
    IUFillText(&GPSDataEndT[GPS_DATA_END_SEC], "GPS_DATA_END_SEC", "Seconds", "NA");
    IUFillText(&GPSDataEndT[GPS_DATA_END_USEC], "GPS_DATA_END_USEC", "Microseconds", "NA");
    IUFillText(&GPSDataEndT[GPS_DATA_END_TS], "GPS_DATA_END_TS", "TS", "NA");
    IUFillTextVector(&GPSDataEndTP, GPSDataEndT, 4, getDeviceName(), "GPS_DATA_END", "End", GPS_DATA_TAB, IP_RO, 60,
                     IPS_IDLE);

    // RAW Data Now
    IUFillText(&GPSDataNowT[GPS_DATA_NOW_FLAG], "GPS_DATA_NOW_FLAG", "Flag", "NA");
    IUFillText(&GPSDataNowT[GPS_DATA_NOW_SEC], "GPS_DATA_NOW_SEC", "Seconds", "NA");
    IUFillText(&GPSDataNowT[GPS_DATA_NOW_USEC], "GPS_DATA_NOW_USEC", "Microseconds", "NA");
    IUFillText(&GPSDataNowT[GPS_DATA_NOW_TS], "GPS_DATA_NOW_TS", "TS", "NA");
    IUFillTextVector(&GPSDataNowTP, GPSDataNowT, 4, getDeviceName(), "GPS_DATA_NOW", "Now", GPS_DATA_TAB, IP_RO, 60, IPS_IDLE);

    addAuxControls();
    setDriverInterface(getDriverInterface());

    return true;
}

void QHYCCD::ISGetProperties(const char *dev)
{
    INDI::CCD::ISGetProperties(dev);

    if (isConnected())
    {
        if (HasCooler())
        {
            defineProperty(&CoolerSP);

            if (HasCoolerManualMode)
            {
                defineProperty(&CoolerModeSP);
            }

            defineProperty(&CoolerNP);
        }

        if (HasHumidity)
            defineProperty(&HumidityNP);

        if (HasUSBSpeed)
            defineProperty(&SpeedNP);

        if (HasReadMode)
            defineProperty(&ReadModeNP);

        if (HasGain)
            defineProperty(&GainNP);

        if (HasOffset)
            defineProperty(&OffsetNP);

        if (HasFilters)
        {
            //Define the Filter Slot and name properties
            defineProperty(&FilterSlotNP);
            if (FilterNameT != nullptr)
                defineProperty(FilterNameTP);
        }

        if (HasUSBTraffic)
            defineProperty(&USBTrafficNP);

        defineProperty(&USBBufferNP);

        defineProperty(&SDKVersionTP);

        if (HasAmpGlow)
            defineProperty(&AMPGlowSP);

        if (HasGPS)
        {
            defineProperty(&GPSSlavingSP);
            defineProperty(&GPSSlavingParamNP);

            defineProperty(&VCOXFreqNP);
            defineProperty(&GPSLEDCalibrationSP);
            defineProperty(&GPSLEDStartPosNP);
            defineProperty(&GPSLEDEndPosNP);

            defineProperty(&GPSControlSP);

            defineProperty(&GPSStateLP);
            defineProperty(&GPSDataHeaderTP);
            defineProperty(&GPSDataStartTP);
            defineProperty(&GPSDataEndTP);
            defineProperty(&GPSDataNowTP);
        }

        //NEW CODE - Add support for overscan/calibration area
        if(HasOverscanArea)
            defineProperty(&OverscanAreaSP);
    }
}

bool QHYCCD::updateProperties()
{
    // Set format first if connected.
    if (isConnected())
    {
        // N.B. AFAIK, there is no way to switch image formats.
        CaptureFormat format;
        if (GetCCDCapability() & CCD_HAS_BAYER)
        {
            format = {"INDI_RAW", "RAW", 16, true};
        }
        else
        {
            format = {"INDI_MONO", "Mono", 16, true};
        }
        addCaptureFormat(format);
    }

    // Define parent class properties
    INDI::CCD::updateProperties();

    if (isConnected())
    {
        if (HasCooler())
        {
            defineProperty(&CoolerSP);
            if (HasCoolerManualMode)
            {
                defineProperty(&CoolerModeSP);
            }

            CoolerNP.p = HasCoolerManualMode ? IP_RW : IP_RO;
            defineProperty(&CoolerNP);

            m_TemperatureTimerID = IEAddTimer(getCurrentPollingPeriod(), QHYCCD::updateTemperatureHelper, this);
        }

        if (HasHumidity)
        {
            if (isSimulation())
            {
                HumidityN[0].value = 99.9;
            }
            else
            {
                double humidity = 0.;
                uint32_t ret = GetQHYCCDHumidity(m_CameraHandle, &humidity);
                if (ret == QHYCCD_SUCCESS)
                {
                    HumidityN[0].value = humidity;
                }

                LOGF_INFO("Humidity Sensor: %s", ret == QHYCCD_SUCCESS ? "true" :
                          "false");
            }

            defineProperty(&HumidityNP);
        }
        double min = 0, max = 0, step = 0;
        if (HasUSBSpeed)
        {
            if (isSimulation())
            {
                SpeedN[0].min   = 1;
                SpeedN[0].max   = 5;
                SpeedN[0].step  = 1;
                SpeedN[0].value = 1;
            }
            else
            {
                int ret = GetQHYCCDParamMinMaxStep(m_CameraHandle, CONTROL_SPEED, &min, &max, &step);
                if (ret == QHYCCD_SUCCESS)
                {
                    SpeedN[0].min  = min;
                    SpeedN[0].max  = max;
                    SpeedN[0].step = step;
                }

                SpeedN[0].value = GetQHYCCDParam(m_CameraHandle, CONTROL_SPEED);

                LOGF_INFO("USB Speed Settings: Value: %.f Min: %.f Max: %.f Step %.f", SpeedN[0].value,
                          SpeedN[0].min, SpeedN[0].max, SpeedN[0].step);
            }

            defineProperty(&SpeedNP);
        }

        // Read mode support
        if (HasReadMode)
        {
            if (isSimulation())
            {
                ReadModeN[0].min   = 0;
                ReadModeN[0].max   = 2;
                ReadModeN[0].step  = 1;
                ReadModeN[0].value = 1;
            }
            else
            {
                ReadModeN[0].min  = 0;
                ReadModeN[0].max = (numReadModes > 0 ? numReadModes - 1 : 0);
                ReadModeN[0].step = 1;
                ReadModeN[0].value = currentQHYReadMode;
            }
            defineProperty(&ReadModeNP);
        }

        if (HasGain)
        {
            if (isSimulation())
            {
                GainN[0].min   = 0;
                GainN[0].max   = 100;
                GainN[0].step  = 10;
                GainN[0].value = 50;
            }
            else
            {
                int ret = GetQHYCCDParamMinMaxStep(m_CameraHandle, CONTROL_GAIN, &min, &max, &step);
                if (ret == QHYCCD_SUCCESS)
                {
                    GainN[0].min  = min;
                    GainN[0].max  = max;
                    GainN[0].step = step;
                }
                GainN[0].value = GetQHYCCDParam(m_CameraHandle, CONTROL_GAIN);

                LOGF_INFO("Gain Settings: Value: %.f Min: %.f Max: %.f Step %.f", GainN[0].value, GainN[0].min,
                          GainN[0].max, GainN[0].step);
            }

            defineProperty(&GainNP);
        }

        if (HasOffset)
        {
            if (isSimulation())
            {
                OffsetN[0].min   = 1;
                OffsetN[0].max   = 10;
                OffsetN[0].step  = 1;
                OffsetN[0].value = 1;
            }
            else
            {
                int ret = GetQHYCCDParamMinMaxStep(m_CameraHandle, CONTROL_OFFSET, &min, &max, &step);
                if (ret == QHYCCD_SUCCESS)
                {
                    OffsetN[0].min  = min;
                    OffsetN[0].max  = max;
                    OffsetN[0].step = step;
                }
                OffsetN[0].value = GetQHYCCDParam(m_CameraHandle, CONTROL_OFFSET);

                LOGF_INFO("Offset Settings: Value: %.f Min: %.f Max: %.f Step %.f", OffsetN[0].value,
                          OffsetN[0].min, OffsetN[0].max, OffsetN[0].step);
            }

            //Define the Offset
            defineProperty(&OffsetNP);
        }

        if (HasFilters)
        {
            INDI::FilterInterface::updateProperties();
        }

        if (HasUSBTraffic)
        {
            if (isSimulation())
            {
                USBTrafficN[0].min   = 1;
                USBTrafficN[0].max   = 100;
                USBTrafficN[0].step  = 5;
                USBTrafficN[0].value = 20;
            }
            else
            {
                int ret = GetQHYCCDParamMinMaxStep(m_CameraHandle, CONTROL_USBTRAFFIC, &min, &max, &step);
                if (ret == QHYCCD_SUCCESS)
                {
                    USBTrafficN[0].min  = min;
                    USBTrafficN[0].max  = max;
                    USBTrafficN[0].step = (max - min) / 20.0;
                }
                USBTrafficN[0].value = GetQHYCCDParam(m_CameraHandle, CONTROL_USBTRAFFIC);

                LOGF_INFO("USB Traffic Settings: Value: %.f Min: %.f Max: %.f Step %.f", USBTrafficN[0].value,
                          USBTrafficN[0].min, USBTrafficN[0].max, USBTrafficN[0].step);
            }
            defineProperty(&USBTrafficNP);
        }

        defineProperty(&USBBufferNP);

        defineProperty(&SDKVersionTP);

        if (HasAmpGlow)
        {
            int index = GetQHYCCDParam(m_CameraHandle, CONTROL_AMPV);
            IUResetSwitch(&AMPGlowSP);
            AMPGlowS[index].s = ISS_ON;
            defineProperty(&AMPGlowSP);
        }

        if (HasGPS)
        {
            defineProperty(&GPSSlavingSP);
            defineProperty(&GPSSlavingParamNP);
            defineProperty(&VCOXFreqNP);
            defineProperty(&GPSLEDCalibrationSP);
            defineProperty(&GPSLEDStartPosNP);
            defineProperty(&GPSLEDEndPosNP);
            defineProperty(&GPSControlSP);

            defineProperty(&GPSStateLP);
            defineProperty(&GPSDataHeaderTP);
            defineProperty(&GPSDataStartTP);
            defineProperty(&GPSDataEndTP);
            defineProperty(&GPSDataNowTP);
        }

        //NEW CODE - Add support for overscan/calibration area
        if (HasOverscanArea)
            defineProperty(&OverscanAreaSP);

        // Let's get parameters now from CCD
        setupParams();
    }
    else
    {
        if (HasCooler())
        {
            deleteProperty(CoolerSP.name);

            if (HasCoolerManualMode)
                deleteProperty(CoolerModeSP.name);

            deleteProperty(CoolerNP.name);

            RemoveTimer(m_TemperatureTimerID);
        }

        if (HasHumidity)
            deleteProperty(HumidityNP.name);

        if (HasUSBSpeed)
        {
            deleteProperty(SpeedNP.name);
        }

        if (HasReadMode)
        {
            deleteProperty(ReadModeNP.name);
        }

        if (HasGain)
        {
            deleteProperty(GainNP.name);
        }

        if (HasOffset)
        {
            deleteProperty(OffsetNP.name);
        }

        if (HasFilters)
        {
            INDI::FilterInterface::updateProperties();
        }

        if (HasUSBTraffic)
            deleteProperty(USBTrafficNP.name);

        deleteProperty(USBBufferNP.name);

        deleteProperty(SDKVersionTP.name);

        if (HasAmpGlow)
            deleteProperty(AMPGlowSP.name);

        if (HasGPS)
        {
            deleteProperty(GPSSlavingSP.name);
            deleteProperty(GPSSlavingParamNP.name);
            deleteProperty(VCOXFreqNP.name);
            deleteProperty(GPSLEDCalibrationSP.name);
            deleteProperty(GPSLEDStartPosNP.name);
            deleteProperty(GPSLEDEndPosNP.name);
            deleteProperty(GPSControlSP.name);

            deleteProperty(GPSStateLP.name);
            deleteProperty(GPSDataHeaderTP.name);
            deleteProperty(GPSDataStartTP.name);
            deleteProperty(GPSDataEndTP.name);
            deleteProperty(GPSDataNowTP.name);
        }

        //NEW CODE - Add support for overscan/calibration area
        if (HasOverscanArea)
            deleteProperty(OverscanAreaSP.name);
    }

    return true;
}

bool QHYCCD::Connect()
{
    uint32_t cap;


    if (isSimulation())
    {
        cap = CCD_CAN_SUBFRAME | CCD_CAN_ABORT | CCD_CAN_BIN | CCD_HAS_COOLER | CCD_HAS_ST4_PORT;
        SetCCDCapability(cap);

        HasUSBTraffic = true;
        HasUSBSpeed   = true;
        HasGain       = true;
        HasOffset     = true;
        HasFilters    = true;
        HasReadMode   = true;

        return true;
    }

    // Query the current CCD cameras. This method makes the driver more robust and
    // it fixes a crash with the new QHC SDK when the INDI driver reopens the same camera
    // with OpenQHYCCD() without a ScanQHYCCD() call before.
    // 2017-12-15 JM: No this method ReleaseQHYCCDResource which ends up clearing handles for multiple
    // cameras
    /*std::vector<std::string> devices = GetDevicesIDs();

    // The CCD device is not available anymore
    if (std::find(devices.begin(), devices.end(), std::string(camid)) == devices.end())
    {
        LOGF_ERROR("Error: Camera %s is not connected", camid);
        return false;
    }*/
    m_CameraHandle = OpenQHYCCD(m_CamID);

    if (m_CameraHandle != nullptr)
    {
        LOGF_INFO("Connected to %s.", m_CamID);

        cap = CCD_CAN_ABORT | CCD_CAN_SUBFRAME;

        // Disable the stream mode before connecting
        currentQHYStreamMode = 0;
        uint32_t ret = SetQHYCCDStreamMode(m_CameraHandle, currentQHYStreamMode);
        if (ret != QHYCCD_SUCCESS)
        {
            LOGF_ERROR("Can not disable stream mode (%d)", ret);
        }
        ret = InitQHYCCD(m_CameraHandle);
        if (ret != QHYCCD_SUCCESS)
        {
            LOGF_ERROR("Init Camera failed (%d)", ret);
            return false;
        }

        ////////////////////////////////////////////////////////////////////
        /// SDK Version
        ////////////////////////////////////////////////////////////////////
        uint32_t year, month, day, subday;
        GetQHYCCDSDKVersion(&year, &month, &day, &subday);
        std::ostringstream versionInfo;
        versionInfo << year << "." << month << "." << day;
        LOGF_INFO("Using QHY SDK version %s", versionInfo.str().c_str());
        IUSaveText(&SDKVersionT[0], versionInfo.str().c_str());

        ////////////////////////////////////////////////////////////////////
        /// Bin Modes
        ////////////////////////////////////////////////////////////////////

        m_SupportedBins[Bin1x1] = IsQHYCCDControlAvailable(m_CameraHandle, CAM_BIN1X1MODE) == QHYCCD_SUCCESS;
        m_SupportedBins[Bin2x2] = IsQHYCCDControlAvailable(m_CameraHandle, CAM_BIN2X2MODE) == QHYCCD_SUCCESS;
        m_SupportedBins[Bin3x3] = IsQHYCCDControlAvailable(m_CameraHandle, CAM_BIN3X3MODE) == QHYCCD_SUCCESS;
        m_SupportedBins[Bin4x4] = IsQHYCCDControlAvailable(m_CameraHandle, CAM_BIN4X4MODE) == QHYCCD_SUCCESS;

        auto supported = std::any_of(m_SupportedBins + 1, m_SupportedBins + 4, [](bool value)
        {
            return value;
        });

        if (supported)
            cap |= CCD_CAN_BIN;

        LOGF_INFO("Binning Control: %s", supported ? "True" : "False");
        if (supported)
        {
            LOGF_DEBUG("Bin2x2: %s, Bin3x3: %s, Bin4x4: %s", m_SupportedBins[Bin2x2] ? "true" : "false",
                       m_SupportedBins[Bin3x3] ? "true" : "false",
                       m_SupportedBins[Bin4x4] ? "true" : "false");
        }

        ////////////////////////////////////////////////////////////////////
        /// Read Modes
        ////////////////////////////////////////////////////////////////////
        ret = GetQHYCCDNumberOfReadModes(m_CameraHandle, &numReadModes);
        if (ret == QHYCCD_SUCCESS && numReadModes > 1)
        {
            HasReadMode = true;
            //NEW CODE - format modifier %zu --> %u
            LOGF_INFO("Number of read modes: %u", numReadModes);
        }

        readModeInfo = new QHYReadModeInfo[numReadModes];

        for (uint32_t rm = 0; rm < numReadModes; rm++)
        {
            readModeInfo[rm].id = rm;
            ret = GetQHYCCDReadModeName(m_CameraHandle, readModeInfo[rm].id, &readModeInfo[rm].label[0]);
            if (ret == QHYCCD_SUCCESS)
            {
                LOGF_INFO("Mode %d: %s", readModeInfo[rm].id, readModeInfo[rm].label);
            }
            else
            {
                LOGF_INFO("Failed to obtain read mode name for modeNumber: %d", readModeInfo[rm].id);
                strcpy(readModeInfo[rm].label, "UNKNOWN");
            }
            ret = GetQHYCCDReadModeResolution(m_CameraHandle, readModeInfo[rm].id, &readModeInfo[rm].subW,
                                              &readModeInfo[rm].subH);
            if (ret == QHYCCD_SUCCESS)
            {
                LOGF_INFO("Sensor resolution for mode %s: %dx%d px", readModeInfo[rm].label,
                          readModeInfo[rm].subW, readModeInfo[rm].subH);
            }
            else
            {
                LOGF_WARN("Failed to read mode resolution name for modeNumber: %d", readModeInfo[rm].id);
                readModeInfo[rm].subW = readModeInfo[rm].subH = 0;
            }
        }

        //Correctly initialize current read mode
        ret = GetQHYCCDReadMode(m_CameraHandle, &currentQHYReadMode);
        if (ret == QHYCCD_SUCCESS && numReadModes > 1)
        {
            LOGF_INFO("Current read mode: %s (%dx%d)", readModeInfo[currentQHYReadMode].label,
                      readModeInfo[currentQHYReadMode].subW, readModeInfo[currentQHYReadMode].subH);
        }

        ////////////////////////////////////////////////////////////////////
        /// Shutter Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CAM_MECHANICALSHUTTER);
        if (ret == QHYCCD_SUCCESS)
        {
            cap |= CCD_HAS_SHUTTER;
        }

        LOGF_DEBUG("Shutter Control: %s", (cap & CCD_HAS_SHUTTER) ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Streaming Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CAM_LIVEVIDEOMODE);
        if (ret == QHYCCD_SUCCESS)
        {
            cap |= CCD_HAS_STREAMING;
        }

        LOGF_DEBUG("Has Streaming: %s", (cap & CCD_HAS_STREAMING) ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// AutoMode Cooler Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_COOLER);
        if (ret == QHYCCD_SUCCESS)
        {
            HasCoolerAutoMode = true;
            cap |= CCD_HAS_COOLER;
        }
        LOGF_DEBUG("Automatic Cooler Control: %s", (cap & CCD_HAS_COOLER) ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Manual PWM Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_MANULPWM);
        if (ret == QHYCCD_SUCCESS)
        {
            HasCoolerManualMode = true;
        }
        LOGF_DEBUG("Manual Cooler Control: %s", HasCoolerManualMode ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// ST4 Port Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_ST4PORT);
        if (ret == QHYCCD_SUCCESS)
        {
            cap |= CCD_HAS_ST4_PORT;
        }

        LOGF_DEBUG("Guider Port Control: %s", (cap & CCD_HAS_ST4_PORT) ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Camera Speed Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_SPEED);
        if (ret == QHYCCD_SUCCESS)
        {
            HasUSBSpeed = true;

            // Force a certain speed on initialization of QHY5PII-C:
            // CONTROL_SPEED = 2 - Fastest, but the camera gets stuck with long exposure times.
            // CONTROL_SPEED = 1 - This is safe with the current driver.
            // CONTROL_SPEED = 0 - This is safe, but slower than 1.
            if (isQHY5PIIC())
                SetQHYCCDParam(m_CameraHandle, CONTROL_SPEED, 1);
        }

        LOGF_DEBUG("USB Speed Control: %s", HasUSBSpeed ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Gain Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_GAIN);
        if (ret == QHYCCD_SUCCESS)
        {
            HasGain = true;
        }

        LOGF_DEBUG("Gain Control: %s", HasGain ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Offset Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_OFFSET);
        if (ret == QHYCCD_SUCCESS)
        {
            HasOffset = true;
        }

        LOGF_DEBUG("Offset Control: %s", HasOffset ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Filter Wheel Support
        ////////////////////////////////////////////////////////////////////

        HasFilters = false;
        //Using new SDK query
        // N.B. JM 2022.09.18: Still must retry multiple times as sometimes the filter is not picked up
        for (int i = 0; i < 3; i++)
        {
            ret = IsQHYCCDCFWPlugged(m_CameraHandle);
            if (ret == QHYCCD_SUCCESS)
            {
                HasFilters = true;
                m_MaxFilterCount = GetQHYCCDParam(m_CameraHandle, CONTROL_CFWSLOTSNUM);
                LOGF_DEBUG("Filter Count (CONTROL_CFWSLOTSNUM): %d", m_MaxFilterCount);
                // If we get invalid value, check again in 0.5 sec
                if (m_MaxFilterCount > 16)
                {
                    usleep(500000);
                    m_MaxFilterCount = GetQHYCCDParam(m_CameraHandle, CONTROL_CFWSLOTSNUM);
                    LOGF_DEBUG("Filter Count (CONTROL_CFWSLOTSNUM): %d", m_MaxFilterCount);
                }

                if (m_MaxFilterCount > 16)
                {
                    LOG_DEBUG("Camera can support CFW but no filters are present.");
                    m_MaxFilterCount = -1;
                    HasFilters = false;
                }

                if (m_MaxFilterCount > 0)
                {
                    HasFilters = true;
                    char currentPos[MAXINDINAME] = {0};
                    if (GetQHYCCDCFWStatus(m_CameraHandle, currentPos) == QHYCCD_SUCCESS)
                    {
                        CurrentFilter = strtol(currentPos, nullptr, 16) + 1;
                        FilterSlotN[0].value = CurrentFilter;
                    }

                    updateFilterProperties();
                    LOGF_INFO("Filter Count (CONTROL_CFWSLOTSNUM): %d", m_MaxFilterCount);
                }
                else
                {
                    HasFilters = false;
                }

                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        if (HasFilters == true)
        {
            setDriverInterface(getDriverInterface() | FILTER_INTERFACE);
            syncDriverInfo();
        }
        LOGF_DEBUG("Has Filters: %s", HasFilters ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// 8bit Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_TRANSFERBIT);
        HasTransferBit = (ret == QHYCCD_SUCCESS);
        LOGF_DEBUG("Has Transfer Bit control? %s", HasTransferBit ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// USB Traffic Control Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_USBTRAFFIC);
        if (ret == QHYCCD_SUCCESS)
        {
            HasUSBTraffic = true;
            // Force the USB traffic value to 30 on initialization of QHY5PII-C otherwise
            // the camera has poor transfer speed.
            if (isQHY5PIIC())
                SetQHYCCDParam(m_CameraHandle, CONTROL_USBTRAFFIC, 30);
        }

        LOGF_DEBUG("USB Traffic Control: %s", HasUSBTraffic ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Color Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CAM_COLOR);
        //if(ret != QHYCCD_ERROR && ret != QHYCCD_ERROR_NOTSUPPORT)
        if (ret != QHYCCD_ERROR)
        {
            if (ret == BAYER_GB)
            {
                IUSaveText(&BayerT[2], "GBRG");
                cap |= CCD_HAS_BAYER;
            }
            else if (ret == BAYER_GR)
            {
                IUSaveText(&BayerT[2], "GRBG");
                cap |= CCD_HAS_BAYER;
            }
            else if (ret == BAYER_BG)
            {
                IUSaveText(&BayerT[2], "BGGR");
                cap |= CCD_HAS_BAYER;
            }
            else if (ret == BAYER_RG)
            {
                IUSaveText(&BayerT[2], "RGGB");
                cap |= CCD_HAS_BAYER;
            }

            LOGF_DEBUG("Color camera: %s", BayerT[2].text);
        }

        ////////////////////////////////////////////////////////////////////
        /// Exposure Limits
        ////////////////////////////////////////////////////////////////////
        double min = 0, max = 0, step = 0;
        // Exposure limits in microseconds
        ret = GetQHYCCDParamMinMaxStep(m_CameraHandle, CONTROL_EXPOSURE, &min, &max, &step);
        if (ret == QHYCCD_SUCCESS)
            PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", min / 1e6, max / 1e6, step / 1e6, false);
        else
            PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", 0.001, 3600, 1, false);

        LOGF_INFO("Camera exposure limits: Min: %.6fs Max: %.fs Step %.fs", min / 1e6, max / 1e6, step / 1e6);

        ////////////////////////////////////////////////////////////////////
        /// Amp glow Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CONTROL_AMPV);
        if (ret == QHYCCD_SUCCESS)
        {
            HasAmpGlow = true;
        }

        LOGF_DEBUG("Ampglow Control: %s", HasAmpGlow ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// GPS Support
        ////////////////////////////////////////////////////////////////////
        ret = IsQHYCCDControlAvailable(m_CameraHandle, CAM_GPS);
        // JM 2021.07.25: CAM_GPS is returned as true even when there is no GPS.
        // This bug was reported to QHY and awaiting a fix. Currently limiting GSP to QHY174 only.
        if (ret == QHYCCD_SUCCESS && strstr(m_CamID, "174"))
        {
            HasGPS = true;
        }

        LOGF_DEBUG("GPS Support: %s", HasGPS ? "True" : "False");

        ////////////////////////////////////////////////////////////////////
        /// Humidity Support
        ////////////////////////////////////////////////////////////////////
        double humidity = 0;
        ret = GetQHYCCDHumidity(m_CameraHandle, &humidity);
        if (ret == QHYCCD_SUCCESS)
        {
            HasHumidity = true;
        }

        LOGF_INFO("Humidity Support: %s", HasHumidity ? "True" : "False");
        ////////////////////////////////////////////////////////////////////
        /// Overscan Area Support
        ////////////////////////////////////////////////////////////////////
        //NEW CODE - Add support for overscan/calibration area
        uint32_t overscanSubX, overscanSubY, overscanSubW, overscanSubH;
        ret = GetQHYCCDOverScanArea(m_CameraHandle, &overscanSubX, &overscanSubY, &overscanSubW, &overscanSubH);
        if (ret == QHYCCD_SUCCESS)
        {
            HasOverscanArea = (overscanSubW + overscanSubH > 0);
        }

        LOGF_DEBUG("Overscan Area Support: %s", HasOverscanArea ? "True" : "False");

        // Set Camera Capability
        SetCCDCapability(cap);

        ////////////////////////////////////////////////////////////////////
        /// Start Threads
        ////////////////////////////////////////////////////////////////////
        m_ThreadRequest = StateIdle;
        m_ThreadState = StateNone;
        int stat = pthread_create(&m_ImagingThread, nullptr, &imagingHelper, this);
        if (stat != 0)
        {
            LOGF_ERROR("Error creating imaging thread (%d)", stat);
            return false;
        }
        pthread_mutex_lock(&condMutex);
        while (m_ThreadState == StateNone)
        {
            pthread_cond_wait(&cv, &condMutex);
        }
        pthread_mutex_unlock(&condMutex);

        SetTimer(getCurrentPollingPeriod());

        return true;
    }

    LOGF_ERROR("Connecting to camera failed (%s).", m_CamID);

    return false;
}

bool QHYCCD::Disconnect()
{
    ImageState  tState;
    LOGF_DEBUG("Closing %s...", m_Name);

    pthread_mutex_lock(&condMutex);
    tState = m_ThreadState;
    m_ThreadRequest = StateTerminate;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);
    pthread_join(m_ImagingThread, nullptr);
    //tState = StateNone;
    if (isSimulation() == false)
    {
        if (tState == StateStream)
        {
            StopQHYCCDLive(m_CameraHandle);
            SetQHYCCDStreamMode(m_CameraHandle, 0x0);
        }
        else if (tState == StateExposure)
            CancelQHYCCDExposingAndReadout(m_CameraHandle);
        CloseQHYCCD(m_CameraHandle);
    }

    tState = StateNone;

    LOG_INFO("Camera is offline.");

    return true;
}

bool QHYCCD::setupParams()
{

    //NEW CODE - Add support for overscan/calibration area, use sensorROI & effectiveROI as containers for frame width/offest
    uint32_t nbuf, bpp;
    double chipw, chiph, pixelw, pixelh;

    //NEW CODE - Add support for overscan/calibration area, use sensorROI & effectiveROI as containers for frame width/offest
    //raw frame origin is always at (0,0)
    sensorROI.subX = sensorROI.subY = 0;

    if (isSimulation())
    {
        //NEW CODE - Add support for overscan/calibration area, use sensorROI & effectiveROI as containers for frame width/offest
        sensorROI.subW  = 1280;
        sensorROI.subH = 1024;
        pixelh = pixelw = 5.4;
        bpp             = 8;
    }
    else
    {

        int rc = GetQHYCCDChipInfo(m_CameraHandle, &chipw, &chiph, &sensorROI.subW, &sensorROI.subH, &pixelw, &pixelh, &bpp);
        /* JM: We need GetQHYCCDErrorString(ret) to get the string description of the error, please implement this in the SDK */
        if (rc != QHYCCD_SUCCESS)
        {
            LOGF_ERROR("Error: GetQHYCCDChipInfo() (%d)", rc);
            return false;
        }

        LOGF_DEBUG("GetQHYCCDChipInfo: chipW :%g chipH: %g imageW: %d imageH: %d pixelW: %g pixelH: %g bbp %d", chipw,
                   chiph, sensorROI.subW, sensorROI.subH, pixelw, pixelh, bpp);

        rc = GetQHYCCDEffectiveArea(m_CameraHandle, &effectiveROI.subX, &effectiveROI.subY, &effectiveROI.subW, &effectiveROI.subH);
        if (rc == QHYCCD_SUCCESS)
        {
            LOGF_DEBUG("GetQHYCCDEffectiveArea: subX :%d subY: %d subW: %d subH: %d", effectiveROI.subX, effectiveROI.subY,
                       effectiveROI.subW, effectiveROI.subH);
        }
        // JM 2021-04-07: If effective ROI fails, we shouldn't IgnoreOverscanArea
        else
        {
            LOG_DEBUG("Querying effective area failed. Setting IgnoreOverscanArea to false and resorting to sensor ROI.");
            IgnoreOverscanArea = false;
        }

        //NEW CODE - Add support for overscan/calibration area, we want to allow also pixels outside of effectiveROI
        //if (effectiveROI.subX > 0 || effectiveROI.subY > 0)
        //{
        //    imagew = effectiveROI.subW;
        //    imageh = effectiveROI.subH;
        //}

        //NEW CODE - Overscan Area not needed here, deprecated variable overscanROI
        //rc = GetQHYCCDOverScanArea(m_CameraHandle, &overscanROI.subX, &overscanROI.subY, &overscanROI.subW, &overscanROI.subH);
        //if (rc == QHYCCD_SUCCESS)
        //{
        //    LOGF_DEBUG("GetQHYCCDOverscanArea: subX :%d subY: %d subW: %d subH: %d", overscanROI.subX, overscanROI.subY,
        //               overscanROI.subW, overscanROI.subH);
        //}
    }

    //NEW CODE - Add support for overscan/calibration area
    //Overscan area is ignored, exposure frame to be within effectiveROI
    if (IgnoreOverscanArea)
        SetCCDParams(effectiveROI.subW, effectiveROI.subH, bpp, pixelw, pixelh);
    else
        //Overscan area is not ignored, exposure frame to be within full sensor frame
        SetCCDParams(sensorROI.subW, sensorROI.subH, bpp, pixelw, pixelh);

    nbuf = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * PrimaryCCD.getBPP() / 8;
    PrimaryCCD.setFrameBufferSize(nbuf);

    if (HasStreaming())
    {
        Streamer->setPixelFormat(INDI_MONO);
        //NEW CODE - Add support for overscan/calibration area
        //TODO - should overscan area be allowd in streaming mode, too?
        //Streamer->setSize(IgnoreOverscanArea ? effectiveROI.subW : sensorROI.subW, IgnoreOverscanArea ? effectiveROI.subH : sensorROI.subH);
        Streamer->setSize(effectiveROI.subW, effectiveROI.subH);
    }

    return true;
}

int QHYCCD::SetTemperature(double temperature)
{
    // If there difference, for example, is less than 0.1 degrees, let's immediately return OK.
    if (fabs(temperature - TemperatureN[0].value) < UPDATE_THRESHOLD)
        return 1;

    LOGF_DEBUG("Requested temperature is %.f, current temperature is %.f", temperature, TemperatureN[0].value);

    m_TemperatureRequest = temperature;
    m_PWMRequest = -1;

    SetQHYCCDParam(m_CameraHandle, CONTROL_COOLER, m_TemperatureRequest);

    setCoolerEnabled(m_TemperatureRequest <= TemperatureN[0].value);
    setCoolerMode(COOLER_AUTOMATIC);
    return 0;
}

bool QHYCCD::StartExposure(float duration)
{
    unsigned int ret = QHYCCD_ERROR;

    //NEW CODE - Add support for overscan/calibration area, add frame offset to be modular based on overscan area settings
    uint32_t subX = (PrimaryCCD.getSubX() + (IgnoreOverscanArea ? effectiveROI.subX : 0)) / PrimaryCCD.getBinX();
    uint32_t subY = (PrimaryCCD.getSubY() + (IgnoreOverscanArea ? effectiveROI.subY : 0)) / PrimaryCCD.getBinY();
    uint32_t subW = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
    uint32_t subH = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

    if (HasStreaming() && Streamer->isBusy())
    {
        LOG_ERROR("Cannot take exposure while streaming/recording is active.");
        return false;
    }

    // Set streaming mode and re-initialize camera
    if (currentQHYStreamMode == 1 && !isSimulation())
    {

        /* NR - Closing the camera will reset the connection in ECOS, I recommend to omit here. */
        //CloseQHYCCD(m_CameraHandle);
        //ret = InitQHYCCD(m_CameraHandle);
        //if(ret != QHYCCD_SUCCESS)
        //{
        //    LOGF_INFO("Init QHYCCD for streaming mode failed, code:%d", ret);
        //    return false;
        //}

        currentQHYStreamMode = 0;
        SetQHYCCDStreamMode(m_CameraHandle, currentQHYStreamMode);

        ret = InitQHYCCD(m_CameraHandle);
        if(ret != QHYCCD_SUCCESS)
        {
            LOGF_INFO("Init QHYCCD for streaming mode failed, code:%d", ret);
            return false;
        }

        // Try to set 16bit mode if supported back.
        SetQHYCCDBitsMode(m_CameraHandle, PrimaryCCD.getBPP());
    }

    m_ImageFrameType = PrimaryCCD.getFrameType();

    if (GetCCDCapability() & CCD_HAS_SHUTTER)
    {
        if (m_ImageFrameType == INDI::CCDChip::DARK_FRAME || m_ImageFrameType == INDI::CCDChip::BIAS_FRAME)
            ControlQHYCCDShutter(m_CameraHandle, MACHANICALSHUTTER_CLOSE);
        else
            ControlQHYCCDShutter(m_CameraHandle, MACHANICALSHUTTER_FREE);
    }

    long uSecs = duration * 1000 * 1000;
    LOGF_DEBUG("Requested exposure time is %ld us", uSecs);
    m_ExposureRequest = static_cast<double>(duration);
    PrimaryCCD.setExposureDuration(m_ExposureRequest);

    // Setting Exposure time, IF different from last exposure time.
    if (isSimulation())
        ret = QHYCCD_SUCCESS;
    else
    {
        if (m_LastExposureRequestuS != uSecs)
        {
            ret = SetQHYCCDParam(m_CameraHandle, CONTROL_EXPOSURE, uSecs);

            if (ret != QHYCCD_SUCCESS)
            {
                LOGF_ERROR("Set expose time failed (%d).", ret);
                return false;
            }

            m_LastExposureRequestuS = uSecs;
        }
    }

    // Set binning mode
    if (isSimulation())
        ret = QHYCCD_SUCCESS;
    else
        ret = SetQHYCCDBinMode(m_CameraHandle, PrimaryCCD.getBinX(), PrimaryCCD.getBinY());
    if (ret != QHYCCD_SUCCESS)
    {
        LOGF_INFO("Set QHYCCD Bin mode failed (%d)", ret);
        return false;
    }

    LOGF_DEBUG("SetQHYCCDBinMode (%dx%d).", PrimaryCCD.getBinX(), PrimaryCCD.getBinY());

    // Set Region of Interest (ROI)
    if (isSimulation())
        ret = QHYCCD_SUCCESS;
    else
        ret = SetQHYCCDResolution(m_CameraHandle, subX, subY, subW, subH);

    if (ret != QHYCCD_SUCCESS)
    {
        LOGF_INFO("Set QHYCCD ROI resolution (%d,%d) (%d,%d) failed (%d)", subX, subY, subW, subH, ret);
        return false;
    }

    LOGF_DEBUG("SetQHYCCDResolution x: %d y: %d w: %d h: %d", subX, subY, subW, subH);

    // Start to expose the frame
    if (isSimulation())
        ret = QHYCCD_SUCCESS;
    else
        ret = ExpQHYCCDSingleFrame(m_CameraHandle);
    if (ret == QHYCCD_ERROR)
    {
        LOGF_INFO("Begin QHYCCD expose failed (%d)", ret);
        return false;
    }

    gettimeofday(&ExpStart, nullptr);
    LOGF_DEBUG("Taking a %.5f seconds frame...", m_ExposureRequest);

    InExposure = true;
    pthread_mutex_lock(&condMutex);
    m_ThreadRequest = StateExposure;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    return true;
}

bool QHYCCD::AbortExposure()
{
    if (!InExposure || isSimulation())
    {
        InExposure = false;
        return true;
    }

    LOG_DEBUG("Aborting camera exposure...");

    pthread_mutex_lock(&condMutex);
    m_ThreadRequest = StateAbort;
    pthread_cond_signal(&cv);
    while (m_ThreadState == StateExposure)
    {
        pthread_cond_wait(&cv, &condMutex);
    }
    pthread_mutex_unlock(&condMutex);

    if (std::string(m_CamID) != "QHY5-M-")
    {
        int rc = CancelQHYCCDExposingAndReadout(m_CameraHandle);
        if (rc == QHYCCD_SUCCESS)
        {
            InExposure = false;
            LOG_INFO("Exposure aborted.");
            return true;
        }
        else
            LOGF_ERROR("Abort exposure failed (%d)", rc);

        return false;
    }
    else
    {
        InExposure = false;
        LOG_INFO("Exposure aborted.");
        return true;
    }
}

bool QHYCCD::UpdateCCDFrame(int x, int y, int w, int h)
{
    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w, h);
    // Total bytes required for image buffer
    uint32_t nbuf = (PrimaryCCD.getSubW() * PrimaryCCD.getSubH() * PrimaryCCD.getBPP() / 8);
    PrimaryCCD.setFrameBufferSize(nbuf);

    // Streamer is always updated with BINNED size.
    if (HasStreaming())
        Streamer->setSize(PrimaryCCD.getSubW() / PrimaryCCD.getBinX(), PrimaryCCD.getSubH() / PrimaryCCD.getBinY());
    return true;
}

bool QHYCCD::UpdateCCDBin(int hor, int ver)
{
    if (hor != ver)
    {
        LOG_ERROR("Invalid binning mode. Asymmetrical binning not supported.");
        return false;
    }
    else if (hor > 4 || ver > 4)
    {
        LOG_ERROR("Invalid binning mode. Maximum theoritical binning is 4x4");
        return false;
    }

    auto supported = m_SupportedBins[hor - 1];

    // Binning ALWAYS succeeds
#if 0
    if (ret != QHYCCD_SUCCESS)
    {
        useSoftBin = true;
    }

    // We always use software binning so QHY binning is always at 1x1
    PrimaryCCD.getBinX() = 1;
    PrimaryCCD.getBinY() = 1;
#endif

    if (!supported)
    {
        LOGF_ERROR("%dx%d binning is not supported.", hor, ver);
        return false;
    }

    PrimaryCCD.setBin(hor, ver);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

double QHYCCD::calcTimeLeft()
{
    double timesince;
    double timeleft;
    struct timeval now;
    gettimeofday(&now, nullptr);

    timesince = static_cast<double>(now.tv_sec * 1000.0 + now.tv_usec / 1000) -
                static_cast<double>(ExpStart.tv_sec * 1000.0 + ExpStart.tv_usec / 1000);
    timesince = timesince / 1000;

    timeleft = m_ExposureRequest - timesince;
    return timeleft;
}

/* Downloads the image from the CCD. */
int QHYCCD::grabImage()
{
    std::unique_lock<std::mutex> guard(ccdBufferLock);
    if (isSimulation())
    {
        uint8_t *image = PrimaryCCD.getFrameBuffer();
        int width      = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * PrimaryCCD.getBPP() / 8;
        int height     = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

        for (int i = 0; i < height; i++)
            for (int j = 0; j < width; j++)
                image[i * width + j] = rand() % 255;
    }
    else
    {
        uint32_t ret, w, h, bpp, channels;

        LOG_DEBUG("GetQHYCCDSingleFrame Blocking read call.");
        ret = GetQHYCCDSingleFrame(m_CameraHandle, &w, &h, &bpp, &channels, PrimaryCCD.getFrameBuffer());
        LOG_DEBUG("GetQHYCCDSingleFrame Blocking read call complete.");

        if (ret != QHYCCD_SUCCESS)
        {
            LOGF_ERROR("GetQHYCCDSingleFrame error (%d)", ret);
            PrimaryCCD.setExposureFailed();
            return -1;
        }
    }
    guard.unlock();

    // Perform software binning if necessary
    //if (useSoftBin)
    //    PrimaryCCD.binFrame();

    if (m_ExposureRequest > getCurrentPollingPeriod() * 5)
        LOG_INFO("Download complete.");
    else
        LOG_DEBUG("Download complete.");

    if (HasGPS && GPSControlS[INDI_ENABLED].s == ISS_ON)
        decodeGPSHeader();

    ExposureComplete(&PrimaryCCD);

    return 0;
}

void QHYCCD::TimerHit()
{
    if (isConnected() == false)
        return;

    //    if (HasFilters && m_MaxFilterCount == -1)
    //    {
    //        m_MaxFilterCount = GetQHYCCDParam(m_CameraHandle, CONTROL_CFWSLOTSNUM);
    //        LOGF_DEBUG("Filter Count (CONTROL_CFWSLOTSNUM): %d", m_MaxFilterCount);
    //        if (m_MaxFilterCount > 16)
    //            m_MaxFilterCount = -1;
    //        if (m_MaxFilterCount > 0)
    //        {
    //            if (updateFilterProperties())
    //            {
    //                deleteProperty("FILTER_NAME");
    //                IUUpdateMinMax(&FilterSlotNP);
    //                defineProperty(FilterNameTP);
    //            }
    //        }
    //    }

    if (FilterSlotNP.s == IPS_BUSY)
    {
        char currentPos[MAXINDINAME] = {0};
        int rc = GetQHYCCDCFWStatus(m_CameraHandle, currentPos);
        if (rc == QHYCCD_SUCCESS)
        {
            // QHY filter wheel positions are from '0' to 'F'
            // 0 to 15
            // INDI Filter Wheel 1 to 16
            CurrentFilter = strtol(currentPos, nullptr, 16) + 1;
            LOGF_DEBUG("Filter current position: %d", CurrentFilter);

            if (TargetFilter == CurrentFilter)
            {
                m_FilterCheckCounter = 0;
                SelectFilterDone(TargetFilter);
                LOGF_DEBUG("%s: Filter changed to %d", m_CamID, TargetFilter);
            }
        }
        else if (++m_FilterCheckCounter > 30)
        {
            FilterSlotNP.s = IPS_ALERT;
            LOG_ERROR("Filter change timed out.");
            IDSetNumber(&FilterSlotNP, nullptr);
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

IPState QHYCCD::GuideNorth(uint32_t ms)
{
    ControlQHYCCDGuide(m_CameraHandle, 1, ms);
    return IPS_OK;
}

IPState QHYCCD::GuideSouth(uint32_t ms)
{
    ControlQHYCCDGuide(m_CameraHandle, 2, ms);
    return IPS_OK;
}

IPState QHYCCD::GuideEast(uint32_t ms)
{
    ControlQHYCCDGuide(m_CameraHandle, 0, ms);
    return IPS_OK;
}

IPState QHYCCD::GuideWest(uint32_t ms)
{
    ControlQHYCCDGuide(m_CameraHandle, 3, ms);
    return IPS_OK;
}

bool QHYCCD::SelectFilter(int position)
{
    if (isSimulation())
        return true;

    // QHY Filter position is '0' to 'F'
    // 0 to 15
    // INDI Filters 1 to 16
    char targetPos[8] = {0};
    snprintf(targetPos, 8, "%X", position - 1);
    return (SendOrder2QHYCCDCFW(m_CameraHandle, targetPos, 1) == QHYCCD_SUCCESS);
}

int QHYCCD::QueryFilter()
{
    return CurrentFilter;
}

bool QHYCCD::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        //////////////////////////////////////////////////////////////////////
        /// Cooler On/Off Control
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, CoolerSP.name))
        {
            if (IUUpdateSwitch(&CoolerSP, states, names, n) < 0)
            {
                CoolerSP.s = IPS_ALERT;
                IDSetSwitch(&CoolerSP, nullptr);
                return true;
            }

            bool enabled = (CoolerS[COOLER_ON].s == ISS_ON);

            // If explicitly enabled, we always set temperature to 0
            if (enabled)
            {
                if (HasCoolerAutoMode)
                {
                    double targetTemperature = TemperatureN[0].value;
                    if (targetTemperature > 0)
                        targetTemperature = 0;
                    if (SetTemperature(targetTemperature) == 0)
                    {
                        TemperatureNP.s = IPS_BUSY;
                        IDSetNumber(&TemperatureNP, nullptr);
                    }
                    return true;
                }
                else
                {
                    IUResetSwitch(&CoolerSP);
                    CoolerS[COOLER_OFF].s = ISS_ON;
                    CoolerSP.s = IPS_ALERT;
                    LOG_ERROR("Cannot turn on cooler in manual mode. Set cooler power to activate it.");
                    IDSetSwitch(&CoolerSP, nullptr);
                    return true;
                }
            }
            else
            {
                if (HasCoolerManualMode)
                {
                    m_PWMRequest = 0;
                    m_TemperatureRequest = 30;
                    SetQHYCCDParam(m_CameraHandle, CONTROL_MANULPWM, 0);

                    CoolerSP.s = IPS_IDLE;
                    IDSetSwitch(&CoolerSP, nullptr);

                    TemperatureNP.s = IPS_IDLE;
                    IDSetNumber(&TemperatureNP, nullptr);

                    setCoolerMode(COOLER_MANUAL);
                    LOG_INFO("Camera is warming up.");
                }
                else
                {
                    // Warm up the camera in auto mode
                    if (SetTemperature(30) == 0)
                    {
                        TemperatureNP.s = IPS_IDLE;
                        IDSetNumber(&TemperatureNP, nullptr);
                    }
                    LOG_INFO("Camera is warming up.");
                    return true;
                }
            }

            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Cooler Mode
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(CoolerModeSP.name, name))
        {
            IUUpdateSwitch(&CoolerModeSP, states, names, n);
            if (IUFindOnSwitchIndex(&CoolerModeSP) == COOLER_AUTOMATIC)
            {
                m_PWMRequest = -1;
                LOG_INFO("Camera cooler is now automatically controlled to maintain the desired temperature.");
            }
            else
            {
                m_TemperatureRequest = 30;
                LOG_INFO("Camera cooler is manually controlled. Set the desired cooler power.");
            }

            IDSetSwitch(&CoolerModeSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// GPS Header
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(GPSControlSP.name, name))
        {
            IUUpdateSwitch(&GPSControlSP, states, names, n);
            int rc = SetQHYCCDParam(m_CameraHandle, CAM_GPS, GPSControlS[INDI_ENABLED].s == ISS_ON ? 1 : 0);
            if (rc == QHYCCD_SUCCESS)
            {
                GPSControlSP.s = IPS_OK;
                LOGF_INFO("GPS header is %s.", GPSControlS[INDI_ENABLED].s == ISS_ON ? "Enabled" : "Disabled");
            }
            else
            {
                GPSControlSP.s = IPS_ALERT;
                LOGF_ERROR("Failed to toggle GPS header: %d.", rc);
            }
            IDSetSwitch(&GPSControlSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// GPS Slaving Mode
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(GPSSlavingSP.name, name))
        {
            IUUpdateSwitch(&GPSSlavingSP, states, names, n);
            int rc = SetQHYCCDGPSMasterSlave(m_CameraHandle, IUFindOnSwitchIndex(&GPSSlavingSP));
            if (rc == QHYCCD_SUCCESS)
            {
                GPSSlavingSP.s = IPS_OK;
                LOGF_INFO("GPS slaving mode is set to %s.", GPSSlavingS[INDI_ENABLED].s == ISS_ON ? "Master" : "Slave");
            }
            else
            {
                GPSSlavingSP.s = IPS_ALERT;
                LOGF_ERROR("Failed to toggle GPS slaving: %d", rc);
            }
            IDSetSwitch(&GPSSlavingSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// GPS LED Calibration
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(GPSLEDCalibrationSP.name, name))
        {
            IUUpdateSwitch(&GPSLEDCalibrationSP, states, names, n);
            int rc = SetQHYCCDGPSLedCalMode(m_CameraHandle, GPSLEDCalibrationS[INDI_ENABLED].s == ISS_ON ? 1 : 0);
            if (rc == QHYCCD_SUCCESS)
            {
                GPSLEDCalibrationSP.s = IPS_OK;
                LOGF_INFO("GPS LED calibration light is %s.", GPSLEDCalibrationS[INDI_ENABLED].s == ISS_ON ? "On" : "Off");
            }
            else
            {
                GPSLEDCalibrationSP.s = IPS_ALERT;
                LOGF_ERROR("Failed to toggle GPS LED calibration light: %d.", rc);
            }
            IDSetSwitch(&GPSLEDCalibrationSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Amp Glow
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(AMPGlowSP.name, name))
        {
            int prevIndex = IUFindOnSwitchIndex(&AMPGlowSP);
            IUUpdateSwitch(&AMPGlowSP, states, names, n);
            double targetIndex = IUFindOnSwitchIndex(&AMPGlowSP);
            int rc = SetQHYCCDParam(m_CameraHandle, CONTROL_AMPV, targetIndex);
            if (rc == QHYCCD_SUCCESS)
            {
                AMPGlowSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&AMPGlowSP);
                AMPGlowS[prevIndex].s = ISS_ON;
                AMPGlowSP.s = IPS_ALERT;
            }

            IDSetSwitch(&AMPGlowSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Overscan Area
        //////////////////////////////////////////////////////////////////////
        //NEW CODE - Add support for overscan/calibration area
        else if (!strcmp(OverscanAreaSP.name, name))
        {

            IUUpdateSwitch(&OverscanAreaSP, states, names, n);
            bool isIgnored = (OverscanAreaS[INDI_ENABLED].s == ISS_OFF); //Overscan Area included switch is 'off', thus excluded

            if (isIgnored == IgnoreOverscanArea)
            {
                OverscanAreaSP.s = IPS_OK;
                IDSetSwitch(&OverscanAreaSP, nullptr);
                return true;
            }

            IgnoreOverscanArea = isIgnored;

            OverscanAreaS[INDI_DISABLED].s = IgnoreOverscanArea ? ISS_ON : ISS_OFF;
            OverscanAreaS[INDI_ENABLED].s = IgnoreOverscanArea ? ISS_OFF : ISS_ON;

            OverscanAreaSP.s = IgnoreOverscanArea ? IPS_IDLE : IPS_OK;

            if (IgnoreOverscanArea)
            {
                //Image Info
                SetCCDParams(effectiveROI.subW, effectiveROI.subH, PrimaryCCD.getBPP(), PrimaryCCD.getPixelSizeX(),
                             PrimaryCCD.getPixelSizeX());

                // Image Settings
                // The true frame origin is at (effectiveROI.subX ,effectiveROI.subY).
                // This vector that needs to be used for offset-correction when taking exposures or streaming while ignoring overscan areas.
                UpdateCCDFrame(0, 0, effectiveROI.subW, effectiveROI.subH);

            }
            else
            {
                //Image Info
                SetCCDParams(sensorROI.subW, sensorROI.subH, PrimaryCCD.getBPP(), PrimaryCCD.getPixelSizeX(), PrimaryCCD.getPixelSizeX());
                //Image Settings
                UpdateCCDFrame(sensorROI.subX, sensorROI.subY, sensorROI.subW, sensorROI.subH);
            }

            LOGF_INFO("The overscan area is %s now. The effective frame starts at coordinates (%u, %u) ",
                      IgnoreOverscanArea ? "ignored" : "included", IgnoreOverscanArea ? effectiveROI.subX : 0,
                      IgnoreOverscanArea ? effectiveROI.subY : 0);

            IDSetSwitch(&OverscanAreaSP, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool QHYCCD::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        //  This is for our device
        //  Now lets see if it's something we process here
        if (strcmp(name, FilterNameTP->name) == 0)
        {
            INDI::FilterInterface::processText(dev, name, texts, names, n);
            return true;
        }
    }

    return INDI::CCD::ISNewText(dev, name, texts, names, n);
}

bool QHYCCD::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, FilterSlotNP.name))
        {
            return INDI::FilterInterface::processNumber(dev, name, values, names, n);
        }

        //////////////////////////////////////////////////////////////////////
        /// Gain Control
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, GainNP.name))
        {
            double currentGain = GainN[0].value;
            IUUpdateNumber(&GainNP, values, names, n);
            if (fabs(m_LastGainRequest - GainN[0].value) > UPDATE_THRESHOLD)
            {
                int rc = SetQHYCCDParam(m_CameraHandle, CONTROL_GAIN, GainN[0].value);
                if (rc == QHYCCD_SUCCESS)
                {
                    m_LastGainRequest = GainN[0].value;
                    GainNP.s = IPS_OK;
                    saveConfig(true, GainNP.name);
                    LOGF_INFO("Gain updated to %.f", GainN[0].value);
                }
                else
                {
                    GainN[0].value = currentGain;
                    GainNP.s = IPS_ALERT;
                    LOGF_ERROR("Failed to changed gain: %d.", rc);
                }
            }
            else
                GainNP.s = IPS_OK;

            IDSetNumber(&GainNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Offset Control
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(name, OffsetNP.name))
        {
            double currentOffset = OffsetN[0].value;
            IUUpdateNumber(&OffsetNP, values, names, n);
            int rc = SetQHYCCDParam(m_CameraHandle, CONTROL_OFFSET, OffsetN[0].value);

            if (rc == QHYCCD_SUCCESS)
            {
                OffsetNP.s = IPS_OK;

                if (std::abs(currentOffset - OffsetN[0].value) > UPDATE_THRESHOLD)
                {
                    LOGF_INFO("Offset updated to %.f", OffsetN[0].value);
                    saveConfig(true, OffsetNP.name);
                }
            }
            else
            {
                LOGF_ERROR("Failed to update offset: %.f", OffsetN[0].value);
                OffsetN[0].value = currentOffset;
                OffsetNP.s = IPS_ALERT;
            }

            IDSetNumber(&OffsetNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Speed Control
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(name, SpeedNP.name))
        {
            double currentSpeed = SpeedN[0].value;
            IUUpdateNumber(&SpeedNP, values, names, n);
            int rc = SetQHYCCDParam(m_CameraHandle, CONTROL_SPEED, SpeedN[0].value);

            if (rc == QHYCCD_SUCCESS)
            {
                SpeedNP.s = IPS_OK;
                if (std::abs(currentSpeed - SpeedN[0].value) > UPDATE_THRESHOLD)
                {
                    LOGF_INFO("Speed updated to %.f", SpeedN[0].value);
                    saveConfig(true, SpeedNP.name);
                }
            }
            else
            {
                LOGF_ERROR("Failed to update speed: %d", rc);
                SpeedNP.s = IPS_ALERT;
                SpeedN[0].value = currentSpeed;
            }

            IDSetNumber(&SpeedNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// USB Traffic Control
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(name, USBTrafficNP.name))
        {
            double currentTraffic = USBTrafficN[0].value;
            IUUpdateNumber(&USBTrafficNP, values, names, n);
            int rc = SetQHYCCDParam(m_CameraHandle, CONTROL_USBTRAFFIC, USBTrafficN[0].value);
            if (rc == QHYCCD_SUCCESS)
            {
                LOGF_INFO("USB Traffic updated to %.f", USBTrafficN[0].value);
                USBTrafficNP.s = IPS_OK;
                saveConfig(true, USBTrafficNP.name);
            }
            else
            {
                USBTrafficNP.s = IPS_ALERT;
                USBTrafficN[0].value = currentTraffic;
                LOGF_ERROR("Failed to update USB Traffic: %d", rc);
            }

            IDSetNumber(&USBTrafficNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// USB Buffer Control
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(name, USBBufferNP.name))
        {
            IUUpdateNumber(&USBBufferNP, values, names, n);
            SetQHYCCDBufferNumber(USBBufferN[0].value);
            LOGF_INFO("USB Buffer updated to %.f", USBBufferN[0].value);
            USBBufferNP.s = IPS_OK;
            saveConfig(true, USBBufferNP.name);
            IDSetNumber(&USBBufferNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Read Modes Control
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(name, ReadModeNP.name))
        {
            //NEW CODE - Fix read mode handling
            IUUpdateNumber(&ReadModeNP, values, names, n);
            uint32_t newReadMode = static_cast<uint32_t>(ReadModeN[0].value);

            // Set readout mode
            if (newReadMode != currentQHYReadMode)
            {
                /* change readout mode */
                int rc = SetQHYCCDReadMode(m_CameraHandle, newReadMode); // [NEW CODE] declaration int rc removed
                if (rc == QHYCCD_SUCCESS)
                {
                    /* re-initialize the camera */
                    rc = InitQHYCCD(m_CameraHandle);
                    if (rc != QHYCCD_SUCCESS)
                    {
                        LOGF_ERROR("Init Camera failed (%d)", rc);
                        SetQHYCCDReadMode(m_CameraHandle, currentQHYReadMode);
                        IDSetNumber(&ReadModeNP, nullptr);
                        return false;
                    }

                    currentQHYReadMode = newReadMode;

                    LOGF_INFO("Current read mode: %s (%dx%d)", readModeInfo[currentQHYReadMode].label,
                              readModeInfo[currentQHYReadMode].subW, readModeInfo[currentQHYReadMode].subH);

                    /* re-initialize the camera parameters */
                    QHYCCD::setupParams();
                    saveConfig(true, ReadModeNP.name);
                    ReadModeNP.s = IPS_OK;
                }
                else
                {
                    ReadModeNP.s = IPS_ALERT;
                    /* assume the camera did not switch read modes and thus is still in the former read mode */
                    ReadModeN[0].value = currentQHYReadMode;
                    LOGF_ERROR("Failed to update read mode: %d", rc);
                }
            }
            else
            {
                /* re-initialize the camera parameters */
                QHYCCD::setupParams();
                saveConfig(true, ReadModeNP.name);
                ReadModeNP.s = IPS_OK;
            }

            IDSetNumber(&ReadModeNP, nullptr);
            return true;
        }


        //////////////////////////////////////////////////////////////////////
        /// Cooler PWM Control
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(name, CoolerNP.name))
        {
            if (HasCoolerManualMode == false)
            {
                CoolerNP.s = IPS_ALERT;
                LOG_WARN("Manual cooler control is not available.");
                IDSetNumber(&CoolerNP, nullptr);
            }

            setCoolerEnabled(values[0] > 0);
            setCoolerMode(COOLER_MANUAL);

            m_PWMRequest = values[0] / 100.0 * 255;
            CoolerNP.s = IPS_BUSY;
            LOGF_INFO("Setting cooler power manually to %.2f%%", values[0]);
            IDSetNumber(&CoolerNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// VCOX Frequency
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(name, VCOXFreqNP.name))
        {
            IUUpdateNumber(&VCOXFreqNP, values, names, n);
            int rc = SetQHYCCDGPSVCOXFreq(m_CameraHandle, static_cast<uint16_t>(VCOXFreqN[0].value));
            VCOXFreqNP.s = (rc == QHYCCD_SUCCESS) ? IPS_OK : IPS_ALERT;
            IDSetNumber(&VCOXFreqNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// GPS Params
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(name, GPSSlavingParamNP.name))
        {
            IUUpdateNumber(&GPSSlavingParamNP, values, names, n);
            SetQHYCCDGPSSlaveModeParameter(m_CameraHandle,
                                           static_cast<uint32_t>(GPSSlavingParamN[PARAM_TARGET_SEC].value),
                                           static_cast<uint32_t>(GPSSlavingParamN[PARAM_TARGET_USEC].value),
                                           static_cast<uint32_t>(GPSSlavingParamN[PARAM_DELTAT_SEC].value),
                                           static_cast<uint32_t>(GPSSlavingParamN[PARAM_DELTAT_USEC].value),
                                           static_cast<uint32_t>(GPSSlavingParamN[PARAM_EXP_TIME].value));
            GPSSlavingParamNP.s = IPS_OK;
            IDSetNumber(&GPSSlavingParamNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// GPS Calibration LED Start
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(name, GPSLEDStartPosNP.name))
        {
            IUUpdateNumber(&GPSLEDStartPosNP, values, names, n);
            SetQHYCCDGPSPOSA(m_CameraHandle, GPSSlavingS[SLAVING_MASTER].s == ISS_ON ? 0 : 1,
                             static_cast<uint32_t>(GPSLEDStartPosN[LED_PULSE_POSITION].value),
                             static_cast<uint8_t>(GPSLEDStartPosN[LED_PULSE_WIDTH].value));
            GPSLEDStartPosNP.s = IPS_OK;
            IDSetNumber(&GPSLEDStartPosNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// GPS Calibration LED Emd
        //////////////////////////////////////////////////////////////////////
        else if (!strcmp(name, GPSLEDEndPosNP.name))
        {
            IUUpdateNumber(&GPSLEDEndPosNP, values, names, n);
            SetQHYCCDGPSPOSB(m_CameraHandle, GPSSlavingS[SLAVING_MASTER].s == ISS_ON ? 0 : 1,
                             static_cast<uint32_t>(GPSLEDEndPosN[LED_PULSE_POSITION].value),
                             static_cast<uint8_t>(GPSLEDEndPosN[LED_PULSE_WIDTH].value));
            GPSLEDEndPosNP.s = IPS_OK;
            IDSetNumber(&GPSLEDEndPosNP, nullptr);
            return true;
        }

    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

void QHYCCD::setCoolerMode(uint8_t mode)
{
    int currentMode = IUFindOnSwitchIndex(&CoolerModeSP);
    if (mode == currentMode)
        return;

    IUResetSwitch(&CoolerModeSP);

    CoolerModeS[COOLER_AUTOMATIC].s = (mode == COOLER_AUTOMATIC) ? ISS_ON : ISS_OFF;
    CoolerModeS[COOLER_MANUAL].s = (mode == COOLER_AUTOMATIC) ? ISS_OFF : ISS_ON;
    CoolerSP.s = IPS_OK;
    LOGF_INFO("Switching to %s cooler control.", (mode == COOLER_AUTOMATIC) ? "automatic" : "manual");
    IDSetSwitch(&CoolerModeSP, nullptr);
}

void QHYCCD::setCoolerEnabled(bool enable)
{
    bool isEnabled = IUFindOnSwitchIndex(&CoolerSP) == COOLER_ON;
    if (isEnabled == enable)
        return;

    IUResetSwitch(&CoolerSP);
    CoolerS[COOLER_ON].s = enable ? ISS_ON : ISS_OFF;
    CoolerS[COOLER_OFF].s = enable ? ISS_OFF : ISS_ON;
    CoolerSP.s = enable ? IPS_BUSY : IPS_IDLE;
    IDSetSwitch(&CoolerSP, nullptr);
}

bool QHYCCD::isQHY5PIIC()
{
    return std::string(m_CamID, 9) == "QHY5PII-C";
}

void QHYCCD::updateTemperatureHelper(void *p)
{
    static_cast<QHYCCD *>(p)->updateTemperature();
}

void QHYCCD::updateTemperature()
{
    double currentTemperature = 0, currentCoolingPower = 0, currentHumidity = 0;

    if (isSimulation())
    {
        currentTemperature = TemperatureN[0].value;
        if (TemperatureN[0].value < m_TemperatureRequest)
            currentTemperature += UPDATE_THRESHOLD * 10;
        else if (TemperatureN[0].value > m_TemperatureRequest)
            currentTemperature -= UPDATE_THRESHOLD * 10;

        currentCoolingPower = 128;
    }
    else
    {
        // Sleep for 1 second before setting temperature/pwm again.
        //usleep(1000000);

        // Call this function as long as we are busy
        if (TemperatureNP.s == IPS_BUSY)
        {
            SetQHYCCDParam(m_CameraHandle, CONTROL_COOLER, m_TemperatureRequest);
        }
        else if (m_PWMRequest >= 0)
        {
            SetQHYCCDParam(m_CameraHandle, CONTROL_MANULPWM, m_PWMRequest);
        }
        // JM 2020-05-18: QHY reported the code below break automatic coolers, so it is only avaiable for manual coolers.
        // Temperature Readout does not work, if we do not set "something", so lets set the current value...
        else if (CoolerModeS[COOLER_MANUAL].s == ISS_ON && TemperatureNP.s == IPS_OK)
        {
            SetQHYCCDParam(m_CameraHandle, CONTROL_MANULPWM, CoolerN[0].value * 255.0 / 100 );
        }

        currentTemperature   = GetQHYCCDParam(m_CameraHandle, CONTROL_CURTEMP);
        currentCoolingPower = GetQHYCCDParam(m_CameraHandle, CONTROL_CURPWM);
    }

    // Only update if above update threshold
    if (std::abs(currentTemperature - TemperatureN[0].value) > UPDATE_THRESHOLD)
    {
        if (currentTemperature > 100)
            TemperatureNP.s = IPS_ALERT;
        else
            TemperatureN[0].value = currentTemperature;
        IDSetNumber(&TemperatureNP, nullptr);

        LOGF_DEBUG("CCD T.: %.f (C)", currentTemperature);
    }
    // Restart temperature regulation if needed.
    else if (TemperatureNP.s == IPS_OK && fabs(TemperatureN[0].value - m_TemperatureRequest) > UPDATE_THRESHOLD)
    {
        if (currentTemperature > 100)
            TemperatureNP.s       = IPS_ALERT;
        else
        {
            TemperatureN[0].value = currentTemperature;
            TemperatureNP.s       = IPS_BUSY;
        }
        IDSetNumber(&TemperatureNP, nullptr);
    }

    // Update cooling power if needed.
    if (std::abs(currentCoolingPower - CoolerN[0].value) > UPDATE_THRESHOLD)
    {
        if (currentCoolingPower > 255)
            CoolerNP.s = IPS_ALERT;
        else
        {
            CoolerN[0].value      = currentCoolingPower / 255.0 * 100;
            CoolerNP.s = CoolerN[0].value > 0 ? IPS_BUSY : IPS_IDLE;
        }
        IDSetNumber(&CoolerNP, nullptr);
        LOGF_DEBUG("Cooling Power: %.f (%.2f%%)", currentCoolingPower, currentCoolingPower / 255.0 * 100);
    }

    // Synchronize state of cooling power and cooling switch
    IPState coolerSwitchState = CoolerN[0].value > 0 ? IPS_BUSY : IPS_OK;
    if (coolerSwitchState != CoolerSP.s)
    {
        CoolerSP.s = coolerSwitchState;
        IDSetSwitch(&CoolerSP, nullptr);
    }

    // Check humidity and update if necessary
    if (HasHumidity)
    {
        IPState currentState = (GetQHYCCDHumidity(m_CameraHandle, &currentHumidity) == QHYCCD_SUCCESS) ? IPS_OK : IPS_ALERT;
        if (currentState != HumidityNP.s || std::abs(currentHumidity - HumidityN[0].value) > UPDATE_THRESHOLD)
        {
            HumidityN[0].value = currentHumidity;
            HumidityNP.s = currentState;
            IDSetNumber(&HumidityNP, nullptr);
        }
    }

    m_TemperatureTimerID = IEAddTimer(getCurrentPollingPeriod(), QHYCCD::updateTemperatureHelper, this);
}

bool QHYCCD::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    if (HasFilters)
    {
        INDI::FilterInterface::saveConfigItems(fp);
    }

    if (HasGain)
        IUSaveConfigNumber(fp, &GainNP);

    if (HasOffset)
        IUSaveConfigNumber(fp, &OffsetNP);

    if (HasUSBSpeed)
        IUSaveConfigNumber(fp, &SpeedNP);

    if (HasReadMode)
        IUSaveConfigNumber(fp, &ReadModeNP);

    if (HasUSBTraffic)
        IUSaveConfigNumber(fp, &USBTrafficNP);

    if (HasAmpGlow)
        IUSaveConfigSwitch(fp, &AMPGlowSP);

    if (HasGPS)
    {
        IUSaveConfigSwitch(fp, &GPSControlSP);
        IUSaveConfigSwitch(fp, &GPSSlavingSP);
        IUSaveConfigNumber(fp, &VCOXFreqNP);
    }

    IUSaveConfigNumber(fp, &USBBufferNP);

    return true;
}

bool QHYCCD::StartStreaming()
{
#if defined(__arm__) || defined(__aarch64__)
    if (USBBufferN[0].value < USBBufferN[0].min * 4)
        LOGF_INFO("For better streaming performance, set USB buffer to %.f or higher.", USBBufferN[0].min * 4);
#endif

    int ret = 0;
    m_ExposureRequest = 1.0 / Streamer->getTargetFPS();

    //NEW CODE - Add support for overscan/calibration area
    uint32_t subX = (PrimaryCCD.getSubX() + (IgnoreOverscanArea ? effectiveROI.subX : 0)) / PrimaryCCD.getBinX();
    uint32_t subY = (PrimaryCCD.getSubY() + (IgnoreOverscanArea ? effectiveROI.subY : 0)) / PrimaryCCD.getBinY();
    uint32_t subW = PrimaryCCD.getSubW() / PrimaryCCD.getBinX();
    uint32_t subH = PrimaryCCD.getSubH() / PrimaryCCD.getBinY();

    // N.B. There is no corresponding value for GBGR. It is odd that QHY selects this as the default as no one seems to process it
    const std::map<const char *, INDI_PIXEL_FORMAT> formats = { { "GBGR", INDI_MONO },
        { "GRGB", INDI_BAYER_GRBG },
        { "BGGR", INDI_BAYER_BGGR },
        { "RGGB", INDI_BAYER_RGGB }
    };

    // Set Stream Mode and re-initialize camera
    //SetQHYCCDStreamMode(m_CameraHandle, currentQHYStreamMode);
    if (currentQHYStreamMode == 0  && !isSimulation())
    {

        /* NR - Closing the camera will reset the connection in ECOS, I recommend to omit here. */
        //CloseQHYCCD(m_CameraHandle);
        //ret = InitQHYCCD(m_CameraHandle);
        //if(ret != QHYCCD_SUCCESS)
        //{
        //    currentQHYStreamMode = 0;
        //    LOGF_INFO("Init QHYCCD for streaming mode failed, code:%d", ret);
        //    return false;
        //}

        /* switch camera to streaming mode */
        currentQHYStreamMode = 1;
        SetQHYCCDStreamMode(m_CameraHandle, currentQHYStreamMode);
        /* re-initialize camera */
        ret = InitQHYCCD(m_CameraHandle);
        if(ret != QHYCCD_SUCCESS)
        {
            currentQHYStreamMode = 0;
            LOGF_INFO("Init QHYCCD for streaming mode failed, code:%d", ret);
            return false;
        }
    }

    // Set binning mode
    if (isSimulation())
        ret = QHYCCD_SUCCESS;
    else
        ret = SetQHYCCDBinMode(m_CameraHandle, PrimaryCCD.getBinX(), PrimaryCCD.getBinY());
    if (ret != QHYCCD_SUCCESS)
    {
        LOGF_INFO("Set QHYCCD Bin mode failed (%d)", ret);
        return false;
    }

    LOGF_DEBUG("SetQHYCCDBinMode (%dx%d).", PrimaryCCD.getBinX(), PrimaryCCD.getBinY());

    // Set Region of Interest (ROI)
    if (isSimulation())
        ret = QHYCCD_SUCCESS;
    else
        ret = SetQHYCCDResolution(m_CameraHandle, subX, subY, subW, subH);

    if (ret != QHYCCD_SUCCESS)
    {
        LOGF_INFO("Set QHYCCD ROI resolution (%d,%d) (%d,%d) failed (%d)", subX, subY, subW, subH, ret);
        return false;
    }

    LOGF_DEBUG("SetQHYCCDResolution x: %d y: %d w: %d h: %d", subX, subY, subW, subH, ret);

    INDI_PIXEL_FORMAT qhyFormat = INDI_MONO;
    if (BayerT[2].text && formats.count(BayerT[2].text) != 0)
        qhyFormat = formats.at(BayerT[2].text);

    double uSecs = static_cast<long>(m_ExposureRequest * 950000.0);

    SetQHYCCDParam(m_CameraHandle, CONTROL_EXPOSURE, uSecs);

    if (HasUSBSpeed)
    {
        ret = SetQHYCCDParam(m_CameraHandle, CONTROL_SPEED, SpeedN[0].value);
        if (ret != QHYCCD_SUCCESS)
            LOG_WARN("SetQHYCCDParam CONTROL_SPEED 2.0 failed.");
    }

    if (HasUSBTraffic)
    {
        ret = SetQHYCCDParam(m_CameraHandle, CONTROL_USBTRAFFIC, USBTrafficN[0].value);
        if (ret != QHYCCD_SUCCESS)
            LOG_WARN("SetQHYCCDParam CONTROL_USBTRAFFIC 20.0 failed.");
    }

    ret = SetQHYCCDBitsMode(m_CameraHandle, 8);
    if (ret == QHYCCD_SUCCESS)
        Streamer->setPixelFormat(qhyFormat, 8);
    else
    {
        LOG_WARN("SetQHYCCDBitsMode 8bit failed.");
        Streamer->setPixelFormat(qhyFormat, PrimaryCCD.getBPP());
    }

    //LOG_INFO("start live mode"); //DEBUG

    LOGF_INFO("Starting video streaming with exposure %.f seconds (%.f FPS), w=%d h=%d", m_ExposureRequest,
              Streamer->getTargetFPS(), subW, subH);
    BeginQHYCCDLive(m_CameraHandle);
    pthread_mutex_lock(&condMutex);
    m_ThreadRequest = StateStream;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    return true;
}

bool QHYCCD::StopStreaming()
{
    pthread_mutex_lock(&condMutex);
    m_ThreadRequest = StateAbort;
    pthread_cond_signal(&cv);
    while (m_ThreadState == StateStream)
    {
        pthread_cond_wait(&cv, &condMutex);
    }
    pthread_mutex_unlock(&condMutex);
    StopQHYCCDLive(m_CameraHandle);

    //LOG_INFO("stopped live mode"); //DEBUG

    //if (HasUSBSpeed)
    //    SetQHYCCDParam(m_CameraHandle, CONTROL_SPEED, SpeedN[0].value);
    //if (HasUSBTraffic)
    //    SetQHYCCDParam(m_CameraHandle, CONTROL_USBTRAFFIC, USBTrafficN[0].value);

    currentQHYStreamMode = 0;
    SetQHYCCDStreamMode(m_CameraHandle, currentQHYStreamMode);

    // FIX: Helps for cleaner teardown and prevents camera from staling
    InitQHYCCD(m_CameraHandle);

    // Try to set 16bit mode if supported back. Use PrimaryCCD.getBPP as this is what we use to allocate the image buffer.
    // Instead of doing this here, set the new bitmode when we go into exposure mode out of streaming mode.
    //SetQHYCCDBitsMode(m_CameraHandle, PrimaryCCD.getBPP());
    return true;
}

void *QHYCCD::imagingHelper(void *context)
{
    return static_cast<QHYCCD *>(context)->imagingThreadEntry();
}

/*
 * A dedicated thread is used for handling streaming video and image
 * exposures because the operations take too much time to be done
 * as part of a timer call-back: there is one timer for the entire
 * process, which must handle events for all ASI cameras
 */
void *QHYCCD::imagingThreadEntry()
{
    pthread_mutex_lock(&condMutex);
    m_ThreadState = StateIdle;
    pthread_cond_signal(&cv);
    while (true)
    {
        while (m_ThreadRequest == StateIdle)
        {
            pthread_cond_wait(&cv, &condMutex);
        }
        m_ThreadState = m_ThreadRequest;
        if (m_ThreadRequest == StateExposure)
        {
            getExposure();
        }
        else if (m_ThreadRequest == StateStream)
        {
            streamVideo();
        }
        else if (m_ThreadRequest == StateRestartExposure)
        {
            m_ThreadRequest = StateIdle;
            pthread_mutex_unlock(&condMutex);
            StartExposure(m_ExposureRequest);
            pthread_mutex_lock(&condMutex);
        }
        else if (m_ThreadRequest == StateTerminate)
        {
            break;
        }
        else
        {
            m_ThreadRequest = StateIdle;
            pthread_cond_signal(&cv);
        }
        m_ThreadState = StateIdle;
    }
    m_ThreadState = StateTerminated;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&condMutex);

    return nullptr;
}

void QHYCCD::streamVideo()
{
    uint32_t ret = 0, w, h, bpp, channels;
    //uint32_t t_start = time(NULL), frames = 0;
    while (m_ThreadRequest == StateStream)
    {
        pthread_mutex_unlock(&condMutex);
        uint32_t retries = 0;
        std::unique_lock<std::mutex> guard(ccdBufferLock);
        uint8_t *buffer = PrimaryCCD.getFrameBuffer();
        while (retries++ < 10)
        {

            ret = GetQHYCCDLiveFrame(m_CameraHandle, &w, &h, &bpp, &channels, buffer);
            if (ret == QHYCCD_ERROR)
                usleep(1000);
            else
                break;
        }
        guard.unlock();
        if (ret == QHYCCD_SUCCESS)
        {
            uint64_t timestamp = 0;
            if (HasGPS && GPSControlS[INDI_ENABLED].s == ISS_ON)
            {
                decodeGPSHeader();
                timestamp = (uint64_t)GPSHeader.start_sec * 1e6;
                timestamp += GPSHeader.start_us + QHY_SER_US_EPOCH;
            }

            Streamer->newFrame(buffer, w * h * bpp / 8 * channels, timestamp);

            //DEBUG
            //if(!frames)
            //    LOGF_DEBUG("Receiving frames ...");
            //if(!(++frames % 30))
            //    LOGF_DEBUG("Frames received: %d (%.1f fps)", frames, 1.0 * frames / (time(NULL) - t_start));
        }
        pthread_mutex_lock(&condMutex);
    }
}

void QHYCCD::getExposure()
{
    pthread_mutex_unlock(&condMutex);
    usleep(10000);
    pthread_mutex_lock(&condMutex);

    while (m_ThreadRequest == StateExposure)
    {
        pthread_mutex_unlock(&condMutex);
        /*
         * Check the status every second until the time left is
         * about one second, after which decrease the poll interval
         */
        double timeLeft = calcTimeLeft();
        uint32_t uSecs = 100000;
        if (timeLeft > 1.1)
        {
            /*
             * For expsoures with more than a second left try
             * to keep the displayed "exposure left" value at
             * a full second boundary, which keeps the
             * count down neat
             */
            timeLeft = round(timeLeft);
            uSecs = 1000000;
        }

        if (timeLeft >= 0)
        {
            PrimaryCCD.setExposureLeft(timeLeft);
        }
        else
        {
            InExposure = false;
            PrimaryCCD.setExposureLeft(0.0);
            if (m_ExposureRequest * 1000 > 5 * getCurrentPollingPeriod())
                DEBUG(INDI::Logger::DBG_SESSION, "Exposure done, downloading image...");
            pthread_mutex_lock(&condMutex);
            exposureSetRequest(StateIdle);
            pthread_mutex_unlock(&condMutex);
            grabImage();
            pthread_mutex_lock(&condMutex);
            break;
        }
        usleep(uSecs);

        pthread_mutex_lock(&condMutex);
    }
}

/* Caller must hold the mutex */
void QHYCCD::exposureSetRequest(ImageState request)
{
    if (m_ThreadRequest == StateExposure)
    {
        m_ThreadRequest = request;
    }
}

void QHYCCD::logQHYMessages(const std::string &message)
{
    LOGF_DEBUG("%s", message.c_str());
}

void QHYCCD::debugTriggered(bool enable)
{
    // For some reason QHYSDK does not define this for MacOS! Needs to be fixed
#ifdef __linux__
    // JM QHY removed this function on 2023.07.16
    //SetQHYCCDLogFunction(m_QHYLogCallback);
#endif
    if (enable)
        SetQHYCCDLogLevel(5);
    else
        SetQHYCCDLogLevel(2);
}

bool QHYCCD::updateFilterProperties()
{
    if (FilterNameTP->ntp != m_MaxFilterCount)
    {
        LOGF_DEBUG("Max filter count is: %d", m_MaxFilterCount);
        FilterSlotN[0].max = m_MaxFilterCount;
        char filterName[MAXINDINAME];
        char filterLabel[MAXINDILABEL];
        if (FilterNameT != nullptr)
        {
            for (int i = 0; i < FilterNameTP->ntp; i++)
                free(FilterNameT[i].text);
            delete [] FilterNameT;
        }

        FilterNameT = new IText[m_MaxFilterCount];
        memset(FilterNameT, 0, sizeof(IText) * m_MaxFilterCount);
        for (int i = 0; i < m_MaxFilterCount; i++)
        {
            snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", i + 1);
            snprintf(filterLabel, MAXINDILABEL, "Filter#%d", i + 1);
            IUFillText(&FilterNameT[i], filterName, filterLabel, filterLabel);
        }
        IUFillTextVector(FilterNameTP, FilterNameT, m_MaxFilterCount, m_defaultDevice->getDeviceName(), "FILTER_NAME", "Filter",
                         FilterSlotNP.group, IP_RW, 0, IPS_IDLE);

        // Try to load config filter labels
        for (int i = 0; i < m_MaxFilterCount; i++)
        {
            char oneFilter[MAXINDINAME] = {0};
            if (IUGetConfigText(getDeviceName(), FilterNameTP->name, FilterNameT[i].name, oneFilter, MAXINDINAME) == 0)
                IUSaveText(&FilterNameT[i], oneFilter);
        }

        return true;
    }

    return false;
}

void QHYCCD::addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords)
{
    INDI::CCD::addFITSKeywords(targetChip, fitsKeywords);

    if (HasGain)
    {
        fitsKeywords.push_back({"GAIN", GainN[0].value, 3, "Gain"});
    }

    if (HasOffset)
    {
        fitsKeywords.push_back({"OFFSET", OffsetN[0].value, 3, "Offset"});
    }

    if (HasAmpGlow)
    {
        fitsKeywords.push_back({"AMPGLOW", IUFindOnSwitch(&AMPGlowSP)->label, "Mode"});
    }

    if (HasReadMode)
    {
        fitsKeywords.push_back({"READMODE", ReadModeN[0].value, 1, "Read Mode"});
    }

    if (HasGPS)
    {
        // #1 Start
        // ## Flag
        fitsKeywords.push_back({"GPS_SFLG", GPSHeader.start_flag, "StartFlag"});
        // ## Seconds
        fitsKeywords.push_back({"GPS_SS", GPSHeader.start_sec, "StartShutterSeconds"});
        // ## Microseconds
        fitsKeywords.push_back({"GPS_SU", GPSHeader.start_us, 3, "StartShutterMicroSeconds"});
        // ## Time
        fitsKeywords.push_back({"GPS_ST", GPSDataStartT[GPS_DATA_START_TS].text, "StartShutterTime"});

        // #2 End
        // ## Flag
        fitsKeywords.push_back({"GPS_EFLG", GPSHeader.end_flag, "EndFlag"});
        // ## Seconds
        fitsKeywords.push_back({"GPS_ES", GPSHeader.end_sec, "EndShutterSeconds"});
        // ## Microseconds
        fitsKeywords.push_back({"GPS_EU", GPSHeader.end_us, 3, "EndShutterMicroSeconds"});
        // ## Time
        fitsKeywords.push_back({"GPS_ET", GPSDataStartT[GPS_DATA_END_TS].text, "EndShutterTime"});

        // #3 Now
        // ## Flag
        fitsKeywords.push_back({"GPS_NFLG", GPSHeader.now_flag, "NowFlag"});
        // ## Seconds
        fitsKeywords.push_back({"GPS_NS", GPSHeader.now_sec, "NowShutterSeconds"});
        // ## Microseconds
        fitsKeywords.push_back({"GPS_NU", GPSHeader.now_us, 3, "NowShutterMicroSeconds"});
        // ## Time
        fitsKeywords.push_back({"GPS_NT", GPSDataStartT[GPS_DATA_NOW_TS].text, "NowShutterTime"});

        // PPS Counter
        fitsKeywords.push_back({"GPS_PPSC", GPSHeader.max_clock, "PPSCounter"});

        // GPS Status

        // System Clock Offset
        //fitsKeywords.push_back({"GPS_DSYS", GPSHeader.now_us, 6, "System Clock - GPS Clock Offset (s)"});

        // Time Offset Stable for
        //fitsKeywords.push_back({"GPS_DSTB", GPSHeader.max_clock, "Time Offset Stable for (s)"});

        // Longitude
        fitsKeywords.push_back({"GPS_LONG", GPSHeader.longitude, 7, "GPS Longitude"});

        // Latitude
        fitsKeywords.push_back({"GPS_LAT", GPSHeader.latitude, 7, "GPS Latitude"});

        // Sequence Number
        fitsKeywords.push_back({"GPS_SEQ", GPSHeader.seqNumber, "Sequence Number"});

        // Temperorary Sequence Number
        fitsKeywords.push_back({"GPS_TMP", GPSHeader.tempNumber, "Temporary Sequence Number"});
    }

}

INumberVectorProperty QHYCCD::getLEDStartPosNP() const
{
    return GPSLEDStartPosNP;
}

void QHYCCD::setLEDStartPosNP(const INumberVectorProperty &value)
{
    GPSLEDStartPosNP = value;
}

void QHYCCD::decodeGPSHeader()
{
    char ts[64] = {0}, iso8601[64] = {0}, data[64] = {0};

    uint8_t gpsarray[64] = {0};
    memcpy(gpsarray, PrimaryCCD.getFrameBuffer(), 64);

    // Sequence Number
    GPSHeader.seqNumber = gpsarray[0] << 24 | gpsarray[1] << 16 | gpsarray[2] << 8 | gpsarray[3];
    snprintf(data, 64, "%u", GPSHeader.seqNumber);
    IUSaveText(&GPSDataHeaderT[GPS_DATA_SEQ_NUMBER], data);

    GPSHeader.tempNumber = gpsarray[4];

    // Width
    GPSHeader.width = gpsarray[5] << 8 | gpsarray[6];
    snprintf(data, 64, "%u", GPSHeader.width);
    IUSaveText(&GPSDataHeaderT[GPS_DATA_WIDTH], data);

    // Height
    GPSHeader.height = gpsarray[7] << 8 | gpsarray[8];
    snprintf(data, 64, "%u", GPSHeader.height);
    IUSaveText(&GPSDataHeaderT[GPS_DATA_HEIGHT], data);

    // Latitude
    uint32_t latitude = gpsarray[9] << 24 | gpsarray[10] << 16 | gpsarray[11] << 8 | gpsarray[12];
    // convert SDDMMMMMMM to DD.DDDDDDD
    GPSHeader.latitude = (latitude % 1000000000) / 10000000;
    GPSHeader.latitude += (latitude % 10000000) / 6000000.0;
    GPSHeader.latitude *= latitude > 1000000000 ? -1.0 : 1.0;
    snprintf(data, 64, "%f", GPSHeader.latitude);
    IUSaveText(&GPSDataHeaderT[GPS_DATA_LATITUDE], data);

    // Longitude
    uint32_t longitude = gpsarray[13] << 24 | gpsarray[14] << 16 | gpsarray[15] << 8 | gpsarray[16];
    // convert SDDDMMMMMM to DDD.DDDDDDD
    GPSHeader.longitude = (longitude % 1000000000) / 1000000;
    GPSHeader.longitude += (longitude % 1000000) / 600000.0;
    GPSHeader.longitude *= longitude > 1000000000 ? -1.0 : 1.0;
    snprintf(data, 64, "%f", GPSHeader.longitude);
    IUSaveText(&GPSDataHeaderT[GPS_DATA_LONGITUDE], data);

    // Start Flag
    GPSHeader.start_flag = gpsarray[17];
    snprintf(data, 64, "%u", GPSHeader.start_flag);
    IUSaveText(&GPSDataStartT[GPS_DATA_START_FLAG], data);

    // Start Seconds
    GPSHeader.start_sec = gpsarray[18] << 24 | gpsarray[19] << 16 | gpsarray[20] << 8 | gpsarray[21];
    snprintf(data, 64, "%u", GPSHeader.start_sec);
    IUSaveText(&GPSDataStartT[GPS_DATA_START_SEC], data);

    // Start microseconds
    // It's a 10Mhz crystal so we divide by 10 to get microseconds
    GPSHeader.start_us = (gpsarray[22] << 16 | gpsarray[23] << 8 | gpsarray[24]) / 10.0;
    snprintf(data, 64, "%.1f", GPSHeader.start_us);
    IUSaveText(&GPSDataStartT[GPS_DATA_START_USEC], data);

    // Start JD
    GPSHeader.start_jd = JStoJD(GPSHeader.start_sec, GPSHeader.start_us);
    // Get ISO8601
    JDtoISO8601(GPSHeader.start_jd, iso8601);
    // Add millisecond
    snprintf(ts, sizeof(ts), "%s.%03d", iso8601, static_cast<int>(GPSHeader.start_us / 1000.0));
    IUSaveText(&GPSDataStartT[GPS_DATA_START_TS], ts);

    // End Flag
    GPSHeader.end_flag = gpsarray[25];
    snprintf(data, 64, "%u", GPSHeader.end_flag);
    IUSaveText(&GPSDataEndT[GPS_DATA_END_FLAG], data);

    // End Seconds
    GPSHeader.end_sec = gpsarray[26] << 24 | gpsarray[27] << 16 | gpsarray[28] << 8 | gpsarray[29];
    snprintf(data, 64, "%u", GPSHeader.end_sec);
    IUSaveText(&GPSDataEndT[GPS_DATA_END_SEC], data);

    // End Microseconds
    GPSHeader.end_us = (gpsarray[30] << 16 | gpsarray[31] << 8 | gpsarray[32]) / 10.0;
    snprintf(data, 64, "%.1f", GPSHeader.end_us);
    IUSaveText(&GPSDataEndT[GPS_DATA_END_USEC], data);

    // End JD
    GPSHeader.end_jd = JStoJD(GPSHeader.end_sec, GPSHeader.end_us);
    // Get ISO8601
    JDtoISO8601(GPSHeader.end_jd, iso8601);
    // Add millisecond
    snprintf(ts, sizeof(ts), "%s.%03d", iso8601, static_cast<int>(GPSHeader.end_us / 1000.0));
    IUSaveText(&GPSDataEndT[GPS_DATA_END_TS], ts);

    // Now Flag
    GPSHeader.now_flag = gpsarray[33];
    snprintf(data, 64, "%u", GPSHeader.now_flag);
    IUSaveText(&GPSDataNowT[GPS_DATA_NOW_FLAG], data);

    // Now Seconds
    GPSHeader.now_sec = gpsarray[34] << 24 | gpsarray[35] << 16 | gpsarray[36] << 8 | gpsarray[37];
    snprintf(data, 64, "%u", GPSHeader.now_sec);
    IUSaveText(&GPSDataNowT[GPS_DATA_NOW_SEC], data);

    // Now microseconds
    GPSHeader.now_us = (gpsarray[38] << 16 | gpsarray[39] << 8 | gpsarray[40]) / 10.0;
    snprintf(data, 64, "%.1f", GPSHeader.now_us);
    IUSaveText(&GPSDataNowT[GPS_DATA_NOW_USEC], data);

    // Now JD
    GPSHeader.now_jd = JStoJD(GPSHeader.now_sec, GPSHeader.now_us);
    // Get ISO8601
    JDtoISO8601(GPSHeader.now_jd, iso8601);
    // Add millisecond
    snprintf(ts, sizeof(ts), "%s.%03d", iso8601, static_cast<int>(GPSHeader.now_us / 1000.0));
    IUSaveText(&GPSDataNowT[GPS_DATA_NOW_TS], ts);

    // PPS
    GPSHeader.max_clock = gpsarray[41] << 16 | gpsarray[42] << 8 | gpsarray[43];
    snprintf(data, 64, "%u", GPSHeader.max_clock);
    IUSaveText(&GPSDataHeaderT[GPS_DATA_MAX_CLOCK], data);

    IDSetText(&GPSDataHeaderTP, nullptr);
    IDSetText(&GPSDataStartTP, nullptr);
    IDSetText(&GPSDataEndTP, nullptr);
    IDSetText(&GPSDataNowTP, nullptr);

    GPSState newGPState = static_cast<GPSState>((GPSHeader.now_flag & 0xF0) >> 4);
    if (GPSStateL[newGPState].s == IPS_IDLE)
    {
        GPSStateL[GPS_ON].s = IPS_IDLE;
        GPSStateL[GPS_SEARCHING].s = IPS_IDLE;
        GPSStateL[GPS_LOCKING].s = IPS_IDLE;
        GPSStateL[GPS_LOCKED].s = IPS_IDLE;

        GPSStateL[newGPState].s = IPS_BUSY;
        GPSStateLP.s = IPS_OK;
        IDSetLight(&GPSStateLP, nullptr);
    }
}

double QHYCCD::JStoJD(uint32_t JS, double us)
{
    // Convert Julian seconds (plus microsecond) to Julian Days since epoch 2450000
    // Since this is why QHY apparently uses as the basis.
    // The 0.5 is added there since JD starts from MID day of the previous day
    return (JS + us / 1e6) / (3600 * 24) + 2450000.5;
}

void QHYCCD::JDtoISO8601(double JD, char *iso8601)
{
    struct tm *tp = nullptr;
    time_t gpstime;
    ln_get_timet_from_julian(JD, &gpstime);
    // Get UTC timestamp
    tp = gmtime(&gpstime);
    // Format it in ISO8601 format
    strftime(iso8601, MAXINDIDEVICE, "%Y-%m-%dT%H:%M:%S", tp);
}

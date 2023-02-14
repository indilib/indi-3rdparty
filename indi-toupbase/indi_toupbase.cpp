/*
 Toupcam CCD Driver

 Copyright (C) 2018-2019 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "indi_toupbase.h"
#include "config.h"
#include <stream/streammanager.h>
#include <math.h>
#include <unistd.h>
#include <deque>
#include <memory>

#define VERBOSE_EXPOSURE        3
#define CONTROL_TAB "Controls"
#define LEVEL_TAB "Levels"

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
    (static_cast<uint32_t>(static_cast<uint8_t>(ch0)) \
    | (static_cast<uint32_t>(static_cast<uint8_t>(ch1)) << 8) \
    | (static_cast<uint32_t>(static_cast<uint8_t>(ch2)) << 16) \
    | (static_cast<uint32_t>(static_cast<uint8_t>(ch3)) << 24))
#endif /* defined(MAKEFOURCC) */

std::map<int, std::string> ToupBase::errCodes =
{
    {0x00000000, "Success"},
    {0x00000001, "Yet another success"},
    {0x8000ffff, "Catastrophic failure"},
    {0x80004001, "Not supported or not implemented"},
    {0x80070005, "Permission denied"},
    {0x8007000e, "Out of memory"},
    {0x80070057, "One or more arguments are not valid"},
    {0x80004003, "Pointer that is not valid"},
    {0x80004005, "Generic failure"},
    {0x8001010e, "Call function in the wrong thread"},
    {0x8007001f, "Device not functioning"},
    {0x800700aa, "The requested resource is in use"},
    {0x8000000a, "The data necessary to complete this operation is not yet available"},
    {0x8001011f, "This operation returned because the timeout period expired"}
};

std::string ToupBase::errorCodes(int rc)
{
	const std::map<int, std::string>::iterator it = errCodes.find(rc);
	if (it != errCodes.end())
		return it->second;
	else
	{
		char str[256];
		sprintf(str, "Unknown error: 0x%08x", rc);
		return std::string(str);
	}
}

static class Loader
{
    std::deque<std::unique_ptr<ToupBase>> cameras;
    XP(DeviceV2) pCameraInfo[CP(MAX)];
public:
    Loader()
    {
        int iConnectedCamerasCount = FP(EnumV2(pCameraInfo));
        if (iConnectedCamerasCount <= 0)
        {
            IDLog("No Toupcam detected. Power on?");
            return;
        }

        for (int i = 0; i < iConnectedCamerasCount; i++)
        {
            if (0 == (CP(FLAG_FILTERWHEEL) & pCameraInfo[i].model->flag))
                cameras.push_back(std::unique_ptr<ToupBase>(new ToupBase(&pCameraInfo[i])));
        }
    }
} loader;

ToupBase::ToupBase(const XP(DeviceV2) *instance) : m_Instance(instance)
{
    setVersion(TOUPBASE_VERSION_MAJOR, TOUPBASE_VERSION_MINOR);

    m_WEtimerID = m_NStimerID = -1;

    snprintf(this->m_name, MAXINDIDEVICE, "%s %s", getDefaultName(), instance->displayname);
    setDeviceName(this->m_name);

    m_CaptureTimeout.callOnTimeout(std::bind(&ToupBase::captureTimeoutHandler, this));
    m_CaptureTimeout.setSingleShot(true);
	
    if (m_Instance->model->flag & (CP(FLAG_MONO)))
        m_MonoCamera = true;	
}

ToupBase::~ToupBase()
{
    m_CaptureTimeout.stop();
}

const char *ToupBase::getDefaultName()
{
    return DNAME;
}

bool ToupBase::initProperties()
{
    INDI::CCD::initProperties();

	int nsp;
    ///////////////////////////////////////////////////////////////////////////////////
    /// Binning Mode Control
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&m_BinningModeS[TC_BINNING_AVG], "TC_BINNING_AVG", "AVG", ISS_OFF);
    IUFillSwitch(&m_BinningModeS[TC_BINNING_ADD], "TC_BINNING_ADD", "Add", ISS_ON);
    IUFillSwitchVector(&m_BinningModeSP, m_BinningModeS, 2, getDeviceName(), "CCD_BINNING_MODE", "Binning Mode", IMAGE_SETTINGS_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Cooler Control
    /// N.B. Some cameras starts with cooling immediately if powered.
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&m_CoolerS[0], "COOLER_ON", "ON", ISS_ON);
    IUFillSwitch(&m_CoolerS[1], "COOLER_OFF", "OFF", ISS_OFF);
    IUFillSwitchVector(&m_CoolerSP, m_CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0, IPS_BUSY);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Controls
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&m_ControlN[TC_GAIN], "Gain", "Gain", "%.f", 0, 400, 10, 0);
    IUFillNumber(&m_ControlN[TC_CONTRAST], "Contrast", "Contrast", "%.f", CP(CONTRAST_MIN), CP(CONTRAST_MAX), 1, CP(CONTRAST_DEF));
    if (m_MonoCamera)
		nsp = 6;
	else
	{
		nsp = 8;
		IUFillNumber(&m_ControlN[TC_HUE], "Hue", "Hue", "%.f", CP(HUE_MIN), CP(HUE_MAX), 1, CP(HUE_DEF));
		IUFillNumber(&m_ControlN[TC_SATURATION], "Saturation", "Saturation", "%.f", CP(SATURATION_MIN), CP(SATURATION_MAX), 1, CP(SATURATION_DEF));
	}
    IUFillNumber(&m_ControlN[TC_BRIGHTNESS], "Brightness", "Brightness", "%.f", CP(BRIGHTNESS_MIN), CP(BRIGHTNESS_MAX), 1, 0);
    IUFillNumber(&m_ControlN[TC_GAMMA], "Gamma", "Gamma", "%.f", CP(GAMMA_MIN), CP(GAMMA_MAX), 1, CP(GAMMA_DEF));
    IUFillNumber(&m_ControlN[TC_SPEED], "Speed", "Speed", "%.f", 0, m_Instance->model->maxspeed, 1, 0);
    IUFillNumber(&m_ControlN[TC_FRAMERATE_LIMIT], "FPS Limit", "FPS Limit", "%.f", 0, 63, 1, 0);
    IUFillNumberVector(&m_ControlNP, m_ControlN, nsp, getDeviceName(), "CCD_CONTROLS", "Controls", CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // Black Balance RGB
    ///////////////////////////////////////////////////////////////////////////////////
    if (m_MonoCamera)
	{
		nsp = 1;
		IUFillNumber(&m_BlackBalanceN[TC_BLACK_R], "TC_BLACK", "Value", "%.f", 0, 255, 1, 0);
	}
	else
	{
		nsp = 3;
		IUFillNumber(&m_BlackBalanceN[TC_BLACK_R], "TC_BLACK_R", "Red", "%.f", 0, 255, 1, 0);
		IUFillNumber(&m_BlackBalanceN[TC_BLACK_G], "TC_BLACK_G", "Green", "%.f", 0, 255, 1, 0);
		IUFillNumber(&m_BlackBalanceN[TC_BLACK_B], "TC_BLACK_B", "Blue", "%.f", 0, 255, 1, 0);
	}
    IUFillNumberVector(&BlackBalanceNP, m_BlackBalanceN, nsp, getDeviceName(), "CCD_BLACK_BALANCE", "Black Balance", LEVEL_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // Black Level RAW
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&m_OffsetN[TC_OFFSET], "OFFSET", "Value", "%.f", 0, 255, 1, 0);
    IUFillNumberVector(&m_OffsetNP, m_OffsetN, 1, getDeviceName(), "CCD_OFFSET", "Offset", CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // R/G/B/Y levels
    ///////////////////////////////////////////////////////////////////////////////////
    if (m_MonoCamera)
	{
		nsp = 2;
		IUFillNumber(&m_LevelRangeN[TC_LO_Y], "TC_LO_Y", "Low", "%.f", 0, 255, 1, 0);
		IUFillNumber(&m_LevelRangeN[TC_HI_Y], "TC_HI_Y", "High", "%.f", 0, 255, 1, 0);		
	}
	else
	{
		nsp = 8;
		IUFillNumber(&m_LevelRangeN[TC_LO_R], "TC_LO_R", "Low Red", "%.f", 0, 255, 1, 0);
		IUFillNumber(&m_LevelRangeN[TC_HI_R], "TC_HI_R", "High Red", "%.f", 0, 255, 1, 0);
		IUFillNumber(&m_LevelRangeN[TC_LO_G], "TC_LO_G", "Low Green", "%.f", 0, 255, 1, 0);
		IUFillNumber(&m_LevelRangeN[TC_HI_G], "TC_HI_G", "High Green", "%.f", 0, 255, 1, 0);
		IUFillNumber(&m_LevelRangeN[TC_LO_B], "TC_LO_B", "Low Blue", "%.f", 0, 255, 1, 0);
		IUFillNumber(&m_LevelRangeN[TC_HI_B], "TC_HI_B", "High Blue", "%.f", 0, 255, 1, 0);
		IUFillNumber(&m_LevelRangeN[TC_LO_Y], "TC_LO_Y", "Low Gray", "%.f", 0, 255, 1, 0);
		IUFillNumber(&m_LevelRangeN[TC_HI_Y], "TC_HI_Y", "High Gray", "%.f", 0, 255, 1, 0);
	}
    IUFillNumberVector(&m_LevelRangeNP, m_LevelRangeN, nsp, getDeviceName(), "CCD_LEVEL_RANGE", "Level Range", LEVEL_TAB, IP_RW, 60, IPS_IDLE);

	if (m_MonoCamera == false)
    {
		///////////////////////////////////////////////////////////////////////////////////
		// Auto Controls
		///////////////////////////////////////////////////////////////////////////////////
		IUFillSwitch(&m_AutoControlS[TC_AUTO_TINT], "TC_AUTO_TINT", "White Balance Tint", ISS_OFF);
		IUFillSwitch(&m_AutoControlS[TC_AUTO_WB], "TC_AUTO_WB", "White Balance RGB", ISS_OFF);
		IUFillSwitch(&m_AutoControlS[TC_AUTO_BB], "TC_AUTO_BB", "Black Balance", ISS_OFF);
		IUFillSwitchVector(&m_AutoControlSP, m_AutoControlS, 3, getDeviceName(), "CCD_AUTO_CONTROL", "Auto", CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);
	}
	
    ///////////////////////////////////////////////////////////////////////////////////
    // Auto Exposure
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&m_AutoExposureS[TC_AUTO_EXPOSURE_ON], "TC_AUTO_EXPOSURE_ON", "Enabled", ISS_ON);
    IUFillSwitch(&m_AutoExposureS[TC_AUTO_EXPOSURE_OFF], "TC_AUTO_EXPOSURE_OFF", "Disabled", ISS_OFF);
    IUFillSwitchVector(&m_AutoExposureSP, m_AutoExposureS, 2, getDeviceName(), "CCD_AUTO_EXPOSURE", "Auto Exp.", CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

	if (m_MonoCamera == false)
    {
		///////////////////////////////////////////////////////////////////////////////////
		// White Balance - Temp/Tint
		///////////////////////////////////////////////////////////////////////////////////
		IUFillNumber(&m_WBTempTintN[TC_WB_TEMP], "TC_WB_TEMP", "Temp", "%.f", CP(TEMP_MIN), CP(TEMP_MAX), 1000, CP(TEMP_DEF));
		IUFillNumber(&m_WBTempTintN[TC_WB_TINT], "TC_WB_TINT", "Tint", "%.f", CP(TINT_MIN), CP(TINT_MAX), 100, CP(TINT_DEF));
		IUFillNumberVector(&m_WBTempTintNP, m_WBTempTintN, 2, getDeviceName(), "TC_WB_TT", "WB #1", LEVEL_TAB, IP_RW, 60, IPS_IDLE);

		///////////////////////////////////////////////////////////////////////////////////
		// White Balance - RGB
		///////////////////////////////////////////////////////////////////////////////////
		IUFillNumber(&m_WBRGBN[TC_WB_R], "TC_WB_R", "Red", "%.f", CP(WBGAIN_MIN), CP(WBGAIN_MAX), 10, CP(WBGAIN_DEF));
		IUFillNumber(&m_WBRGBN[TC_WB_G], "TC_WB_G", "Green", "%.f", CP(WBGAIN_MIN), CP(WBGAIN_MAX), 10, CP(WBGAIN_DEF));
		IUFillNumber(&m_WBRGBN[TC_WB_B], "TC_WB_B", "Blue", "%.f", CP(WBGAIN_MIN), CP(WBGAIN_MAX), 10, CP(WBGAIN_DEF));
		IUFillNumberVector(&m_WBRGBNP, m_WBRGBN, 3, getDeviceName(), "TC_WB_RGB", "WB #2", LEVEL_TAB, IP_RW, 60, IPS_IDLE);

		///////////////////////////////////////////////////////////////////////////////////
		/// White Balance - Auto
		///////////////////////////////////////////////////////////////////////////////////
		IUFillSwitch(&m_WBAutoS[TC_AUTO_WB_TT], "TC_AUTO_WB_TT", "Temp/Tint", ISS_ON);
		IUFillSwitch(&m_WBAutoS[TC_AUTO_WB_RGB], "TC_AUTO_WB_RGB", "RGB", ISS_OFF);
		IUFillSwitchVector(&m_WBAutoSP, m_WBAutoS, 2, getDeviceName(), "TC_AUTO_WB", "Default WB Mode", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
	}
	
    ///////////////////////////////////////////////////////////////////////////////////
    /// Analog Digital Converter
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&m_ADCN[0], "ADC_BITDEPTH", "Bit Depth", "%.f", 8, 32, 0, 8);
    IUFillNumberVector(&m_ADCNP, m_ADCN, 1, getDeviceName(), "ADC", "ADC", IMAGE_INFO_TAB, IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Timeout Factor
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&TimeoutFactorN[0], "VALUE", "Factor", "%.f", 1, 10, 1, 1.2);
    IUFillNumberVector(&m_TimeoutFactorNP, TimeoutFactorN, 1, getDeviceName(), "TIMEOUT_FACTOR", "Timeout", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

	if (m_Instance->model->flag & (CP(FLAG_CG) | CP(FLAG_CGHDR)))
    {
		///////////////////////////////////////////////////////////////////////////////////
		/// Gain Conversion Mode
		///////////////////////////////////////////////////////////////////////////////////
		int nsp = 2;
		IUFillSwitch(&m_GainConversionS[GAIN_LOW], "GAIN_LOW", "Low", ISS_OFF);
		IUFillSwitch(&m_GainConversionS[GAIN_HIGH], "GAIN_HIGH", "High", ISS_OFF);
		if (m_Instance->model->flag & CP(FLAG_CGHDR))
		{
			IUFillSwitch(&m_GainConversionS[GAIN_HDR], "GAIN_HDR", "HDR", ISS_OFF);
			++nsp;
		}
		IUFillSwitchVector(&m_GainConversionSP, m_GainConversionS, nsp, getDeviceName(), "TC_HCG_CONTROL", "Gain Conversion", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
	}

    ///////////////////////////////////////////////////////////////////////////////////
    /// Low Noise Mode
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&m_LowNoiseS[INDI_ENABLED], "INDI_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&m_LowNoiseS[INDI_DISABLED], "INDI_DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&m_LowNoiseSP, m_LowNoiseS, 2, getDeviceName(), "TC_LOW_NOISE_CONTROL", "Low Noise Mode", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);


    ///////////////////////////////////////////////////////////////////////////////////
    /// High Fullwell Mode
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&m_HighFullwellModeS[INDI_ENABLED], "INDI_ENABLED", "Enabled", ISS_OFF);
    IUFillSwitch(&m_HighFullwellModeS[INDI_DISABLED], "INDI_DISABLED", "Disabled", ISS_ON);
    IUFillSwitchVector(&m_HighFullwellModeSP, m_HighFullwellModeS, 2, getDeviceName(), "TC_HIGHFULLWELL_CONTROL", "High Fullwell Mode", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Heat Control
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&m_HeatUpS[TC_HEAT_OFF], "TC_HEAT_OFF", "Off", ISS_ON);
    IUFillSwitch(&m_HeatUpS[TC_HEAT_ON], "TC_HEAT_ON", "On", ISS_OFF);
    IUFillSwitch(&m_HeatUpS[TC_HEAT_MAX], "TC_HEAT_MAX", "Max", ISS_OFF);
    IUFillSwitchVector(&m_HeatUpSP, m_HeatUpS, 2, getDeviceName(), "TC_HEAT_CONTROL", "Heat", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Fan Control
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&m_FanControlS[TC_FAN_ON], "TC_FAN_ON", "On", ISS_ON);
    IUFillSwitch(&m_FanControlS[TC_FAN_OFF], "TC_FAN_OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&m_FanControlSP, m_FanControlS, 2, getDeviceName(), "TC_FAN_CONTROL", "Fan", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Fan Speed
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitchVector(&m_FanSpeedSP, m_FanSpeedS, 0, getDeviceName(), "TC_FAN_Speed", "Fan Speed", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Video Format
    ///////////////////////////////////////////////////////////////////////////////////
    /// RGB Mode with RGB24 color
    IUFillSwitch(&m_VideoFormatS[TC_VIDEO_COLOR_RGB], "TC_VIDEO_COLOR_RGB", "RGB", ISS_OFF);
    /// Raw mode (8 to 16 bit)
    IUFillSwitch(&m_VideoFormatS[TC_VIDEO_COLOR_RAW], "TC_VIDEO_COLOR_RAW", "Raw", ISS_OFF);
    IUFillSwitchVector(&m_VideoFormatSP, m_VideoFormatS, 2, getDeviceName(), "CCD_VIDEO_FORMAT", "Format", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Resolution
    ///////////////////////////////////////////////////////////////////////////////////
    for (unsigned i = 0; i < m_Instance->model->preview; i++)
    {
        char label[MAXINDILABEL] = {0};
        snprintf(label, MAXINDILABEL, "%d x %d", m_Instance->model->res[i].width, m_Instance->model->res[i].height);
        IUFillSwitch(&m_ResolutionS[i], label, label, ISS_OFF);
    }	
    IUFillSwitchVector(&m_ResolutionSP, m_ResolutionS, m_Instance->model->preview, getDeviceName(), "CCD_RESOLUTION", "Resolution", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUGetConfigOnSwitchIndex(getDeviceName(), m_ResolutionSP.name, &m_ConfigResolutionIndex);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Firmware
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillText(&m_FirmwareT[TC_FIRMWARE_SERIAL], "Serial", "Serial", nullptr);
    IUFillText(&m_FirmwareT[TC_FIRMWARE_SW_VERSION], "Software", "Software", nullptr);
    IUFillText(&m_FirmwareT[TC_FIRMWARE_HW_VERSION], "Hardware", "Hardware", nullptr);
    IUFillText(&m_FirmwareT[TC_FIRMWARE_DATE], "Date", "Date", nullptr);
    IUFillText(&m_FirmwareT[TC_FIRMWARE_REV], "Revision", "Revision", nullptr);
    IUFillTextVector(&m_FirmwareTP, m_FirmwareT, 5, getDeviceName(), "Firmware", "Firmware", "Firmware", IP_RO, 0, IPS_IDLE);

    IUFillText(&m_SDKVersionT[0], "VERSION", "Version", nullptr);
    IUFillTextVector(&m_SDKVersionTP, m_SDKVersionT, 1, getDeviceName(), "SDK", "SDK", "Firmware", IP_RO, 0, IPS_IDLE);

    PrimaryCCD.setMinMaxStep("CCD_BINNING", "HOR_BIN", 1, 4, 1, false);
    PrimaryCCD.setMinMaxStep("CCD_BINNING", "VER_BIN", 1, 4, 1, false);

    addAuxControls();

    return true;
}

bool ToupBase::updateProperties()
{
    // Setup parameters and reset capture format.
    if (isConnected())
    {
        // Clear format
        CaptureFormatSP.resize(0);
        m_CaptureFormats.clear();

        // Get parameters from camera
        setupParams();
    }

    INDI::CCD::updateProperties();

    if (isConnected())
    {
        if (HasCooler())
            defineProperty(&m_CoolerSP);
        // Even if there is no cooler, we define temperature property as READ ONLY
        else if (m_Instance->model->flag & CP(FLAG_GETTEMPERATURE))
        {
            TemperatureNP.p = IP_RO;
            defineProperty(&TemperatureNP);
        }

        if (m_Instance->model->flag & CP(FLAG_FAN))
        {
            defineProperty(&m_FanControlSP);
            defineProperty(&m_FanSpeedSP);
        }

        if (m_MonoCamera == false)
            defineProperty(&m_WBAutoSP);

        defineProperty(&m_TimeoutFactorNP);
        defineProperty(&m_ControlNP);
        defineProperty(&m_AutoControlSP);
        defineProperty(&m_AutoExposureSP);
        defineProperty(&m_VideoFormatSP);
        defineProperty(&m_ResolutionSP);
        defineProperty(&m_ADCNP);

        if (m_HasHighFullwellMode)
            defineProperty(&m_HighFullwellModeSP);

        if (m_HasLowNoise)
            defineProperty(&m_LowNoiseSP);

        if (m_HasHeatUp)
            defineProperty(&m_HeatUpSP);

        if (m_Instance->model->flag & (CP(FLAG_CG) | CP(FLAG_CGHDR)))
            defineProperty(&m_GainConversionSP);

        // Binning mode
        defineProperty(&m_BinningModeSP);

        // Levels
        defineProperty(&m_LevelRangeNP);
        defineProperty(&BlackBalanceNP);
        defineProperty(&m_OffsetNP);

        // Balance
        if (m_MonoCamera == false)
        {
            defineProperty(&m_WBTempTintNP);
            defineProperty(&m_WBRGBNP);
        }

        // Firmware
        defineProperty(&m_FirmwareTP);
        defineProperty(&m_SDKVersionTP);
    }
    else
    {
        if (HasCooler())
            deleteProperty(m_CoolerSP.name);
        else
            deleteProperty(TemperatureNP.name);

        if (m_Instance->model->flag & CP(FLAG_FAN))
        {
            deleteProperty(m_FanControlSP.name);
            deleteProperty(m_FanSpeedSP.name);
        }

        if (m_MonoCamera == false)
            deleteProperty(m_WBAutoSP.name);

        deleteProperty(m_TimeoutFactorNP.name);
        deleteProperty(m_ControlNP.name);
        deleteProperty(m_AutoControlSP.name);
        deleteProperty(m_AutoExposureSP.name);
        deleteProperty(m_VideoFormatSP.name);
        deleteProperty(m_ResolutionSP.name);
        deleteProperty(m_ADCNP.name);

        if (m_HasLowNoise)
            deleteProperty(m_LowNoiseSP.name);
        
        if (m_HasHighFullwellMode)
            deleteProperty(m_HighFullwellModeSP.name);

        if (m_HasHeatUp)
            deleteProperty(m_HeatUpSP.name);

        if (m_Instance->model->flag & (CP(FLAG_CG) | CP(FLAG_CGHDR)))
            deleteProperty(m_GainConversionSP.name);

        deleteProperty(m_BinningModeSP.name);
        deleteProperty(m_LevelRangeNP.name);
        deleteProperty(BlackBalanceNP.name);
        deleteProperty(m_OffsetNP.name);

        if (m_MonoCamera == false)
        {
            deleteProperty(m_WBTempTintNP.name);
            deleteProperty(m_WBRGBNP.name);
        }

        deleteProperty(m_FirmwareTP.name);
        deleteProperty(m_SDKVersionTP.name);
    }

    return true;
}

bool ToupBase::Connect()
{
    LOGF_DEBUG("Attempting to open %s with ID %s using SDK version: %s", m_name, m_Instance->id, FP(Version()));

    if (isSimulation() == false)
    {
        std::string fullID = m_Instance->id;
        // For RGB White Balance Mode, we need to add @ at the beginning as per docs.
        if (m_MonoCamera == false && m_WBAutoS[TC_AUTO_WB_RGB].s == ISS_ON)
            fullID = "@" + fullID;

        m_CameraHandle = FP(Open(fullID.c_str()));
    }

    if (m_CameraHandle == nullptr)
    {
        LOG_ERROR("Error connecting to the camera");
        return false;
    }

    uint32_t cap = CCD_CAN_BIN | CCD_CAN_ABORT | CCD_HAS_STREAMING;
    // If raw format is support then we have bayer
    if (m_MonoCamera == false)
        cap |= CCD_HAS_BAYER;

    if (m_Instance->model->flag & CP(FLAG_BINSKIP_SUPPORTED))
        LOG_DEBUG("Bin-Skip supported");

    // Hardware ROI really needed? Check later
    if (m_Instance->model->flag & CP(FLAG_ROI_HARDWARE))
    {
        LOG_DEBUG("Hardware ROI supported");
        cap |= CCD_CAN_SUBFRAME;
    }

    if (m_Instance->model->flag & CP(FLAG_TEC_ONOFF))
    {
        LOG_DEBUG("TEC control enabled");
        cap |= CCD_HAS_COOLER;
    }

    if (m_Instance->model->flag & CP(FLAG_ST4))
    {
        LOG_DEBUG("ST4 guiding enabled");
        cap |= CCD_HAS_ST4_PORT;
    }

    SetCCDCapability(cap);

    LOGF_DEBUG("maxSpeed: %d, preview: %d, still: %d, maxFanSpeed %d", m_Instance->model->maxspeed, m_Instance->model->preview, m_Instance->model->still, m_Instance->model->maxfanspeed);

    // Get min/max exposures
    uint32_t min = 0, max = 0, current = 0;
    FP(get_ExpTimeRange(m_CameraHandle, &min, &max, &current));
    LOGF_DEBUG("Exposure Time Range (us): Min %u, Max %u, Default %u", min, max, current);
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", min / 1000000.0, max / 1000000.0, 0, false);

    // Auto Exposure
    int autoExposure = 0;
    FP(get_AutoExpoEnable(m_CameraHandle, &autoExposure));
    m_AutoExposureS[TC_AUTO_EXPOSURE_ON].s = autoExposure ? ISS_ON : ISS_OFF;
    m_AutoExposureS[TC_AUTO_EXPOSURE_OFF].s = autoExposure ? ISS_OFF : ISS_ON;
    m_AutoExposureSP.s = IPS_OK;

    PrimaryCCD.setBin(1, 1);
    // Success!
    LOGF_INFO("%s is online. Retrieving basic data", getDeviceName());

    return true;
}

bool ToupBase::Disconnect()
{
    stopTimerNS();
    stopTimerWE();

    FP(Close(m_CameraHandle));

    return true;
}

void ToupBase::setupParams()
{
    HRESULT rc = 0;

    FP(put_Option(m_CameraHandle, CP(OPTION_NOFRAME_TIMEOUT), 1));

    // Get Firmware Info
    char firmwareBuffer[32] = {0};
    uint16_t pRevision = 0;
    FP(get_SerialNumber(m_CameraHandle, firmwareBuffer));
    IUSaveText(&m_FirmwareT[TC_FIRMWARE_SERIAL], firmwareBuffer);
    FP(get_FwVersion(m_CameraHandle, firmwareBuffer));
    IUSaveText(&m_FirmwareT[TC_FIRMWARE_SW_VERSION], firmwareBuffer);
    FP(get_HwVersion(m_CameraHandle, firmwareBuffer));
    IUSaveText(&m_FirmwareT[TC_FIRMWARE_HW_VERSION], firmwareBuffer);
    FP(get_ProductionDate(m_CameraHandle, firmwareBuffer));
    IUSaveText(&m_FirmwareT[TC_FIRMWARE_DATE], firmwareBuffer);
    FP(get_Revision(m_CameraHandle, &pRevision));
    snprintf(firmwareBuffer, 32, "%d", pRevision);
    IUSaveText(&m_FirmwareT[TC_FIRMWARE_REV], firmwareBuffer);
    m_FirmwareTP.s = IPS_OK;

    // SDK Version
    IUSaveText(&m_SDKVersionT[0], FP(Version()));
    m_SDKVersionTP.s = IPS_OK;

    // Max supported bit depth
    m_MaxBitDepth = FP(get_MaxBitDepth(m_CameraHandle));
    LOGF_DEBUG("Max bit depth: %d", m_MaxBitDepth);
    m_ADCN[0].value = m_MaxBitDepth;

    m_BitsPerPixel = 8;
    int nVal = 0;

    // Check if mono only camera
    if (m_MonoCamera)
    {
        IUFillSwitch(&m_VideoFormatS[TC_VIDEO_MONO_8], "TC_VIDEO_MONO_8", "Mono 8", ISS_OFF);
        /// RGB Mode but 16 bits grayscale
        IUFillSwitch(&m_VideoFormatS[TC_VIDEO_MONO_16], "TC_VIDEO_MONO_16", "Mono 16", ISS_OFF);
        LOG_DEBUG("Mono camera detected");

        rc = FP(put_Option(m_CameraHandle, CP(OPTION_RAW), 1));
        LOGF_DEBUG("OPTION_RAW 1. rc: %s", errorCodes(rc).c_str());

        CaptureFormat mono16 = {"INDI_MONO_16", "Mono 16", 16, false};
        CaptureFormat mono8 = {"INDI_MONO_8", "Mono 8", 8, false};
        if (m_Instance->model->flag & BITDEPTH_FLAG)
        {
            // enable bitdepth
            rc = FP(put_Option(m_CameraHandle, CP(OPTION_BITDEPTH), 1));
            LOGF_DEBUG("OPTION_BITDEPTH 1. rc: %s", errorCodes(rc).c_str());
            m_BitsPerPixel = 16;
            m_VideoFormatS[TC_VIDEO_MONO_16].s = ISS_ON;
            m_CurrentVideoFormat = TC_VIDEO_MONO_16;
            mono16.isDefault = true;
        }
        else
        {
            m_BitsPerPixel = 8;
            m_VideoFormatS[TC_VIDEO_MONO_8].s = ISS_ON;
            m_CurrentVideoFormat = TC_VIDEO_MONO_8;
            mono8.isDefault = true;
        }

        m_CameraPixelFormat = INDI_MONO;
        m_Channels = 1;

        addCaptureFormat(mono8);
        addCaptureFormat(mono16);
        LOGF_DEBUG("Bits Per Pixel: %d, Video Mode: %s", m_BitsPerPixel, m_VideoFormatS[TC_VIDEO_MONO_8].s == ISS_ON ? "Mono 8-bit" : "Mono 16-bit");
    }
    // Color Camera
    else
    {
		bool RAWHighDepthSupport = false;
        if (m_Instance->model->flag & BITDEPTH_FLAG)
        {
            // enable bitdepth
            FP(put_Option(m_CameraHandle, CP(OPTION_BITDEPTH), 1));
            m_BitsPerPixel = 16;
            RAWHighDepthSupport = true;
            LOG_DEBUG("RAW Bit Depth: 16");
        }

        // Get RAW/RGB Mode
        int cameraDataMode = 0;
        IUResetSwitch(&m_VideoFormatSP);
        rc = FP(get_Option(m_CameraHandle, CP(OPTION_RAW), &cameraDataMode));
        LOGF_DEBUG("OPTION_RAW. rc: %s, Value: %d", errorCodes(rc).c_str(), cameraDataMode);

        CaptureFormat rgb = {"INDI_RGB", "RGB", 8};
        CaptureFormat raw = {"INDI_RAW", RAWHighDepthSupport ? "RAW 16" : "RAW 8", static_cast<uint8_t>(RAWHighDepthSupport ? 16 : 8)};

        // Color RAW
        if (cameraDataMode == TC_VIDEO_COLOR_RAW)
        {
            m_VideoFormatS[TC_VIDEO_COLOR_RAW].s = ISS_ON;
            m_Channels = 1;
            LOG_INFO("Video Mode RAW detected");
            raw.isDefault = true;

            // Get RAW Format
            IUSaveText(&BayerT[2], getBayerString());
        }
        // Color RGB
        else
        {
            LOG_INFO("Video Mode RGB detected");
            m_VideoFormatS[TC_VIDEO_COLOR_RGB].s = ISS_ON;
            m_Channels = 3;
            m_CameraPixelFormat = INDI_RGB;
            m_BitsPerPixel = 8;
            rgb.isDefault = true;

            SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
        }

        addCaptureFormat(rgb);
        addCaptureFormat(raw);

        LOGF_DEBUG("Bits Per Pixel: %d, Video Mode: %s", m_BitsPerPixel, m_VideoFormatS[TC_VIDEO_COLOR_RGB].s == ISS_ON ? "RGB" : "RAW");
    }

    PrimaryCCD.setNAxis(m_Channels == 1 ? 2 : 3);

    // Fan Control
    if (m_Instance->model->flag & CP(FLAG_FAN))
    {
        int fan = 0;
        FP(get_Option(m_CameraHandle, CP(OPTION_FAN), &fan));
        LOGF_DEBUG("Fan is %s", fan == 0 ? "Off" : "On");
        IUResetSwitch(&m_FanControlSP);
        m_FanControlS[TC_FAN_ON].s = fan == 0 ? ISS_OFF : ISS_ON;
        m_FanControlS[TC_FAN_OFF].s = fan == 0 ? ISS_ON : ISS_OFF;
        m_FanControlSP.s = (fan == 0) ? IPS_IDLE : IPS_BUSY;

        // Fan Speed
        delete[] m_FanSpeedS;
        // If Fan is OFF, then set the default one to 1x
        uint32_t activeFan = (fan == 0) ? 1 : fan;
        m_FanSpeedS = new ISwitch[m_Instance->model->maxfanspeed];
        for (uint32_t i = 0; i < m_Instance->model->maxfanspeed; i++)
        {
            char name[MAXINDINAME] = {0}, label[MAXINDINAME] = {0};
            snprintf(name, MAXINDINAME, "FAN_SPEED_%u", i + 1);
            snprintf(label, MAXINDINAME, "%ux", i + 1);
            IUFillSwitch(m_FanSpeedS + i, name, label, (activeFan == i + 1) ? ISS_ON : ISS_OFF);
        }
        m_FanSpeedSP.sp = m_FanSpeedS;
        m_FanSpeedSP.nsp = m_Instance->model->maxfanspeed;
        m_FanSpeedSP.s = IPS_OK;
    }

    // Get active resolution index
    uint32_t currentResolutionIndex = 0, finalResolutionIndex = 0;
    FP(get_eSize(m_CameraHandle, &currentResolutionIndex));
    // If we have a config resolution index, then prefer it over the current resolution index.
    finalResolutionIndex = (m_ConfigResolutionIndex >= 0 && m_ConfigResolutionIndex < m_ResolutionSP.nsp) ? m_ConfigResolutionIndex : currentResolutionIndex;
    // In case there is NO previous resolution set
    // then select the LOWER resolution on arm architecture
    // since this has less chance of failure. If the user explicitly selects any resolution
    // it would be saved in the config and this will not apply.
#if defined(__arm__) || defined (__aarch64__)
    if (m_ConfigResolutionIndex == -1)
        finalResolutionIndex = m_ResolutionSP.nsp - 1;
#endif
    m_ResolutionS[finalResolutionIndex].s = ISS_ON;

    // If final resolution index different from current, let's set it.
    if (finalResolutionIndex != currentResolutionIndex)
        FP(put_eSize(m_CameraHandle, finalResolutionIndex));

    SetCCDParams(m_Instance->model->res[finalResolutionIndex].width, m_Instance->model->res[finalResolutionIndex].height, m_BitsPerPixel, m_Instance->model->xpixsz, m_Instance->model->ypixsz);

    m_CanSnap = m_Instance->model->still > 0;
    LOGF_DEBUG("Camera snap support: %s", m_CanSnap ? "True" : "False");

    // Trigger Mode
    FP(get_Option(m_CameraHandle, CP(OPTION_TRIGGER), &nVal));
    LOGF_DEBUG("Trigger mode: %d", nVal);
    m_CurrentTriggerMode = static_cast<eTriggerMode>(nVal);

    // Set trigger mode to software
    if (m_CurrentTriggerMode != TRIGGER_SOFTWARE)
    {
        LOG_DEBUG("Setting trigger mode to software");
        rc = FP(put_Option(m_CameraHandle, CP(OPTION_TRIGGER), 1));
        if (FAILED(rc))
            LOGF_ERROR("Failed to set software trigger mode. %s", errorCodes(rc).c_str());
        else
            m_CurrentTriggerMode = TRIGGER_SOFTWARE;
    }

    // Get CCD Controls values
    int conversionGain = 0;
    rc = FP(get_Option(m_CameraHandle, CP(OPTION_CG), &conversionGain));
    LOGF_DEBUG("Conversion Gain %d, rc: %d", conversionGain, rc);
    m_GainConversionS[conversionGain].s = ISS_ON;

	uint16_t nMin = 0, nMax = 0, nDef = 0;
    // Gain
    FP(get_ExpoAGainRange(m_CameraHandle, &nMin, &nMax, &nDef));
    LOGF_DEBUG("Exposure Gain Control. Min: %u, Max: %u, Default: %u", nMin, nMax, nDef);
    m_ControlN[TC_GAIN].min = nMin;
    m_ControlN[TC_GAIN].max = nMax;
    m_ControlN[TC_GAIN].step = (m_ControlN[TC_GAIN].max - nMin) / 20.0;
    m_ControlN[TC_GAIN].value = nDef;

    // High FullWell Mode
    if (m_Instance->model->flag & CP(FLAG_HIGH_FULLWELL))
    {
        m_HasHighFullwellMode = true;
        LOG_INFO("High Full Well is possible");
    }
    else 
    {
        m_HasHighFullwellMode = false;
        LOG_INFO("High Full Well is NOT possible");
    }
	
    // Low Noise
    if (m_Instance->model->flag & CP(FLAG_LOW_NOISE))
        m_HasLowNoise = true;

    // Heat Up
    if (m_Instance->model->flag & CP(FLAG_HEAT))
        m_HasHeatUp = true;

    // Contrast
    FP(get_Contrast(m_CameraHandle, &nVal));
    LOGF_DEBUG("Contrast Control: %d", nVal);
    m_ControlN[TC_CONTRAST].value = nVal;

	if (m_MonoCamera == false)
    {
		// Hue
		rc = FP(get_Hue(m_CameraHandle, &nVal));
		LOGF_DEBUG("Hue Control: %d", nVal);
		m_ControlN[TC_HUE].value = nVal;

		// Saturation
		rc = FP(get_Saturation(m_CameraHandle, &nVal));
		LOGF_DEBUG("Saturation Control: %d", nVal);
		m_ControlN[TC_SATURATION].value = nVal;
	}

    // Brightness
    rc = FP(get_Brightness(m_CameraHandle, &nVal));
    LOGF_DEBUG("Brightness Control: %d", nVal);
    m_ControlN[TC_BRIGHTNESS].value = nVal;

    // Gamma
    rc = FP(get_Gamma(m_CameraHandle, &nVal));
    LOGF_DEBUG("Gamma Control: %d", nVal);
    m_ControlN[TC_GAMMA].value = nVal;

    // Speed
    rc = FP(get_Speed(m_CameraHandle, &nDef));
    LOGF_DEBUG("Speed Control: %d", nDef);

    // JM 2020-05-06: Reduce speed on ARM for all resolutions
#if defined(__arm__) || defined (__aarch64__)
    m_ControlN[TC_SPEED].value = 0;
    FP(put_Speed(m_CameraHandle, 0));
#else
    m_ControlN[TC_SPEED].value = nDef;
#endif
    m_ControlN[TC_SPEED].max = m_Instance->model->maxspeed;

    // Frame Rate
    int frameRateLimit = 0;
    rc = FP(get_Option(m_CameraHandle, CP(OPTION_FRAMERATE), &frameRateLimit));
    LOGF_DEBUG("Frame Rate Limit %d, rc: %d", frameRateLimit, rc);

    // JM 2019-08-19: On ARM, set frame limit to max (63) instead of 0 (unlimited)
    // since that results in failure to capture from large sensors
#ifdef __arm__
    frameRateLimit = m_ControlN[TC_FRAMERATE_LIMIT].max;
    FP(put_Option(m_CameraHandle, CP(OPTION_FRAMERATE), frameRateLimit));
#endif
    m_ControlN[TC_FRAMERATE_LIMIT].value = frameRateLimit;

    // Set Bin more for better quality over skip
    if (m_Instance->model->flag & CP(FLAG_BINSKIP_SUPPORTED))
    {
        LOG_DEBUG("Selecting BIN mode over SKIP");
        rc = FP(put_Mode(m_CameraHandle, 0));
    }

	if (m_MonoCamera == false)
    {
		// Get White Balance RGB Gain
		int aGain[3] = {0};
		rc = FP(get_WhiteBalanceGain(m_CameraHandle, aGain));
		if (SUCCEEDED(rc))
		{
			m_WBRGBN[TC_WB_R].value = aGain[TC_WB_R];
			m_WBRGBN[TC_WB_G].value = aGain[TC_WB_G];
			m_WBRGBN[TC_WB_B].value = aGain[TC_WB_B];
			LOGF_DEBUG("White Balance Gain. R: %d, G: %d, B: %d", aGain[TC_WB_R], aGain[TC_WB_G], aGain[TC_WB_B]);
		}
	}

    // Get Level Ranges
    uint16_t aLow[4] = {0}, aHigh[4] = {0};
    rc = FP(get_LevelRange(m_CameraHandle, aLow, aHigh));
    if (SUCCEEDED(rc))
    {
		if (m_MonoCamera)
		{			
			m_LevelRangeN[TC_LO_Y].value = aLow[3];			
			m_LevelRangeN[TC_HI_Y].value = aHigh[3];
		}
		else
		{
			m_LevelRangeN[TC_LO_R].value = aLow[0];
			m_LevelRangeN[TC_LO_G].value = aLow[1];
			m_LevelRangeN[TC_LO_B].value = aLow[2];
			m_LevelRangeN[TC_LO_Y].value = aLow[3];

			m_LevelRangeN[TC_HI_R].value = aHigh[0];
			m_LevelRangeN[TC_HI_G].value = aHigh[1];
			m_LevelRangeN[TC_HI_B].value = aHigh[2];
			m_LevelRangeN[TC_HI_Y].value = aHigh[3];
		}
    }

    // Get Black Balance
    uint16_t aSub[3] = {0};
    rc = FP(get_BlackBalance(m_CameraHandle, aSub));
    if (SUCCEEDED(rc))
    {
		m_BlackBalanceN[TC_BLACK_R].value = aSub[0];
		if (m_MonoCamera == false)
		{
			m_BlackBalanceN[TC_BLACK_G].value = aSub[1];
			m_BlackBalanceN[TC_BLACK_B].value = aSub[2];
		}
    }

    // Get Black Level
    // Getting the black level option from camera yields the defaut setting
    // Therefore, black level is a saved option
    // Set range of black level based on max bit depth RAW
    int bLevelStep = 1 << (m_MaxBitDepth - 8);
    m_OffsetN[TC_OFFSET].max = CP(BLACKLEVEL8_MAX) * bLevelStep;
    m_OffsetN[TC_OFFSET].step = bLevelStep;

    // Allocate memory
    allocateFrameBuffer();

    SetTimer(getCurrentPollingPeriod());
}

void ToupBase::allocateFrameBuffer()
{
    // Allocate memory
    if (m_MonoCamera)
    {
        switch (m_CurrentVideoFormat)
        {
            case TC_VIDEO_MONO_8:
                PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes());
                PrimaryCCD.setBPP(8);
                PrimaryCCD.setNAxis(2);
                Streamer->setPixelFormat(INDI_MONO, 8);
                break;

            case TC_VIDEO_MONO_16:
                PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 2);
                PrimaryCCD.setBPP(16);
                PrimaryCCD.setNAxis(2);
                Streamer->setPixelFormat(INDI_MONO, 16);
                break;
        }
    }
    else
    {
        switch (m_CurrentVideoFormat)
        {
            case TC_VIDEO_COLOR_RGB:
                // RGB24 or RGB888
                PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 3);
                PrimaryCCD.setBPP(8);
                PrimaryCCD.setNAxis(3);
                Streamer->setPixelFormat(INDI_RGB, 8);
                break;

            case TC_VIDEO_COLOR_RAW:
                PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * m_BitsPerPixel / 8);
                PrimaryCCD.setBPP(m_BitsPerPixel);
                PrimaryCCD.setNAxis(2);
                Streamer->setPixelFormat(m_CameraPixelFormat, m_BitsPerPixel);
                break;
        }
    }

    Streamer->setSize(PrimaryCCD.getXRes(), PrimaryCCD.getYRes());
}

bool ToupBase::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        //////////////////////////////////////////////////////////////////////
        /// Controls (Contrast, Brightness, Hue...etc)
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_ControlNP.name))
        {
            double oldValues[8] = {0};
            for (int i = 0; i < m_ControlNP.nnp; i++)
                oldValues[i] = m_ControlN[i].value;

            if (IUUpdateNumber(&m_ControlNP, values, names, n) < 0)
            {
                m_ControlNP.s = IPS_ALERT;
                IDSetNumber(&m_ControlNP, nullptr);
                return true;
            }

            for (uint8_t i = 0; i < m_ControlNP.nnp; i++)
            {
                if (fabs(m_ControlN[i].value - oldValues[i]) < 0.0001)
                    continue;

                int value = static_cast<int>(m_ControlN[i].value);
                switch (i)
                {
                    case TC_GAIN:
                        FP(put_ExpoAGain(m_CameraHandle, value));
                        break;

                    case TC_CONTRAST:
                        FP(put_Contrast(m_CameraHandle, value));
                        break;

                    case TC_HUE:
                        FP(put_Hue(m_CameraHandle, value));
                        break;

                    case TC_SATURATION:
                        FP(put_Saturation(m_CameraHandle, value));
                        break;

                    case TC_BRIGHTNESS:
                        FP(put_Brightness(m_CameraHandle, value));
                        break;

                    case TC_GAMMA:
                        FP(put_Gamma(m_CameraHandle, value));
                        break;

                    case TC_SPEED:
                        FP(put_Speed(m_CameraHandle, value));
                        break;

                    case TC_FRAMERATE_LIMIT:
                        FP(put_Option(m_CameraHandle, CP(OPTION_FRAMERATE), value));
                        if (value == 0)
                            LOG_INFO("FPS rate limit is set to unlimited");
                        else
                            LOGF_INFO("Limiting frame rate to %d FPS", value);
                        break;

                    default:
                        break;
                }
            }

            m_ControlNP.s = IPS_OK;
            IDSetNumber(&m_ControlNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Level Ranges
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_LevelRangeNP.name))
        {
            IUUpdateNumber(&m_LevelRangeNP, values, names, n);
            uint16_t lo[4], hi[4];
			if (m_MonoCamera)
			{
				lo[0] = lo[1] = lo[2] = lo[3] = static_cast<uint16_t>(m_LevelRangeN[TC_LO_Y].value);
				hi[0] = hi[1] = hi[2] = hi[3] = static_cast<uint16_t>(m_LevelRangeN[TC_HI_Y].value);
			}
			else
            {
                lo[0] = static_cast<uint16_t>(m_LevelRangeN[TC_LO_R].value);
                lo[1] = static_cast<uint16_t>(m_LevelRangeN[TC_LO_G].value);
                lo[2] = static_cast<uint16_t>(m_LevelRangeN[TC_LO_B].value);
                lo[3] = static_cast<uint16_t>(m_LevelRangeN[TC_LO_Y].value);
                hi[0] = static_cast<uint16_t>(m_LevelRangeN[TC_HI_R].value);
                hi[1] = static_cast<uint16_t>(m_LevelRangeN[TC_HI_G].value);
                hi[2] = static_cast<uint16_t>(m_LevelRangeN[TC_HI_B].value);
                hi[3] = static_cast<uint16_t>(m_LevelRangeN[TC_HI_Y].value);
            };

            HRESULT rc = FP(put_LevelRange(m_CameraHandle, lo, hi));
            if (SUCCEEDED(rc))
				m_LevelRangeNP.s = IPS_OK;
			else
            {
                m_LevelRangeNP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set level range. %s", errorCodes(rc).c_str());
            }

            IDSetNumber(&m_LevelRangeNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Black Balance
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, BlackBalanceNP.name))
        {
            IUUpdateNumber(&BlackBalanceNP, values, names, n);
            uint16_t aSub[3];
			if (m_MonoCamera)
				aSub[0] = aSub[1] = aSub[2] = static_cast<uint16_t>(m_BlackBalanceN[TC_BLACK_R].value);
			else
            {
                aSub[0] = static_cast<uint16_t>(m_BlackBalanceN[TC_BLACK_R].value);
                aSub[1] = static_cast<uint16_t>(m_BlackBalanceN[TC_BLACK_G].value);
                aSub[2] = static_cast<uint16_t>(m_BlackBalanceN[TC_BLACK_B].value);
            };

            HRESULT rc = FP(put_BlackBalance(m_CameraHandle, aSub));
            if (SUCCEEDED(rc))
				BlackBalanceNP.s = IPS_OK;
			else
            {
                BlackBalanceNP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set Black Balance. %s", errorCodes(rc).c_str());
            }

            IDSetNumber(&BlackBalanceNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Offset
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_OffsetNP.name))
        {
            IUUpdateNumber(&m_OffsetNP, values, names, n);
            int bLevel = static_cast<uint16_t>(m_OffsetN[TC_OFFSET].value);

            HRESULT rc = FP(put_Option(m_CameraHandle, CP(OPTION_BLACKLEVEL), m_OffsetN[TC_OFFSET].value));
            if (FAILED(rc))
            {
                m_OffsetNP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set Offset. %s", errorCodes(rc).c_str());
            }
            else
            {
                m_OffsetNP.s = IPS_OK;
                LOGF_DEBUG("Offset set to %d", bLevel);
            }

            IDSetNumber(&m_OffsetNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Temp/Tint White Balance
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_WBTempTintNP.name))
        {
            IUUpdateNumber(&m_WBTempTintNP, values, names, n);

            HRESULT rc = FP(put_TempTint(m_CameraHandle, static_cast<int>(m_WBTempTintN[TC_WB_TEMP].value), static_cast<int>(m_WBTempTintN[TC_WB_TINT].value)));
            if (SUCCEEDED(rc))
				m_WBTempTintNP.s = IPS_OK;
			else
            {
                m_WBTempTintNP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set White Balance Temperature & Tint. %s", errorCodes(rc).c_str());
            }                

            IDSetNumber(&m_WBTempTintNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// RGB White Balance
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_WBRGBNP.name))
        {
            IUUpdateNumber(&m_WBRGBNP, values, names, n);

            int aSub[3] =
            {
                static_cast<int>(m_WBRGBN[TC_WB_R].value),
                static_cast<int>(m_WBRGBN[TC_WB_G].value),
                static_cast<int>(m_WBRGBN[TC_WB_B].value),
            };

            HRESULT rc = FP(put_WhiteBalanceGain(m_CameraHandle, aSub));
            if (SUCCEEDED(rc))
				m_WBRGBNP.s = IPS_OK;
			else
            {
                m_WBRGBNP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set White Balance gain. %s", errorCodes(rc).c_str());
            }

            IDSetNumber(&m_WBRGBNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Timeout factor
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_TimeoutFactorNP.name))
        {
            IUUpdateNumber(&m_TimeoutFactorNP, values, names, n);
            m_TimeoutFactorNP.s = IPS_OK;
            IDSetNumber(&m_TimeoutFactorNP, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewNumber(dev, name, values, names, n);
}

bool ToupBase::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        //////////////////////////////////////////////////////////////////////
        /// Binning Mode Control
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_BinningModeSP.name))
        {
            IUUpdateSwitch(&m_BinningModeSP, states, names, n);
            auto mode = (m_BinningModeS[TC_BINNING_AVG].s == ISS_ON) ? TC_BINNING_AVG : TC_BINNING_ADD;
            m_BinningMode = mode;
            updateBinningMode(PrimaryCCD.getBinX(), mode);
            LOGF_DEBUG("Set Binning Mode %s", mode == TC_BINNING_AVG ? "AVG" : "ADD");
            saveConfig(true, m_BinningModeSP.name);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Cooler Control
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_CoolerSP.name))
        {
            if (IUUpdateSwitch(&m_CoolerSP, states, names, n) < 0)
            {
                m_CoolerSP.s = IPS_ALERT;
                IDSetSwitch(&m_CoolerSP, nullptr);
                return true;
            }

            activateCooler(m_CoolerS[TC_COOLER_ON].s == ISS_ON);
            saveConfig(true, m_CoolerSP.name);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Fan Speed
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_FanSpeedSP.name))
        {
            IUUpdateSwitch(&m_FanSpeedSP, states, names, n);
            m_FanSpeedSP.s = IPS_OK;
            IDSetSwitch(&m_FanSpeedSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// High Fullwell Mode
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_HighFullwellModeSP.name))
        {
            int prevIndex = IUFindOnSwitchIndex(&m_HighFullwellModeSP);
            IUUpdateSwitch(&m_HighFullwellModeSP, states, names, n);

            if (m_HighFullwellModeS[TC_HIGHFULLWELL_ON].s == ISS_ON)
            {
                HRESULT rc = FP(put_Option(m_CameraHandle, CP(OPTION_HIGH_FULLWELL), 1));
                if (FAILED(rc))
                {
                    LOGF_ERROR("Failed to set High Full Well Mode %s. Error (%s)", m_HighFullwellModeS[INDI_ENABLED].s == ISS_ON ? "on" : "off", errorCodes(rc).c_str());
                    m_HighFullwellModeSP.s = IPS_ALERT;
                    IUResetSwitch(&m_HighFullwellModeSP);
                    m_HighFullwellModeS[prevIndex].s = ISS_ON;
                }
                else
                {
                    LOG_INFO("Set High Full Well Mode to ON");
                    m_HighFullwellModeSP.s = IPS_OK;
                }

                IDSetSwitch(&m_HighFullwellModeSP, nullptr);
            }
            else
            {
                HRESULT rc = FP(put_Option(m_CameraHandle, CP(OPTION_HIGH_FULLWELL), 0));
                if (FAILED(rc))
                {
                    LOGF_ERROR("Failed to set high Full Well Mode %s. Error (%s)", m_HighFullwellModeS[INDI_ENABLED].s == ISS_ON ? "on" : "off", errorCodes(rc).c_str());
                    m_HighFullwellModeSP.s = IPS_ALERT;
                    IUResetSwitch(&m_HighFullwellModeSP);
                    m_HighFullwellModeS[prevIndex].s = ISS_ON;
                }
                else
                {
                    LOG_INFO("Set High Full Well Mode to OFF");
                    m_HighFullwellModeSP.s = IPS_OK;
                }

                IDSetSwitch(&m_HighFullwellModeSP, nullptr);                
            }
            return true;
        }
 
        //////////////////////////////////////////////////////////////////////
        /// Low Noise
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_LowNoiseSP.name))
        {
            int prevIndex = IUFindOnSwitchIndex(&m_LowNoiseSP);
            IUUpdateSwitch(&m_LowNoiseSP, states, names, n);
            HRESULT rc = FP(put_Option(m_CameraHandle, CP(OPTION_LOW_NOISE), m_LowNoiseS[INDI_ENABLED].s));
            if (SUCCEEDED(rc))
				m_LowNoiseSP.s = IPS_OK;
			else
            {
                LOGF_ERROR("Failed to set low noise mode %s. Error (%s)", m_LowNoiseS[INDI_ENABLED].s == ISS_ON ? "on" : "off", errorCodes(rc).c_str());
                m_LowNoiseSP.s = IPS_ALERT;
                IUResetSwitch(&m_LowNoiseSP);
                m_LowNoiseS[prevIndex].s = ISS_ON;
            }

            IDSetSwitch(&m_LowNoiseSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Heat Control
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_HeatUpSP.name))
        {
            int prevIndex = IUFindOnSwitchIndex(&m_HeatUpSP);
            IUUpdateSwitch(&m_HeatUpSP, states, names, n);
            HRESULT rc = 0;
            if (m_HeatUpS[TC_HEAT_OFF].s == ISS_ON)
                rc = FP(put_Option(m_CameraHandle, CP(OPTION_HEAT), 0));
            else if (m_HeatUpS[TC_HEAT_ON].s == ISS_ON)
            {
                // Max heat off
                FP(put_Option(m_CameraHandle, CP(OPTION_HEAT_MAX), 0));
                // Regular heater on
                rc = FP(put_Option(m_CameraHandle, CP(OPTION_HEAT), 1));
            }
            else
            {
                // Regular heater on
                FP(put_Option(m_CameraHandle, CP(OPTION_HEAT), 1));
                // Max heat on
                rc = FP(put_Option(m_CameraHandle, CP(OPTION_HEAT_MAX), 1));
            }
            if (SUCCEEDED(rc))
				m_HeatUpSP.s = IPS_OK;
			else
            {
                LOGF_ERROR("Failed to set heat mode. Error (%s)", errorCodes(rc).c_str());
                m_HeatUpSP.s = IPS_ALERT;
                IUResetSwitch(&m_HeatUpSP);
                m_HeatUpS[prevIndex].s = ISS_ON;
            }

            IDSetSwitch(&m_HeatUpSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Fan Control
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_FanControlSP.name))
        {
            int prevIndex = IUFindOnSwitchIndex(&m_FanControlSP);
            IUUpdateSwitch(&m_FanControlSP, states, names, n);
            HRESULT rc = FP(put_Option(m_CameraHandle, CP(OPTION_FAN), m_FanControlS[0].s == ISS_ON ? IUFindOnSwitchIndex(&m_FanSpeedSP) + 1 : 0 ));
            if (SUCCEEDED(rc))
				m_FanControlSP.s = (m_FanControlS[0].s == ISS_ON) ? IPS_BUSY : IPS_IDLE;
			else
            {
                LOGF_ERROR("Failed to turn the fan %s. Error (%s)", m_FanControlS[0].s == ISS_ON ? "on" : "off", errorCodes(rc).c_str());
                m_FanControlSP.s = IPS_ALERT;
                IUResetSwitch(&m_FanControlSP);
                m_FanControlS[prevIndex].s = ISS_ON;
            }

            IDSetSwitch(&m_FanControlSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Video Format
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_VideoFormatSP.name))
        {
            if (Streamer->isBusy())
            {
                m_VideoFormatSP.s = IPS_ALERT;
                LOG_ERROR("Cannot change format while streaming/recording");
                IDSetSwitch(&m_VideoFormatSP, nullptr);
                return true;
            }

            IUUpdateSwitch(&m_VideoFormatSP, states, names, n);
            int currentIndex = IUFindOnSwitchIndex(&m_VideoFormatSP);
            setVideoFormat(currentIndex);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Auto Exposure
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_AutoExposureSP.name))
        {
            IUUpdateSwitch(&m_AutoExposureSP, states, names, n);
            m_AutoExposureSP.s = IPS_OK;
            FP(put_AutoExpoEnable(m_CameraHandle, m_AutoExposureS[TC_AUTO_EXPOSURE_ON].s == ISS_ON ? 1 : 0));
            IDSetSwitch(&m_AutoExposureSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Conversion Gain
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_GainConversionSP.name))
        {
            IUUpdateSwitch(&m_GainConversionSP, states, names, n);
            m_GainConversionSP.s = IPS_OK;
            FP(put_Option(m_CameraHandle, CP(OPTION_CG), IUFindOnSwitchIndex(&m_GainConversionSP)));
            IDSetSwitch(&m_GainConversionSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Auto Controls
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_AutoControlSP.name))
        {
            int previousSwitch = IUFindOnSwitchIndex(&m_AutoControlSP);

            if (IUUpdateSwitch(&m_AutoControlSP, states, names, n) < 0)
            {
                m_AutoControlSP.s = IPS_ALERT;
                IDSetSwitch(&m_AutoControlSP, nullptr);
                return true;
            }

            HRESULT rc = 0;
            std::string autoOperation;
            switch (IUFindOnSwitchIndex(&m_AutoControlSP))
            {
                case TC_AUTO_TINT:
                    rc = FP(AwbOnce(m_CameraHandle, nullptr, nullptr));
                    autoOperation = "Auto White Balance Tint/Temp";
                    break;
                case TC_AUTO_WB:
                    rc = FP(AwbInit(m_CameraHandle, nullptr, nullptr));
                    autoOperation = "Auto White Balance RGB";
                    break;
                case TC_AUTO_BB:
                    rc = FP(AbbOnce(m_CameraHandle, nullptr, nullptr));
                    autoOperation = "Auto Black Balance";
                    break;
                default:
                    rc = -1;
            }

            IUResetSwitch(&m_AutoControlSP);

            if (FAILED(rc))
            {
                m_AutoControlS[previousSwitch].s = ISS_ON;
                m_AutoControlSP.s = IPS_ALERT;
                LOGF_ERROR("%s failed (%d)", autoOperation.c_str(), rc);
            }
            else
            {
                m_AutoControlSP.s = IPS_OK;
                LOGF_INFO("%s complete", autoOperation.c_str());
            }

            IDSetSwitch(&m_AutoControlSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Resolution
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_ResolutionSP.name))
        {
            if (Streamer->isBusy())
            {
                m_ResolutionSP.s = IPS_ALERT;
                LOG_ERROR("Cannot change resolution while streaming/recording");
                IDSetSwitch(&m_ResolutionSP, nullptr);
                return true;
            }

            int preIndex = IUFindOnSwitchIndex(&m_ResolutionSP);
            IUUpdateSwitch(&m_ResolutionSP, states, names, n);
            int targetIndex = IUFindOnSwitchIndex(&m_ResolutionSP);

            if (m_ConfigResolutionIndex == targetIndex)
            {
                m_ResolutionSP.s = IPS_OK;
                IDSetSwitch(&m_ResolutionSP, nullptr);
                return true;
            }

            // Stop capture
            LOG_DEBUG("Stopping camera to change resolution");
            FP(Stop(m_CameraHandle));

            HRESULT rc = FP(put_eSize(m_CameraHandle, targetIndex));
            if (FAILED(rc))
            {
                m_ResolutionSP.s = IPS_ALERT;
                IUResetSwitch(&m_ResolutionSP);
                m_ResolutionS[preIndex].s = ISS_ON;
                LOGF_ERROR("Failed to change resolution. %s", errorCodes(rc).c_str());
            }
            else
            {
                m_ResolutionSP.s = IPS_OK;
                PrimaryCCD.setResolution(m_Instance->model->res[targetIndex].width, m_Instance->model->res[targetIndex].height);
                LOGF_INFO("Resolution changed to %s", m_ResolutionS[targetIndex].label);
                allocateFrameBuffer();
                m_ConfigResolutionIndex = targetIndex;
                saveConfig(true, m_ResolutionSP.name);
            }

            IDSetSwitch(&m_ResolutionSP, nullptr);

            // Restart capture
            FP(StartPullModeWithCallback(m_CameraHandle, &ToupBase::eventCB, this));
            LOG_DEBUG("Restarting event callback after changing resolution");
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Auto White Balance
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_WBAutoSP.name))
        {
            IUUpdateSwitch(&m_WBAutoSP, states, names, n);
            HRESULT rc = 0;
            if (IUFindOnSwitchIndex(&m_WBAutoSP) == TC_AUTO_WB_TT)
                rc = FP(AwbOnce(m_CameraHandle, nullptr, nullptr));
            else
                rc = FP(AwbInit(m_CameraHandle, nullptr, nullptr));

            IUResetSwitch(&m_WBAutoSP);
            if (SUCCEEDED(rc))
            {
                LOG_INFO("Executing auto white balance");
                m_WBAutoSP.s = IPS_OK;
            }
            else
            {
                LOGF_ERROR("Executing auto white balance failed %s", errorCodes(rc).c_str());
                m_WBAutoSP.s = IPS_ALERT;
            }

            IDSetSwitch(&m_WBAutoSP, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

bool ToupBase::StartStreaming()
{
    int rc = 0;
    // Always disable Auto-Exposure on streaming
    FP(put_AutoExpoEnable(m_CameraHandle, 0));

    if (m_ExposureRequest != (1.0 / Streamer->getTargetFPS()))
    {
        m_ExposureRequest = 1.0 / Streamer->getTargetFPS();

        uint32_t uSecs = static_cast<uint32_t>(m_ExposureRequest * 1000000.0f);
        rc = FP(put_ExpoTime(m_CameraHandle, uSecs));
        if (FAILED(rc))
        {
            LOGF_ERROR("Failed to set video exposure time. Error: %s", errorCodes(rc).c_str());
            return false;
        }
    }

    rc = FP(put_Option(m_CameraHandle, CP(OPTION_TRIGGER), 0));
    if (FAILED(rc))
    {
        LOGF_ERROR("Failed to set video trigger mode. %s", errorCodes(rc).c_str());
        return false;
    }
    m_CurrentTriggerMode = TRIGGER_VIDEO;

    return true;
}

bool ToupBase::StopStreaming()
{
    int rc = FP(put_Option(m_CameraHandle, CP(OPTION_TRIGGER), 1));
    if (FAILED(rc))
    {
        LOGF_ERROR("Failed to set video trigger mode. %s", errorCodes(rc).c_str());
        return false;
    }
    m_CurrentTriggerMode = TRIGGER_SOFTWARE;

    // Return auto exposure to what it was
    FP(put_AutoExpoEnable(m_CameraHandle, m_AutoExposureS[TC_AUTO_EXPOSURE_ON].s == ISS_ON ? 1 : 0));

    return true;
}

int ToupBase::SetTemperature(double temperature)
{
    if (activateCooler(true) == false)
    {
        LOG_ERROR("Failed to activate cooler");
        return -1;
    }

    int16_t nTemperature = static_cast<int16_t>(temperature * 10.0);
    HRESULT rc = FP(put_Temperature(m_CameraHandle, nTemperature));
    if (FAILED(rc))
    {
        LOGF_ERROR("Failed to set temperature. %s", errorCodes(rc).c_str());
        return -1;
    }

    // Otherwise, we set the temperature request and we update the status in TimerHit() function.
    LOGF_INFO("Setting CCD temperature to %+06.2fC", temperature);
    return 0;
}

bool ToupBase::activateCooler(bool enable)
{
    HRESULT rc = FP(put_Option(m_CameraHandle, CP(OPTION_TEC), enable ? 1 : 0));
    IUResetSwitch(&m_CoolerSP);
    if (FAILED(rc))
    {
        m_CoolerS[enable ? TC_COOLER_OFF : TC_COOLER_ON].s = ISS_ON;
        m_CoolerSP.s = IPS_ALERT;
        LOGF_ERROR("Failed to turn cooler %s (%s)", enable ? "on" : "off", errorCodes(rc).c_str());
        IDSetSwitch(&m_CoolerSP, nullptr);
        return false;
    }
    else
    {
        m_CoolerS[enable ? TC_COOLER_ON : TC_COOLER_OFF].s = ISS_ON;
        m_CoolerSP.s = IPS_OK;
        IDSetSwitch(&m_CoolerSP, nullptr);
        return true;
    }
}

bool ToupBase::StartExposure(float duration)
{
    HRESULT rc = 0;
    PrimaryCCD.setExposureDuration(static_cast<double>(duration));

    uint32_t uSecs = static_cast<uint32_t>(duration * 1000000.0f);

    LOGF_DEBUG("Starting exposure: %d us @ %s", uSecs, IUFindOnSwitch(&m_ResolutionSP)->label);

    // Only update exposure when necessary
    if (m_ExposureRequest != duration)
    {
        m_ExposureRequest = duration;

        if (FAILED(rc = FP(put_ExpoTime(m_CameraHandle, uSecs))))
        {
            LOGF_ERROR("Failed to set exposure time. Error: %s", errorCodes(rc).c_str());
            return false;
        }
    }

    timeval exposure_time, current_time;
    gettimeofday(&current_time, nullptr);
    exposure_time.tv_sec = uSecs / 1000000;
    exposure_time.tv_usec = uSecs % 1000000;
    timeradd(&current_time, &exposure_time, &m_ExposureEnd);

    if (m_ExposureRequest > VERBOSE_EXPOSURE)
        LOGF_INFO("Taking a %g seconds frame", static_cast<double>(m_ExposureRequest));

    InExposure = true;

    if (m_CurrentTriggerMode != TRIGGER_SOFTWARE)
    {
        rc = FP(put_Option(m_CameraHandle, CP(OPTION_TRIGGER), 1));
        if (FAILED(rc))
            LOGF_ERROR("Failed to set software trigger mode. %s", errorCodes(rc).c_str());
        m_CurrentTriggerMode = TRIGGER_SOFTWARE;
    }

    bool capturedStarted = false;

    // Snap still image
    if (m_CanSnap)
    {
        if (SUCCEEDED(rc = FP(Snap(m_CameraHandle, IUFindOnSwitchIndex(&m_ResolutionSP)))))
            capturedStarted = true;
        else
        {
            LOGF_WARN("Failed to snap exposure. Error: %s. Switching to regular exposure", errorCodes(rc).c_str());
            m_CanSnap = false;
        }
    }

    if (!capturedStarted)
    {
        // Trigger an exposure
        if (FAILED(rc = FP(Trigger(m_CameraHandle, 1))))
        {
            LOGF_ERROR("Failed to trigger exposure. Error: %s", errorCodes(rc).c_str());
            return false;
        }
    }

    // Timeout 500ms after expected duration
    m_CaptureTimeout.start(duration * 1000 + m_DownloadEstimation * TimeoutFactorN[0].value);

    return true;
}

bool ToupBase::AbortExposure()
{
    FP(Trigger(m_CameraHandle, 0));
    InExposure = false;
    m_CaptureTimeoutCounter = 0;
    m_CaptureTimeout.stop();
    return true;
}

void ToupBase::captureTimeoutHandler()
{
    HRESULT rc = 0;

    if (!isConnected())
        return;

    m_CaptureTimeoutCounter++;

    if (m_CaptureTimeoutCounter >= 3)
    {
        m_CaptureTimeoutCounter = 0;
        LOG_ERROR("Camera timed out multiple times. Exposure failed");
        PrimaryCCD.setExposureFailed();
        return;
    }

    // Snap still image
    if (m_CanSnap && FAILED(rc = FP(Snap(m_CameraHandle, IUFindOnSwitchIndex(&m_ResolutionSP)))))
    {
        LOGF_ERROR("Failed to snap exposure. Error: %s", errorCodes(rc).c_str());
        return;
    }
    else
    {
        // Trigger an exposure
        if (FAILED(rc = FP(Trigger(m_CameraHandle, 1))))
        {
            LOGF_ERROR("Failed to trigger exposure. Error: %s", errorCodes(rc).c_str());
            return;
        }
    }

    LOG_DEBUG("Capture timed out, restarting exposure");
    m_CaptureTimeout.start(m_ExposureRequest * 1000 + m_DownloadEstimation * TimeoutFactorN[0].value);
}

bool ToupBase::UpdateCCDFrame(int x, int y, int w, int h)
{
    // Make sure all are even
    x -= (x % 2);
    y -= (y % 2);
    w -= (w % 2);
    h -= (h % 2);

    if (w > PrimaryCCD.getXRes())
    {
        LOGF_INFO("Error: invalid width requested %d", w);
        return false;
    }
    if (h > PrimaryCCD.getYRes())
    {
        LOGF_INFO("Error: invalid height request %d", h);
        return false;
    }

    LOGF_DEBUG("Camera ROI. X: %d, Y: %d, W: %d, H: %d. Binning %dx%d", x, y, w, h, PrimaryCCD.getBinX(), PrimaryCCD.getBinY());

    HRESULT rc = FP(put_Roi(m_CameraHandle, x, y, w, h));
    if (FAILED(rc))
    {
        LOGF_ERROR("Error setting camera ROI: %d", rc);
        return false;
    }

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w, h);

    // As proposed by Max in INDI forum, increase download estimation after changing ROI since next
    // frame may take longer to download.
    m_DownloadEstimation = 10000;

    // Total bytes required for image buffer
    uint32_t nbuf = (w * h * PrimaryCCD.getBPP() / 8) * m_Channels;
    LOGF_DEBUG("Updating frame buffer size to %d bytes", nbuf);
    PrimaryCCD.setFrameBufferSize(nbuf);

    // Always set BINNED size
    Streamer->setSize(w / PrimaryCCD.getBinX(), h / PrimaryCCD.getBinY());
    return true;
}

bool ToupBase::updateBinningMode(int binx, int mode)
{
    int binningMode = binx;

    if ((mode == TC_BINNING_AVG) && (binx > 1))
        binningMode = binx | 0x80;
    LOGF_DEBUG("binningMode code to set: 0x%x", binningMode);

    HRESULT rc = FP(put_Option(m_CameraHandle, CP(OPTION_BINNING), binningMode));
    if (FAILED(rc))
    {
        LOGF_ERROR("Binning %dx%d with Option 0x%x is not support. %s", binx, binx, binningMode, errorCodes(rc).c_str());
        m_BinningModeSP.s = IPS_ALERT;
        IDSetSwitch(&m_BinningModeSP, nullptr);
        return false;
    }
    else
    {
        m_BinningModeSP.s = IPS_OK;
        IDSetSwitch(&m_BinningModeSP, nullptr);
    }

    PrimaryCCD.setBin(binx, binx);

    return UpdateCCDFrame(PrimaryCCD.getSubX(), PrimaryCCD.getSubY(), PrimaryCCD.getSubW(), PrimaryCCD.getSubH());
}

bool ToupBase::UpdateCCDBin(int binx, int biny)
{
    if (binx != biny)
    {
        LOG_ERROR("Binning dimensions must be equal");
        return false;
    }

    return updateBinningMode(binx, m_BinningMode);
}

// The generic timer call back is used for temperature monitoring
void ToupBase::TimerHit()
{
    if (isConnected() == false)
        return;

    if (InExposure)
    {
        timeval curtime, diff;
        gettimeofday(&curtime, nullptr);
        timersub(&m_ExposureEnd, &curtime, &diff);
        double timeleft = diff.tv_sec + diff.tv_usec / 1e6;
        if (timeleft < 0)
            timeleft = 0;
        PrimaryCCD.setExposureLeft(timeleft);
    }
    else if (m_Instance->model->flag & CP(FLAG_GETTEMPERATURE))
    {
        int16_t nTemperature = 0, currentTemperature = (int16_t)(TemperatureN[0].value * 10);
        HRESULT rc = FP(get_Temperature(m_CameraHandle, &nTemperature));
        if (FAILED(rc))
        {
            LOGF_ERROR("get_Temperature error. %s", errorCodes(rc).c_str());
            TemperatureNP.s = IPS_ALERT;
        }
		
        switch (TemperatureNP.s)
        {
            case IPS_IDLE:
            case IPS_OK:
                if (currentTemperature != nTemperature)
                {					
			        TemperatureN[0].value = static_cast<double>(nTemperature / 10.0);
                    IDSetNumber(&TemperatureNP, nullptr);
                }
                break;

            case IPS_ALERT:
                break;

            case IPS_BUSY:
			    TemperatureN[0].value = static_cast<double>(nTemperature / 10.0);
                IDSetNumber(&TemperatureNP, nullptr);
                break;
        }
    }

    SetTimer(getCurrentPollingPeriod());

}

/* Helper function for NS timer call back */
void ToupBase::TimerHelperNS(void *context)
{
    static_cast<ToupBase*>(context)->TimerNS();
}

/* The timer call back for NS guiding */
void ToupBase::TimerNS()
{
    LOG_DEBUG("Guide NS pulse complete");
    m_NStimerID = -1;
    GuideComplete(AXIS_DE);
}

/* Stop the timer for NS guiding */
void ToupBase::stopTimerNS()
{
    if (m_NStimerID != -1)
    {
        LOG_DEBUG("Guide NS pulse complete");
        GuideComplete(AXIS_DE);
        IERmTimer(m_NStimerID);
        m_NStimerID = -1;
    }
}

IPState ToupBase::guidePulseNS(uint32_t ms, eGUIDEDIRECTION dir, const char *dirName)
{
    stopTimerNS();
    LOGF_DEBUG("Starting %s guide for %d ms", dirName, ms);

    // If pulse < 50ms, we wait. Otherwise, we schedule it.
    int uSecs = ms * 1000;
    HRESULT rc = FP(ST4PlusGuide(m_CameraHandle, dir, ms));
    if (FAILED(rc))
    {
        LOGF_ERROR("%s pulse guiding failed: %s", dirName, errorCodes(rc).c_str());
        return IPS_ALERT;
    }

    if (ms < 50)
    {
        usleep(uSecs);
        return IPS_OK;
    }

    m_NStimerID = IEAddTimer(ms, ToupBase::TimerHelperNS, this);
    return IPS_BUSY;
}

IPState ToupBase::GuideNorth(uint32_t ms)
{
    return guidePulseNS(ms, TOUPBASE_NORTH, "North");
}

IPState ToupBase::GuideSouth(uint32_t ms)
{
    return guidePulseNS(ms, TOUPBASE_SOUTH, "South");
}

/* Helper function for WE timer call back */
void ToupBase::TimerHelperWE(void *context)
{
    static_cast<ToupBase*>(context)->TimerWE();
}

/* The timer call back for WE guiding */
void ToupBase::TimerWE()
{
    LOG_DEBUG("Guide WE pulse complete");
    m_WEtimerID = -1;
    GuideComplete(AXIS_RA);
}

void ToupBase::stopTimerWE()
{
    if (m_WEtimerID != -1)
    {
        LOG_DEBUG("Guide WE pulse complete");
        GuideComplete(AXIS_RA);
        IERmTimer(m_WEtimerID);
        m_WEtimerID = -1;
    }
}

IPState ToupBase::guidePulseWE(uint32_t ms, eGUIDEDIRECTION dir, const char *dirName)
{
    stopTimerWE();
    LOGF_DEBUG("Starting %s guide for %d ms", dirName, ms);

    // If pulse < 50ms, we wait. Otherwise, we schedule it.
    int uSecs = ms * 1000;
    HRESULT rc = FP(ST4PlusGuide(m_CameraHandle, dir, ms));
    if (FAILED(rc))
    {
        LOGF_ERROR("%s pulse guiding failed: %s", dirName, errorCodes(rc).c_str());
        return IPS_ALERT;
    }

    if (ms < 50)
    {
        usleep(uSecs);
        return IPS_OK;
    }

    m_WEtimerID = IEAddTimer(ms, ToupBase::TimerHelperWE, this);
    return IPS_BUSY;
}

IPState ToupBase::GuideEast(uint32_t ms)
{
    return guidePulseWE(ms, TOUPBASE_EAST, "East");
}

IPState ToupBase::GuideWest(uint32_t ms)
{
    return guidePulseWE(ms, TOUPBASE_WEST, "West");
}

const char *ToupBase::getBayerString()
{
    uint32_t nFourCC = 0, nBitDepth = 0;
    FP(get_RawFormat(m_CameraHandle, &nFourCC, &nBitDepth));

    LOGF_DEBUG("Raw format FourCC %#8X bitDepth %d", nFourCC, nBitDepth);

    // 8, 10, 12, 14, or 16
    m_RawBitsPerPixel = nBitDepth;

    switch (nFourCC)
    {
        case MAKEFOURCC('G', 'B', 'R', 'G'):
            m_CameraPixelFormat = INDI_BAYER_GBRG;
            return "GBRG";
        case MAKEFOURCC('R', 'G', 'G', 'B'):
            m_CameraPixelFormat = INDI_BAYER_RGGB;
            return "RGGB";
        case MAKEFOURCC('B', 'G', 'G', 'R'):
            m_CameraPixelFormat = INDI_BAYER_BGGR;
            return "BGGR";
        case MAKEFOURCC('G', 'R', 'B', 'G'):
            m_CameraPixelFormat = INDI_BAYER_GRBG;
            return "GRBG";
        default:
            m_CameraPixelFormat = INDI_BAYER_RGGB;
            return "RGGB";
    }
}

void ToupBase::refreshControls()
{
    IDSetNumber(&m_ControlNP, nullptr);
}

void ToupBase::addFITSKeywords(INDI::CCDChip * targetChip, std::vector<INDI::FITSRecord> &fitsKeywords)
{
    INDI::CCD::addFITSKeywords(targetChip, fitsKeywords);

    INumber *gainNP = IUFindNumber(&m_ControlNP, m_ControlN[TC_GAIN].name);

    if (gainNP)
        fitsKeywords.push_back({"GAIN", gainNP->value, 3, "Gain"});
}

bool ToupBase::saveConfigItems(FILE * fp)
{
    INDI::CCD::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &m_TimeoutFactorNP);
    if (HasCooler())
        IUSaveConfigSwitch(fp, &m_CoolerSP);
    IUSaveConfigNumber(fp, &m_ControlNP);

    IUSaveConfigNumber(fp, &m_OffsetNP);

    if (m_MonoCamera == false)
        IUSaveConfigSwitch(fp, &m_WBAutoSP);

    IUSaveConfigSwitch(fp, &m_VideoFormatSP);
    IUSaveConfigSwitch(fp, &m_ResolutionSP);
    IUSaveConfigSwitch(fp, &m_BinningModeSP);

    if (m_HasLowNoise)
        IUSaveConfigSwitch(fp, &m_LowNoiseSP);

    if (m_HasHighFullwellMode)
        IUSaveConfigSwitch(fp, &m_HighFullwellModeSP);        
    
    return true;
}

void ToupBase::eventCB(unsigned event, void* pCtx)
{
    static_cast<ToupBase*>(pCtx)->eventCallBack(event);
}

void ToupBase::eventCallBack(unsigned event)
{
    LOGF_DEBUG("Event %#04X", event);
    switch (event)
    {
        case CP(EVENT_EXPOSURE):
            m_CaptureTimeoutCounter = 0;
            m_CaptureTimeout.stop();
            break;
        case CP(EVENT_TEMPTINT):
		{
			LOG_DEBUG("Temp Tint changed");
			int nTemp = CP(TEMP_DEF), nTint = CP(TINT_DEF);
			FP(get_TempTint)(m_CameraHandle, &nTemp, &nTint);
			
			m_WBTempTintN[TC_WB_TEMP].value = nTemp;
			m_WBTempTintN[TC_WB_TINT].value = nTint;
			m_WBTempTintNP.s = IPS_OK;
			IDSetNumber(&m_WBTempTintNP, nullptr);
		}
        break;
        case CP(EVENT_IMAGE):
        {
            m_CaptureTimeoutCounter = 0;
            m_CaptureTimeout.stop();

            // Estimate download time
            timeval curtime, diff;
            gettimeofday(&curtime, nullptr);
            timersub(&curtime, &m_ExposureEnd, &diff);
            m_DownloadEstimation = diff.tv_sec * 1000 + diff.tv_usec / 1e3;

            if (m_DownloadEstimation < MIN_DOWNLOAD_ESTIMATION)
            {
                m_DownloadEstimation = MIN_DOWNLOAD_ESTIMATION;
                LOGF_DEBUG("Too low download estimate. Bumping to %.f ms", m_DownloadEstimation);
            }

            XP(FrameInfoV2) info;
            memset(&info, 0, sizeof(XP(FrameInfoV2)));

            int captureBits = m_BitsPerPixel == 8 ? 8 : m_MaxBitDepth;

            if (Streamer->isStreaming() || Streamer->isRecording())
            {
                std::unique_lock<std::mutex> guard(ccdBufferLock);
                HRESULT rc = FP(PullImageWithRowPitchV2(m_CameraHandle, PrimaryCCD.getFrameBuffer(), captureBits * m_Channels, -1, &info));
                guard.unlock();
                if (SUCCEEDED(rc))
                    Streamer->newFrame(PrimaryCCD.getFrameBuffer(), PrimaryCCD.getFrameBufferSize());
            }
            else if (InExposure)
            {
                InExposure = false;
                PrimaryCCD.setExposureLeft(0);
                uint8_t *buffer = PrimaryCCD.getFrameBuffer();

                if (m_MonoCamera == false && m_CurrentVideoFormat == TC_VIDEO_COLOR_RGB)
                    buffer = static_cast<uint8_t*>(malloc(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 3));

                std::unique_lock<std::mutex> guard(ccdBufferLock);
                HRESULT rc = FP(PullImageWithRowPitchV2(m_CameraHandle, buffer, captureBits * m_Channels, -1, &info));
                guard.unlock();
                if (FAILED(rc))
                {
                    LOGF_ERROR("Failed to pull image. %s", errorCodes(rc).c_str());
                    PrimaryCCD.setExposureFailed();
                    if (m_MonoCamera == false && m_CurrentVideoFormat == TC_VIDEO_COLOR_RGB)
                        free(buffer);
                }
                else
                {
                    if (m_MonoCamera == false && m_CurrentVideoFormat == TC_VIDEO_COLOR_RGB)
                    {
                        std::unique_lock<std::mutex> locker(ccdBufferLock);
                        uint8_t *image  = PrimaryCCD.getFrameBuffer();
                        uint32_t width  = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * (PrimaryCCD.getBPP() / 8);
                        uint32_t height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY() * (PrimaryCCD.getBPP() / 8);

                        uint8_t *subR = image;
                        uint8_t *subG = image + width * height;
                        uint8_t *subB = image + width * height * 2;
                        int size      = width * height * 3 - 3;

                        // RGB to three sepearate R-frame, G-frame, and B-frame for color FITS
                        for (int i = 0; i <= size; i += 3)
                        {
                            *subR++ = buffer[i];
                            *subG++ = buffer[i + 1];
                            *subB++ = buffer[i + 2];
                        }

                        locker.unlock();
                        free(buffer);
                    }

                    LOGF_DEBUG("Image received. Width: %d, Height: %d, flag: %d, timestamp: %ld", info.width, info.height, info.flag, info.timestamp);
                    ExposureComplete(&PrimaryCCD);
                }
            }
            else
            {
                HRESULT rc = FP(put_Option(m_CameraHandle, CP(OPTION_FLUSH), 3));
                LOG_DEBUG("Image event received after CCD is stopped. Image flushed");
                if (FAILED(rc))
                    LOGF_ERROR("Failed to flush image. %s", errorCodes(rc).c_str());
            }
        }
        break;
        case CP(EVENT_STILLIMAGE):
        {
            m_CaptureTimeoutCounter = 0;
            m_CaptureTimeout.stop();
            XP(FrameInfoV2) info;
            memset(&info, 0, sizeof(XP(FrameInfoV2)));

            int captureBits = m_BitsPerPixel == 8 ? 8 : m_MaxBitDepth;

            if (Streamer->isStreaming() || Streamer->isRecording())
            {
                std::unique_lock<std::mutex> guard(ccdBufferLock);
                HRESULT rc = FP(PullStillImageWithRowPitchV2(m_CameraHandle, PrimaryCCD.getFrameBuffer(), captureBits * m_Channels, -1, &info));
                guard.unlock();
                if (SUCCEEDED(rc))
                    Streamer->newFrame(PrimaryCCD.getFrameBuffer(), PrimaryCCD.getFrameBufferSize());
            }
            else if (InExposure)
            {
                InExposure = false;
                PrimaryCCD.setExposureLeft(0);
                uint8_t *buffer = PrimaryCCD.getFrameBuffer();

                if (m_MonoCamera == false && m_CurrentVideoFormat == TC_VIDEO_COLOR_RGB)
                    buffer = static_cast<uint8_t*>(malloc(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 3));

                std::unique_lock<std::mutex> guard(ccdBufferLock);
                HRESULT rc = FP(PullStillImageWithRowPitchV2(m_CameraHandle, buffer, captureBits * m_Channels, -1, &info));
                guard.unlock();
                if (FAILED(rc))
                {
                    LOGF_ERROR("Failed to pull image. %s", errorCodes(rc).c_str());
                    PrimaryCCD.setExposureFailed();
                    if (m_MonoCamera == false && m_CurrentVideoFormat == TC_VIDEO_COLOR_RGB)
                        free(buffer);
                }
                else
                {
                    if (m_MonoCamera == false && m_CurrentVideoFormat == TC_VIDEO_COLOR_RGB)
                    {
                        std::unique_lock<std::mutex> locker(ccdBufferLock);
                        uint8_t *image  = PrimaryCCD.getFrameBuffer();
                        uint32_t width  = PrimaryCCD.getSubW() / PrimaryCCD.getBinX() * (PrimaryCCD.getBPP() / 8);
                        uint32_t height = PrimaryCCD.getSubH() / PrimaryCCD.getBinY() * (PrimaryCCD.getBPP() / 8);

                        uint8_t *subR = image;
                        uint8_t *subG = image + width * height;
                        uint8_t *subB = image + width * height * 2;
                        int size      = width * height * 3 - 3;

                        // RGB to three sepearate R-frame, G-frame, and B-frame for color FITS
                        for (int i = 0; i <= size; i += 3)
                        {
                            *subR++ = buffer[i];
                            *subG++ = buffer[i + 1];
                            *subB++ = buffer[i + 2];
                        }

                        locker.unlock();
                        free(buffer);
                    }

                    LOGF_DEBUG("Image received. Width: %d, Height: %d, flag: %d, timestamp: %ld", info.width, info.height, info.flag, info.timestamp);
                    ExposureComplete(&PrimaryCCD);
                }
            }
            else
            {
                HRESULT rc = FP(put_Option(m_CameraHandle, CP(OPTION_FLUSH), 3));
                LOG_DEBUG("Image event received after CCD is stopped. Image flushed");
                if (FAILED(rc))
                    LOGF_ERROR("Failed to flush image. %s", errorCodes(rc).c_str());
            }
        }
        break;
        case CP(EVENT_WBGAIN):
        {
			LOG_DEBUG("White Balance Gain changed");
			int aGain[3] = { 0 };
			FP(get_WhiteBalanceGain)(m_CameraHandle, aGain);
			m_WBRGBN[TC_WB_R].value = aGain[TC_WB_R];
			m_WBRGBN[TC_WB_G].value = aGain[TC_WB_G];
			m_WBRGBN[TC_WB_B].value = aGain[TC_WB_B];
			m_WBRGBNP.s = IPS_OK;
			IDSetNumber(&m_WBRGBNP, nullptr);
        }
		break;
        case CP(EVENT_BLACK):
        {
			LOG_DEBUG("Black Balance Gain changed");
			unsigned short aSub[3] = { 0 };
			FP(get_BlackBalance)(m_CameraHandle, aSub);
			m_BlackBalanceN[TC_BLACK_R].value = aSub[TC_BLACK_R];
			m_BlackBalanceN[TC_BLACK_G].value = aSub[TC_BLACK_G];
			m_BlackBalanceN[TC_BLACK_B].value = aSub[TC_BLACK_B];
			BlackBalanceNP.s = IPS_OK;
			IDSetNumber(&BlackBalanceNP, nullptr);
		}
        break;
        case CP(EVENT_ERROR):
            LOG_DEBUG("Camera Error");
            break;
        case CP(EVENT_DISCONNECTED):
            LOG_DEBUG("Camera disconnected");
            break;
        case CP(EVENT_NOFRAMETIMEOUT):
            LOG_DEBUG("Camera timed out");
            PrimaryCCD.setExposureFailed();
            break;
        default:
            break;
    }
}

bool ToupBase::setVideoFormat(uint8_t index)
{
    if (index == IUFindOnSwitchIndex(&m_VideoFormatSP))
        return true;

    m_Channels = 1;
    m_BitsPerPixel = 8;
    // Mono
    if (m_MonoCamera)
    {
        if (m_MaxBitDepth == 8 && index == TC_VIDEO_MONO_16)
        {
            m_VideoFormatSP.s = IPS_ALERT;
            LOG_ERROR("Only 8-bit format is supported");
            IUResetSwitch(&m_VideoFormatSP);
            m_VideoFormatS[TC_VIDEO_MONO_8].s = ISS_ON;
            IDSetSwitch(&m_VideoFormatSP, nullptr);
            return false;
        }

        // We need to stop camera first
        LOG_DEBUG("Stopping camera to change video mode");
        FP(Stop(m_CameraHandle));

        int rc = FP(put_Option(m_CameraHandle, CP(OPTION_BITDEPTH), index));
        if (SUCCEEDED(rc))
			LOGF_DEBUG("Set OPTION_BITDEPTH --> %d", index);
		else
        {
            LOGF_ERROR("Failed to set high bit depth mode %s", errorCodes(rc).c_str());
            m_VideoFormatSP.s = IPS_ALERT;
            IUResetSwitch(&m_VideoFormatSP);
            m_VideoFormatS[TC_VIDEO_MONO_8].s = ISS_ON;
            IDSetSwitch(&m_VideoFormatSP, nullptr);

            // Restart Capture
            FP(StartPullModeWithCallback(m_CameraHandle, &ToupBase::eventCB, this));
            LOG_DEBUG("Restarting event callback after video mode change failed");

            return false;
        }

        m_BitsPerPixel = (index == TC_VIDEO_MONO_8) ? 8 : 16;
    }
    // Color
    else
    {
        // We need to stop camera first
        LOG_DEBUG("Stopping camera to change video mode");
        FP(Stop(m_CameraHandle));

        int rc = FP(put_Option(m_CameraHandle, CP(OPTION_RAW), index));
        if (SUCCEEDED(rc))
			LOGF_DEBUG("Set OPTION_RAW --> %d", index);
		else
        {
            LOGF_ERROR("Failed to set video mode: %s", errorCodes(rc).c_str());
            m_VideoFormatSP.s = IPS_ALERT;
            IUResetSwitch(&m_VideoFormatSP);
            m_VideoFormatS[TC_VIDEO_COLOR_RGB].s = ISS_ON;
            IDSetSwitch(&m_VideoFormatSP, nullptr);

            // Restart Capture
            FP(StartPullModeWithCallback(m_CameraHandle, &ToupBase::eventCB, this));
            LOG_DEBUG("Restarting event callback after changing video mode failed");
            return false;
        }            

        if (index == TC_VIDEO_COLOR_RGB)
        {
            m_Channels = 3;
            m_BitsPerPixel = 8;
            SetCCDCapability(GetCCDCapability() & ~CCD_HAS_BAYER);
        }
        else
        {
            SetCCDCapability(GetCCDCapability() | CCD_HAS_BAYER);
            IUSaveText(&BayerT[2], getBayerString());
            IDSetText(&BayerTP, nullptr);
            m_BitsPerPixel = m_RawBitsPerPixel;
        }
    }

    m_CurrentVideoFormat = index;
    m_BitsPerPixel = (m_BitsPerPixel > 8) ? 16 : 8;

    LOGF_DEBUG("Video Format: %d m_BitsPerPixel: %d", index, m_BitsPerPixel);

    // Allocate memory
    allocateFrameBuffer();

    IUResetSwitch(&m_VideoFormatSP);
    m_VideoFormatS[index].s = ISS_ON;
    m_VideoFormatSP.s = IPS_OK;
    IDSetSwitch(&m_VideoFormatSP, nullptr);

    // Restart Capture
    FP(StartPullModeWithCallback(m_CameraHandle, &ToupBase::eventCB, this));
    LOG_DEBUG("Restarting event callback after video mode change");
    saveConfig(true, m_VideoFormatSP.name);

    return true;
}

bool ToupBase::SetCaptureFormat(uint8_t index)
{
    return setVideoFormat(index);
}

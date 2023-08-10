/*
 Toupcam & oem CCD Driver

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
#include <unistd.h>
#include <deque>

#define BITDEPTH_FLAG       (CP(FLAG_RAW10) | CP(FLAG_RAW12) | CP(FLAG_RAW14) | CP(FLAG_RAW16))
#define CONTROL_TAB         "Control"

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3) \
    (static_cast<uint32_t>(static_cast<uint8_t>(ch0)) \
    | (static_cast<uint32_t>(static_cast<uint8_t>(ch1)) << 8) \
    | (static_cast<uint32_t>(static_cast<uint8_t>(ch2)) << 16) \
    | (static_cast<uint32_t>(static_cast<uint8_t>(ch3)) << 24))
#endif /* defined(MAKEFOURCC) */

static class Loader
{
        std::deque<std::unique_ptr<ToupBase>> cameras;
        XP(DeviceV2) pCameraInfo[CP(MAX)];
    public:
        Loader()
        {
            const int iConnectedCount = FP(EnumV2(pCameraInfo));
            for (int i = 0; i < iConnectedCount; i++)
            {
                if (0 == (CP(FLAG_FILTERWHEEL) & pCameraInfo[i].model->flag))
                    cameras.push_back(std::unique_ptr<ToupBase>(new ToupBase(&pCameraInfo[i])));
            }
            if (cameras.empty())
                IDLog("No camera detected");
        }
} loader;

ToupBase::ToupBase(const XP(DeviceV2) *instance) : m_Instance(instance)
{
    IDLog("model: %s, maxspeed: %d, preview: %d, maxfanspeed: %d", m_Instance->model->name, m_Instance->model->maxspeed,
          m_Instance->model->preview, m_Instance->model->maxfanspeed);

    setVersion(TOUPBASE_VERSION_MAJOR, TOUPBASE_VERSION_MINOR);

    snprintf(this->m_name, MAXINDIDEVICE, "%s %s", getDefaultName(), m_Instance->model->name);
    setDeviceName(this->m_name);

    if (m_Instance->model->flag & CP(FLAG_MONO))
        m_MonoCamera = true;
}

ToupBase::~ToupBase()
{
    delete[] m_FanS;
    delete[] m_HeatS;
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
    IUFillSwitchVector(&m_BinningModeSP, m_BinningModeS, 2, getDeviceName(), "CCD_BINNING_MODE", "Binning Mode",
                       IMAGE_SETTINGS_TAB, IP_WO, ISR_1OFMANY, 0, IPS_IDLE);

    if (m_Instance->model->flag & CP(FLAG_TEC_ONOFF))
    {
        TemperatureN[0].min = CP(TEC_TARGET_MIN);
        TemperatureN[0].max = CP(TEC_TARGET_MAX);
        TemperatureN[0].value = CP(TEC_TARGET_DEF);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Cooler Control
        ///////////////////////////////////////////////////////////////////////////////////
        IUFillSwitch(&m_CoolerS[INDI_ENABLED], "COOLER_ON", "ON", ISS_ON);
        IUFillSwitch(&m_CoolerS[INDI_DISABLED], "COOLER_OFF", "OFF", ISS_OFF);
        IUFillSwitchVector(&m_CoolerSP, m_CoolerS, 2, getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY,
                           0, IPS_BUSY);

        IUFillText(&m_CoolerT, "COOLER_POWER", "Percent", nullptr);
        IUFillTextVector(&m_CoolerTP, &m_CoolerT, 1, getDeviceName(), "COOLER_POWER", "Cooler Power", MAIN_CONTROL_TAB, IP_RO, 0,
                         IPS_IDLE);
    }

    ///////////////////////////////////////////////////////////////////////////////////
    /// Controls
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&m_ControlN[TC_GAIN], "Gain", "Gain", "%.f", CP(EXPOGAIN_MIN), CP(EXPOGAIN_MIN), 1, CP(EXPOGAIN_MIN));
    IUFillNumber(&m_ControlN[TC_CONTRAST], "Contrast", "Contrast", "%.f", CP(CONTRAST_MIN), CP(CONTRAST_MAX), 1,
                 CP(CONTRAST_DEF));
    if (m_MonoCamera)
        nsp = 6;
    else
    {
        nsp = 8;
        IUFillNumber(&m_ControlN[TC_HUE], "Hue", "Hue", "%.f", CP(HUE_MIN), CP(HUE_MAX), 1, CP(HUE_DEF));
        IUFillNumber(&m_ControlN[TC_SATURATION], "Saturation", "Saturation", "%.f", CP(SATURATION_MIN), CP(SATURATION_MAX), 1,
                     CP(SATURATION_DEF));
    }
    IUFillNumber(&m_ControlN[TC_BRIGHTNESS], "Brightness", "Brightness", "%.f", CP(BRIGHTNESS_MIN), CP(BRIGHTNESS_MAX), 1, 0);
    IUFillNumber(&m_ControlN[TC_GAMMA], "Gamma", "Gamma", "%.f", CP(GAMMA_MIN), CP(GAMMA_MAX), 1, CP(GAMMA_DEF));
    IUFillNumber(&m_ControlN[TC_SPEED], "Speed", "Speed", "%.f", 0, m_Instance->model->maxspeed, 1, 0);
    IUFillNumber(&m_ControlN[TC_FRAMERATE_LIMIT], "FPS Limit", "FPS Limit", "%.f", 0, 63, 1, 0);
    IUFillNumberVector(&m_ControlNP, m_ControlN, nsp, getDeviceName(), "CCD_CONTROLS", "Controls", CONTROL_TAB, IP_RW, 60,
                       IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // Black Balance
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
    IUFillNumberVector(&m_BlackBalanceNP, m_BlackBalanceN, nsp, getDeviceName(), "CCD_BLACK_BALANCE", "Black Balance",
                       CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Auto Black Balance
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&m_BBAutoS, "TC_AUTO_BB", "Auto", ISS_OFF);
    IUFillSwitchVector(&m_BBAutoSP, &m_BBAutoS, 1, getDeviceName(), "TC_AUTO_BB", "Auto Black Balance", CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // Black Level (Offset)
    // JM 2023.04.07 DO NOT NAME IT BLACK LEVEL, it must remain as OFFSET
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&m_OffsetN[0], "OFFSET", "Value", "%.f", 0, 255, 1, 0);
    IUFillNumberVector(&m_OffsetNP, m_OffsetN, 1, getDeviceName(), "CCD_OFFSET", "Offset", CONTROL_TAB, IP_RW,
                       60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // R/G/B/Y Level Range
    ///////////////////////////////////////////////////////////////////////////////////
    if (m_MonoCamera)
    {
        nsp = 2;
        IUFillNumber(&m_LevelRangeN[TC_LO_R], "TC_LO", "Low", "%.f", 0, 255, 1, 0);
        IUFillNumber(&m_LevelRangeN[TC_HI_R], "TC_HI", "High", "%.f", 0, 255, 1, 255);
    }
    else
    {
        nsp = 8;
        IUFillNumber(&m_LevelRangeN[TC_LO_R], "TC_LO_R", "Low Red", "%.f", 0, 255, 1, 0);
        IUFillNumber(&m_LevelRangeN[TC_HI_R], "TC_HI_R", "High Red", "%.f", 0, 255, 1, 255);
        IUFillNumber(&m_LevelRangeN[TC_LO_G], "TC_LO_G", "Low Green", "%.f", 0, 255, 1, 0);
        IUFillNumber(&m_LevelRangeN[TC_HI_G], "TC_HI_G", "High Green", "%.f", 0, 255, 1, 255);
        IUFillNumber(&m_LevelRangeN[TC_LO_B], "TC_LO_B", "Low Blue", "%.f", 0, 255, 1, 0);
        IUFillNumber(&m_LevelRangeN[TC_HI_B], "TC_HI_B", "High Blue", "%.f", 0, 255, 1, 255);
        IUFillNumber(&m_LevelRangeN[TC_LO_Y], "TC_LO_Y", "Low Gray", "%.f", 0, 255, 1, 0);
        IUFillNumber(&m_LevelRangeN[TC_HI_Y], "TC_HI_Y", "High Gray", "%.f", 0, 255, 1, 255);
    }
    IUFillNumberVector(&m_LevelRangeNP, m_LevelRangeN, nsp, getDeviceName(), "CCD_LEVEL_RANGE", "Level Range", CONTROL_TAB,
                       IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // Auto Exposure
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&m_AutoExposureS[INDI_ENABLED], "INDI_ENABLED", "ON", ISS_OFF);
    IUFillSwitch(&m_AutoExposureS[INDI_DISABLED], "INDI_DISABLED", "OFF", ISS_ON);
    IUFillSwitchVector(&m_AutoExposureSP, m_AutoExposureS, 2, getDeviceName(), "CCD_AUTO_EXPOSURE", "Auto Exposure",
                       CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    if (m_MonoCamera == false)
    {
        ///////////////////////////////////////////////////////////////////////////////////
        // White Balance
        ///////////////////////////////////////////////////////////////////////////////////
        IUFillNumber(&m_WBN[TC_WB_R], "TC_WB_R", "Red", "%.f", CP(WBGAIN_MIN), CP(WBGAIN_MAX), 10, CP(WBGAIN_DEF));
        IUFillNumber(&m_WBN[TC_WB_G], "TC_WB_G", "Green", "%.f", CP(WBGAIN_MIN), CP(WBGAIN_MAX), 10, CP(WBGAIN_DEF));
        IUFillNumber(&m_WBN[TC_WB_B], "TC_WB_B", "Blue", "%.f", CP(WBGAIN_MIN), CP(WBGAIN_MAX), 10, CP(WBGAIN_DEF));
        IUFillNumberVector(&m_WBNP, m_WBN, 3, getDeviceName(), "TC_WB", "White Balance", CONTROL_TAB, IP_RW, 60, IPS_IDLE);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Auto White Balance
        ///////////////////////////////////////////////////////////////////////////////////
        IUFillSwitch(&m_WBAutoS, "TC_AUTO_WB", "Auto", ISS_OFF);
        IUFillSwitchVector(&m_WBAutoSP, &m_WBAutoS, 1, getDeviceName(), "TC_AUTO_WB", "Auto White Balance", CONTROL_TAB, IP_RW,
                           ISR_1OFMANY, 60, IPS_IDLE);
    }

    ///////////////////////////////////////////////////////////////////////////////////
    /// Timeout Factor
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&m_TimeoutFactorN, "Timeout", "Factor", "%.2f", 1, 1.2, 0.01, 1.02);
    IUFillNumberVector(&m_TimeoutFactorNP, &m_TimeoutFactorN, 1, getDeviceName(), "TIMEOUT_FACTOR", "Timeout", OPTIONS_TAB,
                       IP_RW, 60, IPS_IDLE);

    if (m_Instance->model->flag & (CP(FLAG_CG) | CP(FLAG_CGHDR)))
    {
        ///////////////////////////////////////////////////////////////////////////////////
        /// Conversion Gain
        ///////////////////////////////////////////////////////////////////////////////////
        int nsp = 2;
        IUFillSwitch(&m_GainConversionS[GAIN_LOW], "GAIN_LOW", "Low", ISS_OFF);
        IUFillSwitch(&m_GainConversionS[GAIN_HIGH], "GAIN_HIGH", "High", ISS_OFF);
        if (m_Instance->model->flag & CP(FLAG_CGHDR))
        {
            IUFillSwitch(&m_GainConversionS[GAIN_HDR], "GAIN_HDR", "HDR", ISS_OFF);
            ++nsp;
        }
        IUFillSwitchVector(&m_GainConversionSP, m_GainConversionS, nsp, getDeviceName(), "TC_CONVERSION_GAIN", "Conversion Gain",
                           CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    }

    ///////////////////////////////////////////////////////////////////////////////////
    /// Low Noise
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&m_LowNoiseS[INDI_ENABLED], "INDI_ENABLED", "ON", ISS_OFF);
    IUFillSwitch(&m_LowNoiseS[INDI_DISABLED], "INDI_DISABLED", "OFF", ISS_ON);
    IUFillSwitchVector(&m_LowNoiseSP, m_LowNoiseS, 2, getDeviceName(), "TC_LOW_NOISE", "Low Noise Mode", CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// High Fullwell
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&m_HighFullwellS[INDI_ENABLED], "INDI_ENABLED", "ON", ISS_OFF);
    IUFillSwitch(&m_HighFullwellS[INDI_DISABLED], "INDI_DISABLED", "OFF", ISS_ON);
    IUFillSwitchVector(&m_HighFullwellSP, m_HighFullwellS, 2, getDeviceName(), "TC_HIGHFULLWELL", "High Fullwell Mode",
                       CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    if (m_Instance->model->flag & CP(FLAG_FAN))
    {
        ///////////////////////////////////////////////////////////////////////////////////
        /// Fan
        ///////////////////////////////////////////////////////////////////////////////////
        m_FanS = new ISwitch[m_Instance->model->maxfanspeed + 1];
        IUFillSwitch(&m_FanS[0], "INDI_DISABLED", "OFF", ISS_OFF);
        if (m_Instance->model->maxfanspeed <= 1)
            IUFillSwitch(&m_FanS[1], "INDI_ENABLED", "ON", ISS_OFF);
        else
        {
            for (uint32_t i = 1; i <= m_Instance->model->maxfanspeed; ++i)
                IUFillSwitch(&m_FanS[i], (std::string("FAN_SPEED") + std::to_string(i)).c_str(), std::to_string(i).c_str(), ISS_OFF);
        }
        IUFillSwitchVector(&m_FanSP, m_FanS, m_Instance->model->maxfanspeed + 1, getDeviceName(), "TC_FAN_SPEED",
                           (m_Instance->model->maxfanspeed <= 1) ? "Fan" : "Fan Speed", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    }

    ///////////////////////////////////////////////////////////////////////////////////
    /// Resolution
    ///////////////////////////////////////////////////////////////////////////////////
    for (unsigned i = 0; i < m_Instance->model->preview; i++)
    {
        char label[MAXINDILABEL] = {0};
        snprintf(label, MAXINDILABEL, "%d x %d", m_Instance->model->res[i].width, m_Instance->model->res[i].height);
        IUFillSwitch(&m_ResolutionS[i], label, label, ISS_OFF);
    }
    IUFillSwitchVector(&m_ResolutionSP, m_ResolutionS, m_Instance->model->preview, getDeviceName(), "CCD_RESOLUTION",
                       "Resolution", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUGetConfigOnSwitchIndex(getDeviceName(), m_ResolutionSP.name, &m_ConfigResolutionIndex);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Firmware
    ///////////////////////////////////////////////////////////////////////////////////
    IUFillText(&m_CameraT[TC_CAMERA_MODEL], "MODEL", "Model", m_Instance->model->name);
    IUFillText(&m_CameraT[TC_CAMERA_DATE], "PRODUCTIONDATE", "Production Date", nullptr);
    IUFillText(&m_CameraT[TC_CAMERA_SN], "SN", "SN", nullptr);
    IUFillText(&m_CameraT[TC_CAMERA_FW_VERSION], "FIRMWAREVERSION", "Firmware Version", nullptr);
    IUFillText(&m_CameraT[TC_CAMERA_HW_VERSION], "HARDWAREVERSION", "Hardware Version", nullptr);
    IUFillText(&m_CameraT[TC_CAMERA_REV], "REVISION", "Revision", nullptr);
    IUFillTextVector(&m_CameraTP, m_CameraT, 6, getDeviceName(), "CAMERA", "Camera", INFO_TAB, IP_RO, 0, IPS_IDLE);

    IUFillText(&m_SDKVersionT, "VERSION", "Version", FP(Version()));
    IUFillTextVector(&m_SDKVersionTP, &m_SDKVersionT, 1, getDeviceName(), "SDK", "SDK", INFO_TAB, IP_RO, 0, IPS_IDLE);

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
        {
            defineProperty(&m_CoolerSP);
            defineProperty(&m_CoolerTP);
        }

        // Even if there is no cooler, we define temperature property as READ ONLY
        else if (m_Instance->model->flag & CP(FLAG_GETTEMPERATURE))
        {
            TemperatureNP.p = IP_RO;
            defineProperty(&TemperatureNP);
        }

        if (m_Instance->model->flag & CP(FLAG_FAN))
            defineProperty(&m_FanSP);

        defineProperty(&m_TimeoutFactorNP);
        defineProperty(&m_ControlNP);
        defineProperty(&m_AutoExposureSP);
        defineProperty(&m_ResolutionSP);

        if (m_Instance->model->flag & CP(FLAG_HIGH_FULLWELL))
            defineProperty(&m_HighFullwellSP);

        if (m_Instance->model->flag & CP(FLAG_LOW_NOISE))
            defineProperty(&m_LowNoiseSP);

        if (m_Instance->model->flag & CP(FLAG_HEAT))
            defineProperty(&m_HeatSP);

        if (m_Instance->model->flag & (CP(FLAG_CG) | CP(FLAG_CGHDR)))
            defineProperty(&m_GainConversionSP);

        // Binning mode
        defineProperty(&m_BinningModeSP);
        if (m_MonoCamera == false)
        {
            defineProperty(&m_WBNP);
            defineProperty(&m_WBAutoSP);
        }
        defineProperty(&m_BlackBalanceNP);
        defineProperty(&m_BBAutoSP);
        // Levels
        defineProperty(&m_LevelRangeNP);
        defineProperty(&m_OffsetNP);

        // Firmware
        defineProperty(&m_CameraTP);
        defineProperty(&m_SDKVersionTP);
    }
    else
    {
        if (HasCooler())
        {
            deleteProperty(m_CoolerSP.name);
            deleteProperty(m_CoolerTP.name);
        }
        else
        {
            deleteProperty(TemperatureNP.name);
        }

        if (m_Instance->model->flag & CP(FLAG_FAN))
            deleteProperty(m_FanSP.name);

        deleteProperty(m_TimeoutFactorNP.name);
        deleteProperty(m_ControlNP.name);
        deleteProperty(m_AutoExposureSP.name);
        deleteProperty(m_ResolutionSP.name);

        if (m_Instance->model->flag & CP(FLAG_LOW_NOISE))
            deleteProperty(m_LowNoiseSP.name);

        if (m_Instance->model->flag & CP(FLAG_HIGH_FULLWELL))
            deleteProperty(m_HighFullwellSP.name);

        if (m_Instance->model->flag & CP(FLAG_HEAT))
            deleteProperty(m_HeatSP.name);

        if (m_Instance->model->flag & (CP(FLAG_CG) | CP(FLAG_CGHDR)))
            deleteProperty(m_GainConversionSP.name);

        deleteProperty(m_BinningModeSP.name);
        if (m_MonoCamera == false)
        {
            deleteProperty(m_WBNP.name);
            deleteProperty(m_WBAutoSP.name);
        }
        deleteProperty(m_BlackBalanceNP.name);
        deleteProperty(m_BBAutoSP.name);
        deleteProperty(m_LevelRangeNP.name);
        deleteProperty(m_OffsetNP.name);

        deleteProperty(m_CameraTP.name);
        deleteProperty(m_SDKVersionTP.name);
    }

    return true;
}

bool ToupBase::Connect()
{
    if (isSimulation() == false)
    {
        std::string fullID = m_Instance->id;
        // For RGB White Balance Mode, we need to add @ at the beginning as per docs.
        if (m_MonoCamera == false)
            fullID = "@" + fullID;

        m_Handle = FP(Open(fullID.c_str()));
    }

    if (m_Handle == nullptr)
    {
        LOG_ERROR("Error connecting to the camera");
        return false;
    }

    uint32_t cap = CCD_CAN_BIN | CCD_CAN_ABORT | CCD_HAS_STREAMING | CCD_CAN_SUBFRAME;
    if (m_MonoCamera == false)
        cap |= CCD_HAS_BAYER;
    if (m_Instance->model->flag & CP(FLAG_TEC_ONOFF))
        cap |= CCD_HAS_COOLER;
    if (m_Instance->model->flag & CP(FLAG_ST4))
        cap |= CCD_HAS_ST4_PORT;
    SetCCDCapability(cap);

    // Get min/max exposures
    uint32_t min = 0, max = 0, current = 0;
    FP(get_ExpTimeRange(m_Handle, &min, &max, &current));
    PrimaryCCD.setMinMaxStep("CCD_EXPOSURE", "CCD_EXPOSURE_VALUE", min / 1000000.0, max / 1000000.0, 0, false);
    PrimaryCCD.setBin(1, 1);

    LOGF_INFO("%s connect", getDeviceName());
    return true;
}

bool ToupBase::Disconnect()
{
    stopTimerNS();
    stopTimerWE();

    FP(Close(m_Handle));

    if (m_rgbBuffer)
    {
        free(m_rgbBuffer);
        m_rgbBuffer = nullptr;
    }
    m_rgbBufferSize = 0;

    return true;
}

void ToupBase::setupParams()
{
    HRESULT rc = 0;

    if (m_Instance->model->flag & CP(FLAG_HEAT))
    {
        ///////////////////////////////////////////////////////////////////////////////////
        /// Heat Control
        ///////////////////////////////////////////////////////////////////////////////////
        int maxval = 0, curval = 0;
        FP(get_Option(m_Handle, CP(OPTION_HEAT_MAX), &maxval));
        FP(get_Option(m_Handle, CP(OPTION_HEAT), &curval));

        m_HeatS = new ISwitch[maxval + 1];
        IUFillSwitch(&m_HeatS[0], "INDI_DISABLED", "OFF", (0 == curval) ? ISS_ON : ISS_OFF);
        if (maxval <= 1)
            IUFillSwitch(&m_HeatS[1], "INDI_ENABLED", "ON", (1 == curval) ? ISS_ON : ISS_OFF);
        else
        {
            for (int i = 1; i <= maxval; ++i)
                IUFillSwitch(&m_HeatS[i], (std::string("HEAT") + std::to_string(i)).c_str(), std::to_string(i).c_str(),
                             (i == curval) ? ISS_ON : ISS_OFF);
        }
        IUFillSwitchVector(&m_HeatSP, m_HeatS, maxval + 1, getDeviceName(), "TC_HEAT_CONTROL", "Heat", CONTROL_TAB, IP_RW,
                           ISR_1OFMANY, 60, IPS_IDLE);
    }

    FP(put_AutoExpoEnable(m_Handle, 0));
    FP(put_Option(m_Handle, CP(OPTION_NOFRAME_TIMEOUT), 1));

    // Get Firmware Info
    char tmpBuffer[64] = {0};
    uint16_t pRevision = 0;
    FP(get_SerialNumber(m_Handle, tmpBuffer));
    IUSaveText(&m_CameraT[TC_CAMERA_SN], tmpBuffer);
    FP(get_ProductionDate(m_Handle, tmpBuffer));
    IUSaveText(&m_CameraT[TC_CAMERA_DATE], tmpBuffer);
    FP(get_FwVersion(m_Handle, tmpBuffer));
    IUSaveText(&m_CameraT[TC_CAMERA_FW_VERSION], tmpBuffer);
    FP(get_HwVersion(m_Handle, tmpBuffer));
    IUSaveText(&m_CameraT[TC_CAMERA_HW_VERSION], tmpBuffer);
    FP(get_Revision(m_Handle, &pRevision));
    snprintf(tmpBuffer, 32, "%d", pRevision);
    IUSaveText(&m_CameraT[TC_CAMERA_REV], tmpBuffer);

    // Max supported bit depth
    m_maxBitDepth = FP(get_MaxBitDepth(m_Handle));

    FP(get_Option(m_Handle, CP(OPTION_TEC_VOLTAGE_MAX), &m_maxTecVoltage));

    m_BitsPerPixel = 8;

    if (m_maxBitDepth > 8)
    {
        FP(put_Option(m_Handle, CP(OPTION_BITDEPTH), 1));// enable bitdepth
        m_BitsPerPixel = 16;
    }

    FP(put_Option(m_Handle, CP(OPTION_RAW), 1));

    if (m_MonoCamera)// Check if mono camera
    {
        CaptureFormat mono16 = {"INDI_MONO_16", (std::string("Mono ") + std::to_string(m_maxBitDepth)).c_str(), 16, false};
        CaptureFormat mono8 = {"INDI_MONO_8", "Mono 8", 8, false};
        if (m_maxBitDepth > 8)
        {
            m_CurrentVideoFormat = 1;
            mono16.isDefault = true;
        }
        else
        {
            m_CurrentVideoFormat = 0;
            m_BitsPerPixel = 8;
            mono8.isDefault = true;
        }

        m_CameraPixelFormat = INDI_MONO;
        m_Channels = 1;

        addCaptureFormat(mono8);
        if (m_maxBitDepth > 8)
            addCaptureFormat(mono16);
    }
    else// Color Camera
    {
        CaptureFormat rgb = {"INDI_RGB", "RGB", 8, false };
        CaptureFormat raw = {"INDI_RAW", (m_maxBitDepth > 8) ? (std::string("RAW ") + std::to_string(m_maxBitDepth)).c_str() : "RAW 8", static_cast<uint8_t>((m_maxBitDepth > 8) ? 16 : 8), true };

        m_Channels = 1;
        IUSaveText(&BayerT[2], getBayerString());// Get RAW Format

        addCaptureFormat(rgb);
        addCaptureFormat(raw);
    }

    PrimaryCCD.setNAxis(m_Channels == 1 ? 2 : 3);

    // Fan
    if (m_Instance->model->flag & CP(FLAG_FAN))
    {
        int fan = 0;
        FP(get_Option(m_Handle, CP(OPTION_FAN), &fan));
        IUResetSwitch(&m_FanSP);
        for (unsigned i = 0; i <= m_Instance->model->maxfanspeed; ++i)
            m_FanS[i].s = (fan == static_cast<int>(i)) ? ISS_ON : ISS_OFF;
    }

    // Get active resolution index
    uint32_t currentResolutionIndex = 0, finalResolutionIndex = 0;
    FP(get_eSize(m_Handle, &currentResolutionIndex));
    // If we have a config resolution index, then prefer it over the current resolution index.
    finalResolutionIndex = (m_ConfigResolutionIndex >= 0
                            && m_ConfigResolutionIndex < m_ResolutionSP.nsp) ? m_ConfigResolutionIndex : currentResolutionIndex;
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
        FP(put_eSize(m_Handle, finalResolutionIndex));

    SetCCDParams(m_Instance->model->res[finalResolutionIndex].width, m_Instance->model->res[finalResolutionIndex].height,
                 m_BitsPerPixel, m_Instance->model->xpixsz, m_Instance->model->ypixsz);

    // Set trigger mode to software
    rc = FP(put_Option(m_Handle, CP(OPTION_TRIGGER), m_CurrentTriggerMode));
    if (FAILED(rc))
        LOGF_ERROR("Failed to set software trigger mode. %s", errorCodes(rc).c_str());

    // Get CCD Controls values
    int conversionGain = 0;
    FP(get_Option(m_Handle, CP(OPTION_CG), &conversionGain));
    m_GainConversionS[conversionGain].s = ISS_ON;

    uint16_t nMax = 0, nDef = 0;
    // Gain
    FP(get_ExpoAGainRange(m_Handle, nullptr, &nMax, &nDef));
    m_ControlN[TC_GAIN].max = nMax;
    m_ControlN[TC_GAIN].value = nDef;

    int nVal = 0;
    // Contrast
    FP(get_Contrast(m_Handle, &nVal));
    m_ControlN[TC_CONTRAST].value = nVal;

    if (m_MonoCamera == false)
    {
        // Hue
        FP(get_Hue(m_Handle, &nVal));
        m_ControlN[TC_HUE].value = nVal;

        // Saturation
        FP(get_Saturation(m_Handle, &nVal));
        m_ControlN[TC_SATURATION].value = nVal;
    }

    // Brightness
    FP(get_Brightness(m_Handle, &nVal));
    m_ControlN[TC_BRIGHTNESS].value = nVal;

    // Gamma
    FP(get_Gamma(m_Handle, &nVal));
    m_ControlN[TC_GAMMA].value = nVal;

    // Speed
    // JM 2020-05-06: Reduce speed on ARM for all resolutions
#if defined(__arm__) || defined (__aarch64__)
    m_ControlN[TC_SPEED].value = 0;
    FP(put_Speed(m_Handle, 0));
#else
    rc = FP(get_Speed(m_Handle, &nDef));
    m_ControlN[TC_SPEED].value = nDef;
#endif

    // Frame Rate
    int frameRateLimit = 0;
    FP(get_Option(m_Handle, CP(OPTION_FRAMERATE), &frameRateLimit));
    // JM 2019-08-19: On ARM, set frame limit to max (63) instead of 0 (unlimited)
    // since that results in failure to capture from large sensors
#ifdef __arm__
    frameRateLimit = m_ControlN[TC_FRAMERATE_LIMIT].max;
    FP(put_Option(m_Handle, CP(OPTION_FRAMERATE), frameRateLimit));
#endif
    m_ControlN[TC_FRAMERATE_LIMIT].value = frameRateLimit;

    // Set Bin more for better quality over skip
    if (m_Instance->model->flag & CP(FLAG_BINSKIP_SUPPORTED))
    {
        FP(put_Mode(m_Handle, 0));
    }
    FP(put_HZ(m_Handle, 2));

    if (m_MonoCamera == false)
    {
        // Get White Balance Gain
        int aGain[3] = {0};
        rc = FP(get_WhiteBalanceGain(m_Handle, aGain));
        if (SUCCEEDED(rc))
        {
            m_WBN[TC_WB_R].value = aGain[TC_WB_R];
            m_WBN[TC_WB_G].value = aGain[TC_WB_G];
            m_WBN[TC_WB_B].value = aGain[TC_WB_B];
        }
    }

    // Get Level Ranges
    uint16_t aLow[4] = {0}, aHigh[4] = {255, 255, 255, 255};
    rc = FP(get_LevelRange(m_Handle, aLow, aHigh));
    if (SUCCEEDED(rc))
    {
        if (m_MonoCamera)
        {
            m_LevelRangeN[TC_LO_R].value = aLow[3];
            m_LevelRangeN[TC_HI_R].value = aHigh[3];
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
    rc = FP(get_BlackBalance(m_Handle, aSub));
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
    int bLevelStep = 1 << (m_maxBitDepth - 8);
    m_OffsetN[0].max = CP(BLACKLEVEL8_MAX) * bLevelStep;

    // Allocate memory
    allocateFrameBuffer();

    rc = FP(StartPullModeWithCallback(m_Handle, &ToupBase::eventCB, this));
    if (FAILED(rc))
        LOGF_ERROR("Failed to start camera. %s", errorCodes(rc).c_str());
    SetTimer(getCurrentPollingPeriod());
}

void ToupBase::allocateFrameBuffer()
{
    // Allocate memory
    if (m_MonoCamera)
    {
        if (0 == m_CurrentVideoFormat)
        {
            PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes());
            PrimaryCCD.setBPP(8);
            PrimaryCCD.setNAxis(2);
            Streamer->setPixelFormat(INDI_MONO, 8);
        }
        else
        {
            PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 2);
            PrimaryCCD.setBPP(16);
            PrimaryCCD.setNAxis(2);
            Streamer->setPixelFormat(INDI_MONO, 16);
        }
    }
    else
    {
        if (0 == m_CurrentVideoFormat)
        {
            // RGB24 or RGB888
            PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 3);
            PrimaryCCD.setBPP(8);
            PrimaryCCD.setNAxis(3);
            Streamer->setPixelFormat(INDI_RGB, 8);
        }
        else
        {
            PrimaryCCD.setFrameBufferSize(PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * m_BitsPerPixel / 8);
            PrimaryCCD.setBPP(m_BitsPerPixel);
            PrimaryCCD.setNAxis(2);
            Streamer->setPixelFormat(m_CameraPixelFormat, m_BitsPerPixel);
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
            if (IUUpdateNumber(&m_ControlNP, values, names, n) < 0)
            {
                m_ControlNP.s = IPS_ALERT;
                IDSetNumber(&m_ControlNP, nullptr);
                return true;
            }

            for (uint8_t i = 0; i < m_ControlNP.nnp; i++)
            {
                int value = static_cast<int>(m_ControlN[i].value);
                switch (i)
                {
                    case TC_GAIN:
                        FP(put_ExpoAGain(m_Handle, value));
                        break;

                    case TC_CONTRAST:
                        FP(put_Contrast(m_Handle, value));
                        break;

                    case TC_HUE:
                        FP(put_Hue(m_Handle, value));
                        break;

                    case TC_SATURATION:
                        FP(put_Saturation(m_Handle, value));
                        break;

                    case TC_BRIGHTNESS:
                        FP(put_Brightness(m_Handle, value));
                        break;

                    case TC_GAMMA:
                        FP(put_Gamma(m_Handle, value));
                        break;

                    case TC_SPEED:
                        FP(put_Speed(m_Handle, value));
                        break;

                    case TC_FRAMERATE_LIMIT:
                        FP(put_Option(m_Handle, CP(OPTION_FRAMERATE), value));
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
        /// Level Range
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_LevelRangeNP.name))
        {
            IUUpdateNumber(&m_LevelRangeNP, values, names, n);
            uint16_t lo[4], hi[4];
            if (m_MonoCamera)
            {
                lo[0] = lo[1] = lo[2] = lo[3] = static_cast<uint16_t>(m_LevelRangeN[TC_LO_R].value);
                hi[0] = hi[1] = hi[2] = hi[3] = static_cast<uint16_t>(m_LevelRangeN[TC_HI_R].value);
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

            HRESULT rc = FP(put_LevelRange(m_Handle, lo, hi));
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
        if (!strcmp(name, m_BlackBalanceNP.name))
        {
            IUUpdateNumber(&m_BlackBalanceNP, values, names, n);
            uint16_t aSub[3];
            if (m_MonoCamera)
                aSub[0] = aSub[1] = aSub[2] = static_cast<uint16_t>(m_BlackBalanceN[TC_BLACK_R].value);
            else
            {
                aSub[0] = static_cast<uint16_t>(m_BlackBalanceN[TC_BLACK_R].value);
                aSub[1] = static_cast<uint16_t>(m_BlackBalanceN[TC_BLACK_G].value);
                aSub[2] = static_cast<uint16_t>(m_BlackBalanceN[TC_BLACK_B].value);
            };

            HRESULT rc = FP(put_BlackBalance(m_Handle, aSub));
            if (SUCCEEDED(rc))
                m_BlackBalanceNP.s = IPS_OK;
            else
            {
                m_BlackBalanceNP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set black balance. %s", errorCodes(rc).c_str());
            }

            IDSetNumber(&m_BlackBalanceNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Offset (Black Level)
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_OffsetNP.name))
        {
            IUUpdateNumber(&m_OffsetNP, values, names, n);
            int bLevel = static_cast<uint16_t>(m_OffsetN[0].value);

            HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_BLACKLEVEL), bLevel));
            if (FAILED(rc))
            {
                m_OffsetNP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set offset. %s", errorCodes(rc).c_str());
            }
            else
            {
                m_OffsetNP.s = IPS_OK;
            }

            IDSetNumber(&m_OffsetNP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// White Balance
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_WBNP.name))
        {
            IUUpdateNumber(&m_WBNP, values, names, n);

            int aSub[3] =
            {
                static_cast<int>(m_WBN[TC_WB_R].value),
                static_cast<int>(m_WBN[TC_WB_G].value),
                static_cast<int>(m_WBN[TC_WB_B].value),
            };

            HRESULT rc = FP(put_WhiteBalanceGain(m_Handle, aSub));
            if (SUCCEEDED(rc))
                m_WBNP.s = IPS_OK;
            else
            {
                m_WBNP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set white balance. %s", errorCodes(rc).c_str());
            }

            IDSetNumber(&m_WBNP, nullptr);
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

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupBase::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        //////////////////////////////////////////////////////////////////////
        /// Binning
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_BinningModeSP.name))
        {
            IUUpdateSwitch(&m_BinningModeSP, states, names, n);
            auto mode = (m_BinningModeS[TC_BINNING_AVG].s == ISS_ON) ? TC_BINNING_AVG : TC_BINNING_ADD;
            m_BinningMode = mode;
            updateBinningMode(PrimaryCCD.getBinX(), mode);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Cooler
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_CoolerSP.name))
        {
            IUUpdateSwitch(&m_CoolerSP, states, names, n);
            activateCooler(m_CoolerS[INDI_ENABLED].s == ISS_ON);
            saveConfig(true, m_CoolerSP.name);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// High Fullwell
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_HighFullwellSP.name))
        {
            int prevIndex = IUFindOnSwitchIndex(&m_HighFullwellSP);
            IUUpdateSwitch(&m_HighFullwellSP, states, names, n);
            HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_HIGH_FULLWELL), m_HighFullwellS[INDI_ENABLED].s));
            if (SUCCEEDED(rc))
                m_HighFullwellSP.s = IPS_OK;
            else
            {
                LOGF_ERROR("Failed to set high fullwell %s. %s", m_HighFullwellS[INDI_ENABLED].s == ISS_ON ? "ON" : "OFF",
                           errorCodes(rc).c_str());
                m_HighFullwellSP.s = IPS_ALERT;
                IUResetSwitch(&m_HighFullwellSP);
                m_HighFullwellS[prevIndex].s = ISS_ON;
            }

            IDSetSwitch(&m_HighFullwellSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Low Noise
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_LowNoiseSP.name))
        {
            int prevIndex = IUFindOnSwitchIndex(&m_LowNoiseSP);
            IUUpdateSwitch(&m_LowNoiseSP, states, names, n);
            HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_LOW_NOISE), m_LowNoiseS[INDI_ENABLED].s));
            if (SUCCEEDED(rc))
                m_LowNoiseSP.s = IPS_OK;
            else
            {
                LOGF_ERROR("Failed to set low noise %s. %s", m_LowNoiseS[INDI_ENABLED].s == ISS_ON ? "ON" : "OFF", errorCodes(rc).c_str());
                m_LowNoiseSP.s = IPS_ALERT;
                IUResetSwitch(&m_LowNoiseSP);
                m_LowNoiseS[prevIndex].s = ISS_ON;
            }

            IDSetSwitch(&m_LowNoiseSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Auto Exposure
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_AutoExposureSP.name))
        {
            IUUpdateSwitch(&m_AutoExposureSP, states, names, n);
            m_AutoExposureSP.s = IPS_OK;
            FP(put_AutoExpoEnable(m_Handle, m_AutoExposureS[INDI_ENABLED].s == ISS_ON ? 1 : 0));
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
            FP(put_Option(m_Handle, CP(OPTION_CG), IUFindOnSwitchIndex(&m_GainConversionSP)));
            IDSetSwitch(&m_GainConversionSP, nullptr);
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
            FP(Stop(m_Handle));

            HRESULT rc = FP(put_eSize(m_Handle, targetIndex));
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
                PrimaryCCD.setFrame(0, 0, m_Instance->model->res[targetIndex].width, m_Instance->model->res[targetIndex].height);
                LOGF_INFO("Resolution changed to %s", m_ResolutionS[targetIndex].label);
                allocateFrameBuffer();
                m_ConfigResolutionIndex = targetIndex;
                saveConfig(true, m_ResolutionSP.name);
            }

            IDSetSwitch(&m_ResolutionSP, nullptr);

            // Restart capture
            rc = FP(StartPullModeWithCallback(m_Handle, &ToupBase::eventCB, this));
            if (FAILED(rc))
                LOGF_ERROR("Failed to start camera. %s", errorCodes(rc).c_str());
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Auto White Balance
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_WBAutoSP.name))
        {
            IUUpdateSwitch(&m_WBAutoSP, states, names, n);
            HRESULT rc = FP(AwbInit(m_Handle, nullptr, nullptr));
            IUResetSwitch(&m_WBAutoSP);
            if (SUCCEEDED(rc))
            {
                LOG_INFO("Auto white balance once");
                m_WBAutoSP.s = IPS_OK;
            }
            else
            {
                LOGF_ERROR("Failed to auto white balance. %s", errorCodes(rc).c_str());
                m_WBAutoSP.s = IPS_ALERT;
            }

            IDSetSwitch(&m_WBAutoSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Auto Black Balance
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_BBAutoSP.name))
        {
            IUUpdateSwitch(&m_BBAutoSP, states, names, n);
            HRESULT rc = FP(AbbOnce(m_Handle, nullptr, nullptr));
            IUResetSwitch(&m_BBAutoSP);
            if (SUCCEEDED(rc))
            {
                LOG_INFO("Auto black balance once");
                m_BBAutoSP.s = IPS_OK;
            }
            else
            {
                LOGF_ERROR("Failed to auto black balance. %s", errorCodes(rc).c_str());
                m_BBAutoSP.s = IPS_ALERT;
            }

            IDSetSwitch(&m_BBAutoSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Fan
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_FanSP.name))
        {
            IUUpdateSwitch(&m_FanSP, states, names, n);
            HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_FAN), IUFindOnSwitchIndex(&m_FanSP)));
            if (SUCCEEDED(rc))
                m_FanSP.s = IPS_OK;
            else
            {
                m_FanSP.s = IPS_ALERT;
                LOGF_ERROR("Failed to set fan. %s", errorCodes(rc).c_str());
            }
            IDSetSwitch(&m_FanSP, nullptr);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Heat
        //////////////////////////////////////////////////////////////////////
        if (!strcmp(name, m_HeatSP.name))
        {
            IUUpdateSwitch(&m_HeatSP, states, names, n);
            HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_HEAT), IUFindOnSwitchIndex(&m_HeatSP)));
            if (SUCCEEDED(rc))
                m_HeatSP.s = IPS_OK;
            else
            {
                LOGF_ERROR("Failed to set heat. %s", errorCodes(rc).c_str());
                m_HeatSP.s = IPS_ALERT;
            }
            IDSetSwitch(&m_HeatSP, nullptr);
            return true;
        }
    }

    return INDI::CCD::ISNewSwitch(dev, name, states, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupBase::StartStreaming()
{
    const uint32_t uSecs = static_cast<uint32_t>(1000000.0f / Streamer->getTargetFPS());
    HRESULT rc = FP(put_ExpoTime(m_Handle, uSecs));
    if (FAILED(rc))
    {
        LOGF_ERROR("Failed to set streaming exposure time. %s", errorCodes(rc).c_str());
        return false;
    }

    rc = FP(put_Option(m_Handle, CP(OPTION_TRIGGER), 0));
    if (FAILED(rc))
    {
        LOGF_ERROR("Failed to set trigger mode. %s", errorCodes(rc).c_str());
        return false;
    }
    m_CurrentTriggerMode = TRIGGER_VIDEO;

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupBase::StopStreaming()
{
    HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_TRIGGER), 1));
    if (FAILED(rc))
    {
        LOGF_ERROR("Failed to set trigger mode. %s", errorCodes(rc).c_str());
        return false;
    }
    m_CurrentTriggerMode = TRIGGER_SOFTWARE;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
int ToupBase::SetTemperature(double temperature)
{
    if (activateCooler(true) == false)
    {
        LOG_ERROR("Failed to activate cooler");
        return -1;
    }

    HRESULT rc = FP(put_Temperature(m_Handle, static_cast<int16_t>(temperature * 10.0)));
    if (FAILED(rc))
    {
        LOGF_ERROR("Failed to set temperature. %s", errorCodes(rc).c_str());
        return -1;
    }

    LOGF_INFO("Set CCD temperature to %.1fC", temperature);
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupBase::activateCooler(bool enable)
{
    HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_TEC), enable ? 1 : 0));
    IUResetSwitch(&m_CoolerSP);
    if (FAILED(rc))
    {
        m_CoolerS[enable ? INDI_DISABLED : INDI_ENABLED].s = ISS_ON;
        m_CoolerSP.s = IPS_ALERT;
        LOGF_ERROR("Failed to turn cooler %s. %s", enable ? "ON" : "OFF", errorCodes(rc).c_str());
        IDSetSwitch(&m_CoolerSP, nullptr);
        return false;
    }
    else
    {
        m_CoolerS[enable ? INDI_ENABLED : INDI_DISABLED].s = ISS_ON;
        m_CoolerSP.s = IPS_OK;
        IDSetSwitch(&m_CoolerSP, nullptr);

        /* turn on TEC may force to turn on the fan */
        if (enable && (m_Instance->model->flag & CP(FLAG_FAN)))
        {
            int fan = 0;
            FP(get_Option(m_Handle, CP(OPTION_FAN), &fan));
            IUResetSwitch(&m_FanSP);
            for (unsigned i = 0; i <= m_Instance->model->maxfanspeed; ++i)
                m_FanS[i].s = (fan == static_cast<int>(i)) ? ISS_ON : ISS_OFF;
            IDSetSwitch(&m_FanSP, nullptr);
        }

        return true;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupBase::StartExposure(float duration)
{
    PrimaryCCD.setExposureDuration(static_cast<double>(duration));

    HRESULT rc = 0;
    time_t uSecs = static_cast<time_t>(duration * 1000000.0f);

    m_ExposureRequest = duration;
    if (FAILED(rc = FP(put_ExpoTime(m_Handle, uSecs))))
    {
        LOGF_ERROR("Failed to set exposure time. %s", errorCodes(rc).c_str());
        return false;
    }

    if (m_CurrentTriggerMode != TRIGGER_SOFTWARE)
    {
        rc = FP(put_Option(m_Handle, CP(OPTION_TRIGGER), 1));
        if (FAILED(rc))
            LOGF_ERROR("Failed to set software trigger mode. %s", errorCodes(rc).c_str());
        m_CurrentTriggerMode = TRIGGER_SOFTWARE;
    }

    timeval current_time, exposure_time;
    exposure_time.tv_sec = uSecs / 1000000;
    exposure_time.tv_usec = uSecs % 1000000;
    gettimeofday(&current_time, nullptr);
    timeradd(&current_time, &exposure_time, &m_ExposureEnd);

    InExposure = true;
    if (FAILED(rc = FP(Trigger(m_Handle, 1))))// Trigger an exposure
    {
        LOGF_ERROR("Failed to trigger exposure. %s", errorCodes(rc).c_str());
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupBase::AbortExposure()
{
    FP(Trigger(m_Handle, 0));
    InExposure = false;
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupBase::UpdateCCDFrame(int x, int y, int w, int h)
{
    // Make sure all are even
    x -= x % 2;
    y -= y % 2;
    w -= w % 2;
    h -= h % 2;

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

    HRESULT rc = FP(put_Roi(m_Handle, x, y, w, h));
    if (FAILED(rc))
    {
        LOGF_ERROR("Failed to set camera ROI: %d", rc);
        return false;
    }

    // Set UNBINNED coords
    PrimaryCCD.setFrame(x, y, w, h);

    // Total bytes required for image buffer
    uint32_t nbuf = (w * h * PrimaryCCD.getBPP() / 8) * m_Channels;
    LOGF_DEBUG("Updating frame buffer size to %d bytes", nbuf);
    PrimaryCCD.setFrameBufferSize(nbuf);

    // Always set BINNED size
    Streamer->setSize(w / PrimaryCCD.getBinX(), h / PrimaryCCD.getBinY());
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupBase::updateBinningMode(int binx, int mode)
{
    int binningMode = binx;

    if ((mode == TC_BINNING_AVG) && (binx > 1))
        binningMode = binx | 0x80;

    HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_BINNING), binningMode));
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

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupBase::UpdateCCDBin(int binx, int biny)
{
    if (binx != biny)
    {
        LOG_ERROR("Binning dimensions must be equal");
        return false;
    }

    return updateBinningMode(binx, m_BinningMode);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
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
    if (m_Instance->model->flag & CP(FLAG_GETTEMPERATURE))
    {
        int16_t currentTemperature = (int16_t)(TemperatureN[0].value * 10);
        int16_t nTemperature = currentTemperature;
        HRESULT rc = FP(get_Temperature(m_Handle, &nTemperature));
        if (FAILED(rc))
        {
            LOGF_ERROR("get Temperature error. %s", errorCodes(rc).c_str());
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
    if (HasCooler() && (m_maxTecVoltage > 0))
    {
        int val = 0;
        HRESULT rc = FP(get_Option(m_Handle, CP(OPTION_TEC), &val));
        if (FAILED(rc))
            m_CoolerTP.s = IPS_ALERT;
        else if (0 == val)
        {
            IUSaveText(&m_CoolerT, "0.0% (OFF)");
            IDSetText(&m_CoolerTP, nullptr);
        }
        else
        {
            rc = FP(get_Option(m_Handle, CP(OPTION_TEC_VOLTAGE), &val));
            if (FAILED(rc))
                m_CoolerTP.s = IPS_ALERT;
            else if (val <= m_maxTecVoltage)
            {
                char str[32];
                sprintf(str, "%.1f%%", val * 100.0 / m_maxTecVoltage);
                IUSaveText(&m_CoolerT, str);
                IDSetText(&m_CoolerTP, nullptr);
            }
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

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState ToupBase::guidePulseNS(uint32_t ms, eGUIDEDIRECTION dir, const char *dirName)
{
    stopTimerNS();
    LOGF_DEBUG("Starting %s guide for %d ms", dirName, ms);

    // If pulse < 50ms, we wait. Otherwise, we schedule it.
    int uSecs = ms * 1000;
    HRESULT rc = FP(ST4PlusGuide(m_Handle, dir, ms));
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

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState ToupBase::GuideNorth(uint32_t ms)
{
    return guidePulseNS(ms, TOUPBASE_NORTH, "North");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState ToupBase::guidePulseWE(uint32_t ms, eGUIDEDIRECTION dir, const char *dirName)
{
    stopTimerWE();
    LOGF_DEBUG("Starting %s guide for %d ms", dirName, ms);

    // If pulse < 50ms, we wait. Otherwise, we schedule it.
    int uSecs = ms * 1000;
    HRESULT rc = FP(ST4PlusGuide(m_Handle, dir, ms));
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

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState ToupBase::GuideEast(uint32_t ms)
{
    return guidePulseWE(ms, TOUPBASE_EAST, "East");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState ToupBase::GuideWest(uint32_t ms)
{
    return guidePulseWE(ms, TOUPBASE_WEST, "West");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
const char *ToupBase::getBayerString()
{
    uint32_t nFourCC = 0, nBitDepth = 0;
    FP(get_RawFormat(m_Handle, &nFourCC, &nBitDepth));
    switch (nFourCC)
    {
        case MAKEFOURCC('G', 'B', 'R', 'G'):
            m_CameraPixelFormat = INDI_BAYER_GBRG;
            return "GBRG";
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

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
void ToupBase::refreshControls()
{
    IDSetNumber(&m_ControlNP, nullptr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
void ToupBase::addFITSKeywords(INDI::CCDChip *targetChip, std::vector<INDI::FITSRecord> &fitsKeywords)
{
    INDI::CCD::addFITSKeywords(targetChip, fitsKeywords);

    fitsKeywords.push_back({"GAIN", m_ControlN[TC_GAIN].value, 3, "Gain"});
    fitsKeywords.push_back({"OFFSET", m_OffsetN[0].value, 3, "Offset"});
    if (m_Instance->model->flag & CP(FLAG_LOW_NOISE))
        fitsKeywords.push_back({"LOWNOISE", m_LowNoiseS[INDI_ENABLED].s == ISS_ON ? "ON" : "OFF", "Low Noise"});
    if (m_Instance->model->flag & CP(FLAG_HIGH_FULLWELL))
        fitsKeywords.push_back({"FULLWELL", m_HighFullwellS[INDI_ENABLED].s == ISS_ON ? "ON" : "OFF", "High Fullwell"});
    fitsKeywords.push_back({"SN", m_CameraT[TC_CAMERA_SN].text, "Serial Number"});
    fitsKeywords.push_back({"PRODATE", m_CameraT[TC_CAMERA_DATE].text, "Production Date"});
    fitsKeywords.push_back({"FIRMVER", m_CameraT[TC_CAMERA_FW_VERSION].text, "Firmware Version"});
    fitsKeywords.push_back({"HARDVER", m_CameraT[TC_CAMERA_HW_VERSION].text, "Hardware Version"});
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupBase::saveConfigItems(FILE *fp)
{
    INDI::CCD::saveConfigItems(fp);

    IUSaveConfigNumber(fp, &m_TimeoutFactorNP);
    if (HasCooler())
        IUSaveConfigSwitch(fp, &m_CoolerSP);

    IUSaveConfigNumber(fp, &m_ControlNP);
    IUSaveConfigNumber(fp, &m_OffsetNP);
    IUSaveConfigSwitch(fp, &m_ResolutionSP);
    IUSaveConfigSwitch(fp, &m_BinningModeSP);

    if (m_Instance->model->flag & CP(FLAG_LOW_NOISE))
        IUSaveConfigSwitch(fp, &m_LowNoiseSP);

    if (m_Instance->model->flag & CP(FLAG_HIGH_FULLWELL))
        IUSaveConfigSwitch(fp, &m_HighFullwellSP);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
void ToupBase::eventCB(unsigned event, void* pCtx)
{
    static_cast<ToupBase*>(pCtx)->eventCallBack(event);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t* ToupBase::getRgbBuffer()
{
    if (m_rgbBuffer && (m_rgbBufferSize == PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 3))
        return m_rgbBuffer;
    m_rgbBufferSize = PrimaryCCD.getXRes() * PrimaryCCD.getYRes() * 3;
    m_rgbBuffer = static_cast<uint8_t*>(realloc(m_rgbBuffer, m_rgbBufferSize));
    return m_rgbBuffer;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
void ToupBase::eventCallBack(unsigned event)
{
    LOGF_DEBUG("%s: 0x%08x", __func__, event);
    switch (event)
    {
        case CP(EVENT_EXPOSURE):
        {
            uint16_t expoGain = CP(EXPOGAIN_MIN);
            FP(get_ExpoAGain(m_Handle, &expoGain));
            m_ControlN[TC_GAIN].value = expoGain;
            m_ControlNP.s = IPS_OK;
            IDSetNumber(&m_ControlNP, nullptr);
        }
        break;
        case CP(EVENT_IMAGE):
        {
            int captureBits = m_BitsPerPixel == 8 ? 8 : m_maxBitDepth;
            if (Streamer->isStreaming() || Streamer->isRecording())
            {
                HRESULT rc = FP(PullImageWithRowPitchV2(m_Handle, PrimaryCCD.getFrameBuffer(), captureBits * m_Channels, -1, nullptr));
                if (SUCCEEDED(rc))
                    Streamer->newFrame(PrimaryCCD.getFrameBuffer(), PrimaryCCD.getFrameBufferSize());
            }
            else if (InExposure)
            {
                InExposure = false;
                PrimaryCCD.setExposureLeft(0);

                XP(FrameInfoV2) info;
                memset(&info, 0, sizeof(XP(FrameInfoV2)));

                uint8_t *buffer = PrimaryCCD.getFrameBuffer();
                if (m_MonoCamera == false && (0 == m_CurrentVideoFormat))
                    buffer = getRgbBuffer();

                HRESULT rc = FP(PullImageWithRowPitchV2(m_Handle, buffer, captureBits * m_Channels, -1, &info));
                if (FAILED(rc))
                {
                    LOGF_ERROR("Failed to pull image. %s", errorCodes(rc).c_str());
                    PrimaryCCD.setExposureFailed();
                }
                else
                {
                    if (m_MonoCamera == false && (0 == m_CurrentVideoFormat))
                    {
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
                    }

                    LOGF_DEBUG("Image received. Width: %d, Height: %d, flag: %d, timestamp: %ld", info.width, info.height, info.flag,
                               info.timestamp);
                    ExposureComplete(&PrimaryCCD);
                }
            }
            else
            {
                HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_FLUSH), 3));
                if (FAILED(rc))
                    LOGF_ERROR("Failed to flush image. %s", errorCodes(rc).c_str());
            }
        }
        break;
        case CP(EVENT_WBGAIN):
        {
            int aGain[3] = { 0 };
            FP(get_WhiteBalanceGain)(m_Handle, aGain);
            m_WBN[TC_WB_R].value = aGain[TC_WB_R];
            m_WBN[TC_WB_G].value = aGain[TC_WB_G];
            m_WBN[TC_WB_B].value = aGain[TC_WB_B];
            m_WBNP.s = IPS_OK;
            IDSetNumber(&m_WBNP, nullptr);
        }
        break;
        case CP(EVENT_BLACK):
        {
            unsigned short aSub[3] = { 0 };
            FP(get_BlackBalance)(m_Handle, aSub);
            m_BlackBalanceN[TC_BLACK_R].value = aSub[TC_BLACK_R];
            m_BlackBalanceN[TC_BLACK_G].value = aSub[TC_BLACK_G];
            m_BlackBalanceN[TC_BLACK_B].value = aSub[TC_BLACK_B];
            m_BlackBalanceNP.s = IPS_OK;
            IDSetNumber(&m_BlackBalanceNP, nullptr);
        }
        break;
        case CP(EVENT_ERROR):
            LOG_ERROR("Camera Error");
            break;
        case CP(EVENT_DISCONNECTED):
            LOG_ERROR("Camera disconnected");
            break;
        case CP(EVENT_NOFRAMETIMEOUT):
            LOG_ERROR("Camera timed out");
            PrimaryCCD.setExposureFailed();
            break;
        default:
            break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupBase::SetCaptureFormat(uint8_t index)
{
    // If no changes, ignore request.
    if (m_CurrentVideoFormat == index)
        return true;

    m_Channels = 1;

    if (m_MonoCamera)
    {
        // We need to stop camera first
        FP(Stop(m_Handle));

        HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_BITDEPTH), index));
        if (FAILED(rc))
        {
            LOGF_ERROR("Failed to set high bit depth. %s", errorCodes(rc).c_str());

            // Restart Capture
            rc = FP(StartPullModeWithCallback(m_Handle, &ToupBase::eventCB, this));
            if (FAILED(rc))
                LOGF_ERROR("Failed to start camera. %s", errorCodes(rc).c_str());

            return false;
        }

        m_BitsPerPixel = (index ? 8 : 16);
    }
    // Color
    else
    {
        // We need to stop camera first
        FP(Stop(m_Handle));

        HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_RAW), index));
        if (FAILED(rc))
        {
            LOGF_ERROR("Failed to set raw mode. %s", errorCodes(rc).c_str());

            // Restart Capture
            rc = FP(StartPullModeWithCallback(m_Handle, &ToupBase::eventCB, this));
            if (FAILED(rc))
                LOGF_ERROR("Failed to start camera. %s", errorCodes(rc).c_str());
            return false;
        }

        if (0 == index)
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
            m_BitsPerPixel = (m_maxBitDepth > 8) ? 16 : 8;
        }
    }

    m_CurrentVideoFormat = index;

    int bLevelStep = 1;
    if (m_BitsPerPixel > 8)
        bLevelStep = 1 << (m_maxBitDepth - 8);
    m_OffsetN[0].max = CP(BLACKLEVEL8_MAX) * bLevelStep;
    IUUpdateMinMax(&m_OffsetNP);

    LOGF_DEBUG("Video Format: %d, BitsPerPixel: %d", index, m_BitsPerPixel);

    // Allocate memory
    allocateFrameBuffer();

    // Restart Capture
    HRESULT rc = FP(StartPullModeWithCallback(m_Handle, &ToupBase::eventCB, this));
    if (FAILED(rc))
        LOGF_ERROR("Failed to start camera. %s", errorCodes(rc).c_str());

    return true;
}

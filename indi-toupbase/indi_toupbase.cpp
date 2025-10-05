/*
 Toupcam & oem CCD Driver

 Copyright (C) 2018-2025 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
#include <unordered_map>
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
        std::deque<std::unique_ptr<ToupBase >> cameras;
        XP(DeviceV2) pCameraInfo[CP(MAX)];
    public:
        Loader()
        {
            const int iConnectedCount = FP(EnumV2(pCameraInfo));

            // In case we have identical cameras we need to fix that.
            // e.g. if we have Camera, Camera, it will become
            // Camera, Camera #2
            std::vector<std::string> names;
            for (int i = 0; i < iConnectedCount; i++)
            {
                names.push_back(pCameraInfo[i].model->name);
            }
            if (iConnectedCount > 0)
                fixDuplicates(names);

            for (int i = 0; i < iConnectedCount; i++)
            {
                if ((CP(FLAG_CCD_INTERLACED) | CP(FLAG_CCD_PROGRESSIVE) | CP(FLAG_CMOS)) & pCameraInfo[i].model->flag)
                    cameras.push_back(std::unique_ptr<ToupBase>(new ToupBase(&pCameraInfo[i], names[i])));
            }
            if (cameras.empty())
                IDLog("No camera detected");
        }

        // If duplicate cameras are found, append a number to the camera to set it apart.
        void fixDuplicates(std::vector<std::string> &strings)
        {
            std::unordered_map<std::string, int> stringCounts;

            for (std::string &str : strings)
            {
                if (stringCounts.count(str) > 0)
                {
                    int count = stringCounts[str]++;
                    str += " #" + std::to_string(count + 1);
                    stringCounts[str]++;
                }
                else
                {
                    stringCounts[str] = 1;
                }
            }
        }
} loader;

ToupBase::ToupBase(const XP(DeviceV2) *instance, const std::string &name) : m_Instance(instance)
{
    IDLog("model: %s, name: %s, maxspeed: %d, preview: %d, maxfanspeed: %d", m_Instance->model->name, name.c_str(),
          m_Instance->model->maxspeed,
          m_Instance->model->preview, m_Instance->model->maxfanspeed);

    setVersion(TOUPBASE_VERSION_MAJOR, TOUPBASE_VERSION_MINOR);

    setDeviceName((std::string(DNAME) + " " + name).c_str());

    if (m_Instance->model->flag & CP(FLAG_MONO))
        m_MonoCamera = true;

    mTimerWE.setSingleShot(true);
    mTimerNS.setSingleShot(true);
}

ToupBase::~ToupBase()
{
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
    m_BinningModeSP[TC_BINNING_AVG].fill("TC_BINNING_AVG", "AVG", ISS_OFF);
    m_BinningModeSP[TC_BINNING_ADD].fill("TC_BINNING_ADD", "Add", ISS_ON);
    m_BinningModeSP.fill(getDeviceName(), "CCD_BINNING_MODE", "Binning Mode", IMAGE_SETTINGS_TAB, IP_WO, ISR_1OFMANY, 0,
                         IPS_IDLE);

    if (m_Instance->model->flag & CP(FLAG_TEC_ONOFF))
    {
        ///////////////////////////////////////////////////////////////////////////////////
        /// Cooler Control
        ///////////////////////////////////////////////////////////////////////////////////
        m_CoolerSP[INDI_ENABLED].fill("COOLER_ON", "ON", ISS_ON);
        m_CoolerSP[INDI_DISABLED].fill("COOLER_OFF", "OFF", ISS_OFF);
        m_CoolerSP.fill(getDeviceName(), "CCD_COOLER", "Cooler", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0, IPS_BUSY);

        m_CoolerNP[0].fill("COOLER_POWER", "Percent", "%.f", 0, 100, 10, 0);
        m_CoolerNP.fill(getDeviceName(), "CCD_COOLER_POWER", "Cooler Power", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);
    }

    ///////////////////////////////////////////////////////////////////////////////////
    /// Controls
    ///////////////////////////////////////////////////////////////////////////////////
    m_ControlNP[TC_GAIN].fill("Gain", "Gain", "%.f", CP(EXPOGAIN_MIN), CP(EXPOGAIN_MIN), 1, CP(EXPOGAIN_MIN));
    m_ControlNP[TC_CONTRAST].fill("Contrast", "Contrast", "%.f", CP(CONTRAST_MIN), CP(CONTRAST_MAX), 1, CP(CONTRAST_DEF));
    if (m_MonoCamera)
        nsp = 6;
    else
    {
        nsp = 8;
        m_ControlNP[TC_HUE].fill("Hue", "Hue", "%.f", CP(HUE_MIN), CP(HUE_MAX), 1, CP(HUE_DEF));
        m_ControlNP[TC_SATURATION].fill("Saturation", "Saturation", "%.f", CP(SATURATION_MIN), CP(SATURATION_MAX), 1,
                                        CP(SATURATION_DEF));
    }
    m_ControlNP[TC_BRIGHTNESS].fill("Brightness", "Brightness", "%.f", CP(BRIGHTNESS_MIN), CP(BRIGHTNESS_MAX), 1, 0);
    m_ControlNP[TC_GAMMA].fill("Gamma", "Gamma", "%.f", CP(GAMMA_MIN), CP(GAMMA_MAX), 1, CP(GAMMA_DEF));
    m_ControlNP[TC_SPEED].fill("Speed", "Speed", "%.f", 0, m_Instance->model->maxspeed, 1, 0);
    m_ControlNP[TC_FRAMERATE_LIMIT].fill("FPS Limit", "FPS Limit", "%.f", 0, 63, 1, 0);
    m_ControlNP.resize(nsp);
    m_ControlNP.fill(getDeviceName(), "CCD_CONTROLS", "Controls", CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // Black Balance
    ///////////////////////////////////////////////////////////////////////////////////
    if (m_MonoCamera)
    {
        nsp = 1;
        m_BlackBalanceNP[TC_BLACK_R].fill("TC_BLACK", "Value", "%.f", 0, 255, 1, 0);
    }
    else
    {
        nsp = 3;
        m_BlackBalanceNP[TC_BLACK_R].fill("TC_BLACK_R", "Red", "%.f", 0, 255, 1, 0);
        m_BlackBalanceNP[TC_BLACK_G].fill("TC_BLACK_G", "Green", "%.f", 0, 255, 1, 0);
        m_BlackBalanceNP[TC_BLACK_B].fill("TC_BLACK_B", "Blue", "%.f", 0, 255, 1, 0);
    }
    m_BlackBalanceNP.resize(nsp);
    m_BlackBalanceNP.fill(getDeviceName(), "CCD_BLACK_BALANCE", "Black Balance", CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Auto Black Balance
    ///////////////////////////////////////////////////////////////////////////////////
    m_BBAutoSP[0].fill("TC_AUTO_BB", "Auto", ISS_OFF);
    m_BBAutoSP.fill(getDeviceName(), "TC_AUTO_BB", "Auto Black Balance", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // Black Level (Offset)
    // JM 2023.04.07 DO NOT NAME IT BLACK LEVEL, it must remain as OFFSET
    ///////////////////////////////////////////////////////////////////////////////////
    m_OffsetNP[0].fill("OFFSET", "Value", "%.f", 0, 255, 1, 0);
    m_OffsetNP.fill(getDeviceName(), "CCD_OFFSET", "Offset", CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // R/G/B/Y Level Range
    ///////////////////////////////////////////////////////////////////////////////////
    if (m_MonoCamera)
    {
        nsp = 2;
        m_LevelRangeNP[TC_LO_R].fill("TC_LO", "Low", "%.f", 0, 255, 1, 0);
        m_LevelRangeNP[TC_HI_R].fill("TC_HI", "High", "%.f", 0, 255, 1, 255);
    }
    else
    {
        nsp = 8;
        m_LevelRangeNP[TC_LO_R].fill("TC_LO_R", "Low Red", "%.f", 0, 255, 1, 0);
        m_LevelRangeNP[TC_HI_R].fill("TC_HI_R", "High Red", "%.f", 0, 255, 1, 255);
        m_LevelRangeNP[TC_LO_G].fill("TC_LO_G", "Low Green", "%.f", 0, 255, 1, 0);
        m_LevelRangeNP[TC_HI_G].fill("TC_HI_G", "High Green", "%.f", 0, 255, 1, 255);
        m_LevelRangeNP[TC_LO_B].fill("TC_LO_B", "Low Blue", "%.f", 0, 255, 1, 0);
        m_LevelRangeNP[TC_HI_B].fill("TC_HI_B", "High Blue", "%.f", 0, 255, 1, 255);
        m_LevelRangeNP[TC_LO_Y].fill("TC_LO_Y", "Low Gray", "%.f", 0, 255, 1, 0);
        m_LevelRangeNP[TC_HI_Y].fill("TC_HI_Y", "High Gray", "%.f", 0, 255, 1, 255);
    }
    m_LevelRangeNP.resize(nsp);
    m_LevelRangeNP.fill(getDeviceName(), "CCD_LEVEL_RANGE", "Level Range", CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    // Auto Exposure
    ///////////////////////////////////////////////////////////////////////////////////
    m_AutoExposureSP[INDI_ENABLED].fill("INDI_ENABLED", "ON", ISS_OFF);
    m_AutoExposureSP[INDI_DISABLED].fill("INDI_DISABLED", "OFF", ISS_ON);
    m_AutoExposureSP.fill(getDeviceName(), "CCD_AUTO_EXPOSURE", "Auto Exposure", CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    if (m_MonoCamera == false)
    {
        ///////////////////////////////////////////////////////////////////////////////////
        // White Balance
        ///////////////////////////////////////////////////////////////////////////////////
        m_WBNP[TC_WB_R].fill("TC_WB_R", "Red", "%.f", CP(WBGAIN_MIN), CP(WBGAIN_MAX), 10, CP(WBGAIN_DEF));
        m_WBNP[TC_WB_G].fill("TC_WB_G", "Green", "%.f", CP(WBGAIN_MIN), CP(WBGAIN_MAX), 10, CP(WBGAIN_DEF));
        m_WBNP[TC_WB_B].fill("TC_WB_B", "Blue", "%.f", CP(WBGAIN_MIN), CP(WBGAIN_MAX), 10, CP(WBGAIN_DEF));
        m_WBNP.fill(getDeviceName(), "TC_WB", "White Balance", CONTROL_TAB, IP_RW, 60, IPS_IDLE);

        ///////////////////////////////////////////////////////////////////////////////////
        /// Auto White Balance
        ///////////////////////////////////////////////////////////////////////////////////
        m_WBAutoSP[0].fill("TC_AUTO_WB", "Auto", ISS_OFF);
        m_WBAutoSP.fill(getDeviceName(), "TC_AUTO_WB", "Auto White Balance", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    }

    ///////////////////////////////////////////////////////////////////////////////////
    /// Timeout Factor
    ///////////////////////////////////////////////////////////////////////////////////
    m_TimeoutFactorNP[MINIMAL_TIMEOUT].fill("TIMEOUT_MINIMAL", "Minimum", "%.2f", 0.1, 10, 1, 1);
    m_TimeoutFactorNP[TIMEOUT_FACTOR].fill("TIMEOUT_FACTOR", "Factor", "%.2f", 0, 2, 0.1, 0);
    m_TimeoutFactorNP.fill(getDeviceName(), "TIMEOUT_HANDLING", "Timeout", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);
    m_TimeoutFactorNP.load();

    if (m_Instance->model->flag & (CP(FLAG_CG) | CP(FLAG_CGHDR)))
    {
        ///////////////////////////////////////////////////////////////////////////////////
        /// Conversion Gain
        ///////////////////////////////////////////////////////////////////////////////////
        int nsp = 2;
        m_GainConversionSP[GAIN_LOW].fill("GAIN_LOW", "Low", ISS_OFF);
        m_GainConversionSP[GAIN_HIGH].fill("GAIN_HIGH", "High", ISS_OFF);
        if (m_Instance->model->flag & CP(FLAG_CGHDR))
        {
            m_GainConversionSP[GAIN_HDR].fill("GAIN_HDR", "HDR", ISS_OFF);
            ++nsp;
        }
        m_GainConversionSP.resize(nsp);
        m_GainConversionSP.fill(getDeviceName(), "TC_CONVERSION_GAIN", "Conversion Gain", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60,
                                IPS_IDLE);
    }

    ///////////////////////////////////////////////////////////////////////////////////
    /// Low Noise
    ///////////////////////////////////////////////////////////////////////////////////
    m_LowNoiseSP[INDI_ENABLED].fill("INDI_ENABLED", "ON", ISS_OFF);
    m_LowNoiseSP[INDI_DISABLED].fill("INDI_DISABLED", "OFF", ISS_ON);
    m_LowNoiseSP.fill(getDeviceName(), "TC_LOW_NOISE", "Low Noise Mode", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Tail Light
    m_TailLightSP[INDI_ENABLED].fill("INDI_ENABLED", "ON", ISS_OFF);
    m_TailLightSP[INDI_DISABLED].fill("INDI_DISABLED", "OFF", ISS_ON);
    m_TailLightSP.fill(getDeviceName(), "TC_TAILLIGHT", "Tail Light", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////
    /// High Fullwell
    ///////////////////////////////////////////////////////////////////////////////////
    m_HighFullwellSP[INDI_ENABLED].fill("INDI_ENABLED", "ON", ISS_OFF);
    m_HighFullwellSP[INDI_DISABLED].fill("INDI_DISABLED", "OFF", ISS_ON);
    m_HighFullwellSP.fill(getDeviceName(), "TC_HIGHFULLWELL", "High Fullwell Mode", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60,
                          IPS_IDLE);

    if (m_Instance->model->flag & CP(FLAG_FAN))
    {
        ///////////////////////////////////////////////////////////////////////////////////
        /// Fan
        ///////////////////////////////////////////////////////////////////////////////////
        m_FanSP.resize(m_Instance->model->maxfanspeed + 1);
        m_FanSP[0].fill("INDI_DISABLED", "OFF", ISS_OFF);
        if (m_Instance->model->maxfanspeed <= 1)
            m_FanSP[1].fill("INDI_ENABLED", "ON", ISS_OFF);
        else
        {
            for (uint32_t i = 1; i <= m_Instance->model->maxfanspeed; ++i)
                m_FanSP[i].fill((std::string("FAN_SPEED") + std::to_string(i)).c_str(), std::to_string(i).c_str(), ISS_OFF);
        }
        m_FanSP.fill(getDeviceName(), "TC_FAN_SPEED", (m_Instance->model->maxfanspeed <= 1) ? "Fan" : "Fan Speed", CONTROL_TAB,
                     IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    }

    ///////////////////////////////////////////////////////////////////////////////////
    /// Resolution
    ///////////////////////////////////////////////////////////////////////////////////
    m_ResolutionSP.resize(m_Instance->model->preview);
    for (unsigned i = 0; i < m_Instance->model->preview; i++)
    {
        char label[MAXINDILABEL] = {0};
        snprintf(label, MAXINDILABEL, "%d x %d", m_Instance->model->res[i].width, m_Instance->model->res[i].height);
        m_ResolutionSP[i].fill(label, label, ISS_OFF);
    }
    m_ResolutionSP.fill(getDeviceName(), "CCD_RESOLUTION", "Resolution", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    IUGetConfigOnSwitchIndex(getDeviceName(), m_ResolutionSP.getName(), &m_ConfigResolutionIndex);

    ///////////////////////////////////////////////////////////////////////////////////
    /// Firmware
    ///////////////////////////////////////////////////////////////////////////////////
    m_CameraTP[TC_CAMERA_MODEL].fill("MODEL", "Model", m_Instance->model->name);
    m_CameraTP[TC_CAMERA_DATE].fill("PRODUCTIONDATE", "Production Date", nullptr);
    m_CameraTP[TC_CAMERA_SN].fill("SN", "SN", nullptr);
    m_CameraTP[TC_CAMERA_FW_VERSION].fill("FIRMWAREVERSION", "Firmware Version", nullptr);
    m_CameraTP[TC_CAMERA_HW_VERSION].fill("HARDWAREVERSION", "Hardware Version", nullptr);
    m_CameraTP[TC_CAMERA_FPGA_VERSION].fill("FPGAVERSION", "FPGA Version", nullptr);
    m_CameraTP[TC_CAMERA_REV].fill("REVISION", "Revision", nullptr);
    m_CameraTP.fill(getDeviceName(), "CAMERA", "Camera", INFO_TAB, IP_RO, 0, IPS_IDLE);

    m_SDKVersionTP[0].fill("VERSION", "Version", FP(Version()));
    m_SDKVersionTP.fill(getDeviceName(), "SDK", "SDK", INFO_TAB, IP_RO, 0, IPS_IDLE);

    m_ADCDepthNP[0].fill("BITS", "Bits", "%2.0f", 0, 32, 1, m_maxBitDepth);
    m_ADCDepthNP.fill(getDeviceName(), "ADC_DEPTH", "ADC Depth", IMAGE_INFO_TAB, IP_RO, 60, IPS_IDLE);

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
            defineProperty(m_CoolerSP);
            defineProperty(m_CoolerNP);
        }
        // Even if there is no cooler, we define temperature property as READ ONLY
        else if (m_Instance->model->flag & CP(FLAG_GETTEMPERATURE))
        {
            TemperatureNP.setPermission(IP_RO);
            defineProperty(TemperatureNP);
        }

        if (m_Instance->model->flag & CP(FLAG_FAN))
            defineProperty(m_FanSP);

        defineProperty(m_TimeoutFactorNP);
        defineProperty(m_ControlNP);
        defineProperty(m_AutoExposureSP);
        defineProperty(m_ResolutionSP);

        if (m_Instance->model->flag & CP(FLAG_HIGH_FULLWELL))
            defineProperty(m_HighFullwellSP);

        if (m_Instance->model->flag & CP(FLAG_LOW_NOISE))
            defineProperty(m_LowNoiseSP);

        if (m_Instance->model->flag & CP(FLAG_HEAT))
            defineProperty(m_HeatSP);

        if (m_Instance->model->flag & (CP(FLAG_CG) | CP(FLAG_CGHDR)))
            defineProperty(m_GainConversionSP);

        if (m_SupportTailLight)
            defineProperty(m_TailLightSP);

        // Binning mode
        defineProperty(m_BinningModeSP);
        if (m_MonoCamera == false)
        {
            defineProperty(m_WBNP);
            defineProperty(m_WBAutoSP);
        }
        defineProperty(m_BlackBalanceNP);
        defineProperty(m_BBAutoSP);
        // Levels
        defineProperty(m_LevelRangeNP);
        defineProperty(m_OffsetNP);

        // Firmware
        defineProperty(m_CameraTP);
        defineProperty(m_SDKVersionTP);
        defineProperty(m_ADCDepthNP);
    }
    else
    {
        if (HasCooler())
        {
            deleteProperty(m_CoolerSP);
            deleteProperty(m_CoolerNP);
        }
        else
        {
            deleteProperty(TemperatureNP);
        }

        if (m_Instance->model->flag & CP(FLAG_FAN))
            deleteProperty(m_FanSP);

        deleteProperty(m_TimeoutFactorNP);
        deleteProperty(m_ControlNP);
        deleteProperty(m_AutoExposureSP);
        deleteProperty(m_ResolutionSP);

        if (m_Instance->model->flag & CP(FLAG_LOW_NOISE))
            deleteProperty(m_LowNoiseSP);

        if (m_Instance->model->flag & CP(FLAG_HIGH_FULLWELL))
            deleteProperty(m_HighFullwellSP);

        if (m_Instance->model->flag & CP(FLAG_HEAT))
            deleteProperty(m_HeatSP);

        if (m_Instance->model->flag & (CP(FLAG_CG) | CP(FLAG_CGHDR)))
            deleteProperty(m_GainConversionSP);

        if (m_SupportTailLight)
            deleteProperty(m_TailLightSP);

        deleteProperty(m_BinningModeSP);
        if (m_MonoCamera == false)
        {
            deleteProperty(m_WBNP);
            deleteProperty(m_WBAutoSP);
        }
        deleteProperty(m_BlackBalanceNP);
        deleteProperty(m_BBAutoSP);
        deleteProperty(m_LevelRangeNP);
        deleteProperty(m_OffsetNP);

        deleteProperty(m_CameraTP);
        deleteProperty(m_SDKVersionTP);
        deleteProperty(m_ADCDepthNP);
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

    if (m_Instance->model->flag & CP(FLAG_TEC_ONOFF))
    {
        int tecRange = 0;
        FP(get_Option(m_Handle, CP(OPTION_TECTARGET_RANGE), &tecRange));
        TemperatureNP[0].setMin((static_cast<short>(tecRange & 0xffff)) / 10.0);
        TemperatureNP[0].setMax((static_cast<short>((tecRange >> 16) & 0xffff)) / 10.0);
        TemperatureNP[0].setValue(0); // reasonable default
    }

    {
        int taillight = 0;
        HRESULT rc = FP(get_Option(m_Handle, CP(OPTION_TAILLIGHT), &taillight));
        m_SupportTailLight = SUCCEEDED(rc);
    }

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
    stopGuidePulse(mTimerNS);
    stopGuidePulse(mTimerWE);

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

        m_HeatSP.resize(maxval + 1);
        m_HeatSP[0].fill("INDI_DISABLED", "OFF", (0 == curval) ? ISS_ON : ISS_OFF);
        if (maxval <= 1)
            m_HeatSP[1].fill("INDI_ENABLED", "ON", (1 == curval) ? ISS_ON : ISS_OFF);
        else
        {
            for (int i = 1; i <= maxval; ++i)
                m_HeatSP[i].fill((std::string("HEAT") + std::to_string(i)).c_str(), std::to_string(i).c_str(),
                                 (i == curval) ? ISS_ON : ISS_OFF);
        }
        m_HeatSP.fill(getDeviceName(), "TC_HEAT_CONTROL", "Heat", CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    }

    FP(put_AutoExpoEnable(m_Handle, 0));
    FP(put_Option(m_Handle, CP(OPTION_NOFRAME_TIMEOUT), 1));
    FP(put_Option(m_Handle, CP(OPTION_ZERO_PADDING), 1));

    // Get Firmware Info
    char tmpBuffer[64] = {0};
    uint16_t pRevision = 0;
    FP(get_SerialNumber(m_Handle, tmpBuffer));
    m_CameraTP[TC_CAMERA_SN].setText(tmpBuffer);
    FP(get_ProductionDate(m_Handle, tmpBuffer));
    m_CameraTP[TC_CAMERA_DATE].setText(tmpBuffer);
    FP(get_FwVersion(m_Handle, tmpBuffer));
    m_CameraTP[TC_CAMERA_FW_VERSION].setText(tmpBuffer);
    FP(get_HwVersion(m_Handle, tmpBuffer));
    m_CameraTP[TC_CAMERA_HW_VERSION].setText(tmpBuffer);
    if (FP(get_FpgaVersion(m_Handle, tmpBuffer)) >= 0)
		m_CameraTP[TC_CAMERA_FPGA_VERSION].setText(tmpBuffer);
	else
		m_CameraTP[TC_CAMERA_FPGA_VERSION].setText("NA");	
    FP(get_Revision(m_Handle, &pRevision));
    snprintf(tmpBuffer, 32, "%d", pRevision);
    m_CameraTP[TC_CAMERA_REV].setText(tmpBuffer);

    // Max supported bit depth
    m_maxBitDepth = FP(get_MaxBitDepth(m_Handle));

    FP(get_Option(m_Handle, CP(OPTION_TEC_VOLTAGE_MAX), &m_maxTecVoltage));

    m_BitsPerPixel = 8;

    if (m_maxBitDepth > 8)
    {
        FP(put_Option(m_Handle, CP(OPTION_BITDEPTH), 1));// enable bitdepth
        m_BitsPerPixel = 16;
    }

    uint32_t nBitDepth = 0;
    FP(get_RawFormat(m_Handle, nullptr, &nBitDepth));
    m_ADCDepthNP[0].setValue(nBitDepth);

    FP(put_Option(m_Handle, CP(OPTION_RAW), 1));

    if (m_MonoCamera)// Check if mono camera
    {
        CaptureFormat mono16 = {"INDI_MONO_16", "Mono 16", 16, false};
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
        CaptureFormat raw = {"INDI_RAW", (m_maxBitDepth > 8) ? "RAW 16" : "RAW 8", static_cast<uint8_t>((m_maxBitDepth > 8) ? 16 : 8), true };

        m_Channels = 1;
        BayerTP[2].setText(getBayerString());// Get RAW Format

        addCaptureFormat(rgb);
        addCaptureFormat(raw);
    }

    PrimaryCCD.setNAxis(m_Channels == 1 ? 2 : 3);

    // Fan
    if (m_Instance->model->flag & CP(FLAG_FAN))
    {
        int fan = 0;
        FP(get_Option(m_Handle, CP(OPTION_FAN), &fan));
        m_FanSP.reset();
        for (unsigned i = 0; i <= m_Instance->model->maxfanspeed; ++i)
            m_FanSP[i].setState((fan == static_cast<int>(i)) ? ISS_ON : ISS_OFF);
    }

    // Get active resolution index
    uint32_t currentResolutionIndex = 0, finalResolutionIndex = 0;
    FP(get_eSize(m_Handle, &currentResolutionIndex));
    // If we have a config resolution index, then prefer it over the current resolution index.
    finalResolutionIndex = (m_ConfigResolutionIndex >= 0
                            && m_ConfigResolutionIndex < static_cast<int>(m_ResolutionSP.size())) ? m_ConfigResolutionIndex : currentResolutionIndex;
    // In case there is NO previous resolution set
    // then select the LOWER resolution on arm architecture
    // since this has less chance of failure. If the user explicitly selects any resolution
    // it would be saved in the config and this will not apply.
    // JM 2025.08.19: Disabled this restriction, we should get full resolution on ARM as well
    // #if defined(__arm__) || defined (__aarch64__)
    //     if (m_ConfigResolutionIndex == -1)
    //         finalResolutionIndex = m_ResolutionSP.size() - 1;
    // #endif
    m_ResolutionSP[finalResolutionIndex].setState(ISS_ON);
    // If final resolution index different from current, let's set it.
    if (finalResolutionIndex != currentResolutionIndex)
        FP(put_eSize(m_Handle, finalResolutionIndex));

    SetCCDParams(m_Instance->model->res[finalResolutionIndex].width, m_Instance->model->res[finalResolutionIndex].height,
                 m_BitsPerPixel, m_Instance->model->xpixsz, m_Instance->model->ypixsz);

    // Set trigger mode to software
    rc = FP(put_Option(m_Handle, CP(OPTION_TRIGGER), m_CurrentTriggerMode));
    if (FAILED(rc))
        LOGF_ERROR("Failed to set software trigger mode. %s", errorCodes(rc).c_str());

    // Set tail light status
    if (m_SupportTailLight)
    {
        int currentTailLightValue, configuredTailLightValue = 0;
        rc = FP(get_Option(m_Handle, CP(OPTION_TAILLIGHT), &currentTailLightValue));
        if (FAILED(rc))
        {
            LOGF_ERROR("Failed to get camera tail light status. %s", errorCodes(rc).c_str());
        }
        configuredTailLightValue = m_TailLightSP.findOnSwitchIndex();
        if (currentTailLightValue != configuredTailLightValue)
        {
            rc = FP(put_Option(m_Handle, CP(OPTION_TAILLIGHT), configuredTailLightValue));
            if (FAILED(rc))
            {
                m_TailLightSP.setState(IPS_ALERT);
                LOGF_ERROR("Failed to set camera tail light status. %s", errorCodes(rc).c_str());
                m_TailLightSP.apply();
            }
        }
    }

    // Get CCD Controls values
    int conversionGain = 0;
    FP(get_Option(m_Handle, CP(OPTION_CG), &conversionGain));
    m_GainConversionSP[conversionGain].setState(ISS_ON);

    uint16_t nMax = 0, nDef = 0;
    // Gain
    FP(get_ExpoAGainRange(m_Handle, nullptr, &nMax, &nDef));
    m_ControlNP[TC_GAIN].setMax(nMax);
    m_ControlNP[TC_GAIN].setValue(nDef);

    int nVal = 0;
    // Contrast
    FP(get_Contrast(m_Handle, &nVal));
    m_ControlNP[TC_CONTRAST].setValue(nVal);

    if (m_MonoCamera == false)
    {
        // Hue
        FP(get_Hue(m_Handle, &nVal));
        m_ControlNP[TC_HUE].setValue(nVal);

        // Saturation
        FP(get_Saturation(m_Handle, &nVal));
        m_ControlNP[TC_SATURATION].setValue(nVal);
    }

    // Brightness
    FP(get_Brightness(m_Handle, &nVal));
    m_ControlNP[TC_BRIGHTNESS].setValue(nVal);

    // Gamma
    FP(get_Gamma(m_Handle, &nVal));
    m_ControlNP[TC_GAMMA].setValue(nVal);

    // Speed
    // JM 2020-05-06: Reduce speed on ARM for all resolutions
#if defined(__arm__) || defined (__aarch64__)
    m_ControlNP[TC_SPEED].setValue(0);
    FP(put_Speed(m_Handle, 0));
#else
    rc = FP(get_Speed(m_Handle, &nDef));
    m_ControlNP[TC_SPEED].setValue(nDef);
#endif

    // Frame Rate
    int frameRateLimit = 0;
    FP(get_Option(m_Handle, CP(OPTION_FRAMERATE), &frameRateLimit));
    // JM 2019-08-19: On ARM, set frame limit to max (63) instead of 0 (unlimited)
    // since that results in failure to capture from large sensors
#ifdef __arm__
    frameRateLimit = m_ControlNP[TC_FRAMERATE_LIMIT].getMax();
    FP(put_Option(m_Handle, CP(OPTION_FRAMERATE), frameRateLimit));
#endif
    m_ControlNP[TC_FRAMERATE_LIMIT].setValue(frameRateLimit);

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
            m_WBNP[TC_WB_R].setValue(aGain[TC_WB_R]);
            m_WBNP[TC_WB_G].setValue(aGain[TC_WB_G]);
            m_WBNP[TC_WB_B].setValue(aGain[TC_WB_B]);
        }
    }

    // Get Level Ranges
    uint16_t aLow[4] = {0}, aHigh[4] = {255, 255, 255, 255};
    rc = FP(get_LevelRange(m_Handle, aLow, aHigh));
    if (SUCCEEDED(rc))
    {
        if (m_MonoCamera)
        {
            m_LevelRangeNP[TC_LO_R].setValue(aLow[3]);
            m_LevelRangeNP[TC_HI_R].setValue(aHigh[3]);
        }
        else
        {
            m_LevelRangeNP[TC_LO_R].setValue(aLow[0]);
            m_LevelRangeNP[TC_LO_G].setValue(aLow[1]);
            m_LevelRangeNP[TC_LO_B].setValue(aLow[2]);
            m_LevelRangeNP[TC_LO_Y].setValue(aLow[3]);

            m_LevelRangeNP[TC_HI_R].setValue(aHigh[0]);
            m_LevelRangeNP[TC_HI_G].setValue(aHigh[1]);
            m_LevelRangeNP[TC_HI_B].setValue(aHigh[2]);
            m_LevelRangeNP[TC_HI_Y].setValue(aHigh[3]);
        }
    }

    // Get Black Balance
    uint16_t aSub[3] = {0};
    rc = FP(get_BlackBalance(m_Handle, aSub));
    if (SUCCEEDED(rc))
    {
        m_BlackBalanceNP[TC_BLACK_R].setValue(aSub[0]);
        if (m_MonoCamera == false)
        {
            m_BlackBalanceNP[TC_BLACK_G].setValue(aSub[1]);
            m_BlackBalanceNP[TC_BLACK_B].setValue(aSub[2]);
        }
    }

    // Get Black Level
    // Getting the black level option from camera yields the defaut setting
    // Therefore, black level is a saved option
    // Set range of black level based on max bit depth RAW
    int bLevelStep = 1 << (m_maxBitDepth - 8);
    m_OffsetNP[0].setMax(CP(BLACKLEVEL8_MAX) * bLevelStep);

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
        if (m_ControlNP.isNameMatch(name))
        {
            double const old_framerate_limit = m_ControlNP[TC_FRAMERATE_LIMIT].getValue();

            if (m_ControlNP.isUpdated(values, names, n))
            {
                m_ControlNP.update(values, names, n);
                saveConfig(m_ControlNP);
            }
            else
            {
                m_ControlNP.setState(IPS_OK);
                m_ControlNP.apply();
                return true;
            }

            for (uint8_t i = 0; i < m_ControlNP.size(); i++)
            {
                int value = static_cast<int>(m_ControlNP[i].getValue());
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
                        if (value != old_framerate_limit)
                        {
                            if (value == 0)
                                LOG_INFO("FPS rate limit is set to unlimited");
                            else
                                LOGF_INFO("Limiting frame rate to %d FPS", value);
                        }
                        break;

                    default:
                        break;
                }
            }

            m_ControlNP.setState(IPS_OK);
            m_ControlNP.apply();
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Level Range
        //////////////////////////////////////////////////////////////////////
        if (m_LevelRangeNP.isNameMatch(name))
        {
            if (m_LevelRangeNP.isUpdated(values, names, n))
            {
                m_LevelRangeNP.update(values, names, n);
                saveConfig(m_LevelRangeNP);
            }
            else
            {
                m_LevelRangeNP.setState(IPS_OK);
                m_LevelRangeNP.apply();
                return true;
            }

            uint16_t lo[4], hi[4];
            if (m_MonoCamera)
            {
                lo[0] = lo[1] = lo[2] = lo[3] = static_cast<uint16_t>(m_LevelRangeNP[TC_LO_R].getValue());
                hi[0] = hi[1] = hi[2] = hi[3] = static_cast<uint16_t>(m_LevelRangeNP[TC_HI_R].getValue());
            }
            else
            {
                lo[0] = static_cast<uint16_t>(m_LevelRangeNP[TC_LO_R].getValue());
                lo[1] = static_cast<uint16_t>(m_LevelRangeNP[TC_LO_G].getValue());
                lo[2] = static_cast<uint16_t>(m_LevelRangeNP[TC_LO_B].getValue());
                lo[3] = static_cast<uint16_t>(m_LevelRangeNP[TC_LO_Y].getValue());
                hi[0] = static_cast<uint16_t>(m_LevelRangeNP[TC_HI_R].getValue());
                hi[1] = static_cast<uint16_t>(m_LevelRangeNP[TC_HI_G].getValue());
                hi[2] = static_cast<uint16_t>(m_LevelRangeNP[TC_HI_B].getValue());
                hi[3] = static_cast<uint16_t>(m_LevelRangeNP[TC_HI_Y].getValue());
            };

            HRESULT rc = FP(put_LevelRange(m_Handle, lo, hi));
            if (SUCCEEDED(rc))
                m_LevelRangeNP.setState(IPS_OK);
            else
            {
                m_LevelRangeNP.setState(IPS_ALERT);
                LOGF_ERROR("Failed to set level range. %s", errorCodes(rc).c_str());
            }

            m_LevelRangeNP.apply();
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Black Balance
        //////////////////////////////////////////////////////////////////////
        if (m_BlackBalanceNP.isNameMatch(name))
        {
            if (m_BlackBalanceNP.isUpdated(values, names, n))
            {
                m_BlackBalanceNP.update(values, names, n);
                saveConfig(m_BlackBalanceNP);
            }
            else
            {
                m_BlackBalanceNP.setState(IPS_OK);
                m_BlackBalanceNP.apply();
                return true;
            }

            uint16_t aSub[3];
            if (m_MonoCamera)
                aSub[0] = aSub[1] = aSub[2] = static_cast<uint16_t>(m_BlackBalanceNP[TC_BLACK_R].getValue());
            else
            {
                aSub[0] = static_cast<uint16_t>(m_BlackBalanceNP[TC_BLACK_R].getValue());
                aSub[1] = static_cast<uint16_t>(m_BlackBalanceNP[TC_BLACK_G].getValue());
                aSub[2] = static_cast<uint16_t>(m_BlackBalanceNP[TC_BLACK_B].getValue());
            };

            HRESULT rc = FP(put_BlackBalance(m_Handle, aSub));
            if (SUCCEEDED(rc))
                m_BlackBalanceNP.setState(IPS_OK);
            else
            {
                m_BlackBalanceNP.setState(IPS_ALERT);
                LOGF_ERROR("Failed to set black balance. %s", errorCodes(rc).c_str());
            }

            m_BlackBalanceNP.apply();
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Offset (Black Level)
        //////////////////////////////////////////////////////////////////////
        if (m_OffsetNP.isNameMatch(name))
        {
            if (m_OffsetNP.isUpdated(values, names, n))
            {
                m_OffsetNP.update(values, names, n);
                int bLevel = static_cast<uint16_t>(m_OffsetNP[0].getValue());

                HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_BLACKLEVEL), bLevel));
                if (FAILED(rc))
                {
                    m_OffsetNP.setState(IPS_ALERT);
                    LOGF_ERROR("Failed to set offset. %s", errorCodes(rc).c_str());
                }
                else
                {
                    m_OffsetNP.setState(IPS_OK);
                }

                m_OffsetNP.apply();
                saveConfig(m_OffsetNP);
            }
            else
            {
                m_OffsetNP.setState(IPS_OK);
                m_OffsetNP.apply();
            }
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// White Balance
        //////////////////////////////////////////////////////////////////////
        if (m_WBNP.isNameMatch(name))
        {
            if (m_WBNP.isUpdated(values, names, n))
            {
                m_WBNP.update(values, names, n);

                int aSub[3] =
                {
                    static_cast<int>(m_WBNP[TC_WB_R].getValue()),
                    static_cast<int>(m_WBNP[TC_WB_G].getValue()),
                    static_cast<int>(m_WBNP[TC_WB_B].getValue()),
                };

                HRESULT rc = FP(put_WhiteBalanceGain(m_Handle, aSub));
                if (SUCCEEDED(rc))
                    m_WBNP.setState(IPS_OK);
                else
                {
                    m_WBNP.setState(IPS_ALERT);
                    LOGF_ERROR("Failed to set white balance. %s", errorCodes(rc).c_str());
                }

                m_WBNP.apply();
                saveConfig(m_WBNP);
            }
            else
            {
                m_WBNP.setState(IPS_OK);
                m_WBNP.apply();
            }
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Timeout factor
        //////////////////////////////////////////////////////////////////////
        if (m_TimeoutFactorNP.isNameMatch(name))
        {
            if (m_TimeoutFactorNP.isUpdated(values, names, n))
            {
                auto oldFactor = m_TimeoutFactorNP[TIMEOUT_FACTOR].getValue();
                m_TimeoutFactorNP.update(values, names, n);
                auto newFactor = m_TimeoutFactorNP[TIMEOUT_FACTOR].getValue();

                if (oldFactor != newFactor)
                {
                    if (oldFactor == 0 && newFactor != 0)
                        LOG_INFO("Timeout handling is enabled.");
                    else if (oldFactor != 0 && newFactor == 0)
                        LOG_INFO("Timeout handling is disabled.");
                }

                m_TimeoutFactorNP.setState(IPS_OK);
                m_TimeoutFactorNP.apply();
                saveConfig(m_TimeoutFactorNP);
            }
            else
            {
                m_TimeoutFactorNP.setState(IPS_OK);
                m_TimeoutFactorNP.apply();
            }
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
        if (m_BinningModeSP.isNameMatch(name))
        {
            if (m_BinningModeSP.isUpdated(states, names, n))
            {
                m_BinningModeSP.update(states, names, n);
                saveConfig(m_BinningModeSP);
            }
            else
            {
                m_BinningModeSP.setState(IPS_OK);
                m_BinningModeSP.apply();
                return true;
            }

            auto mode = (m_BinningModeSP[TC_BINNING_AVG].getState() == ISS_ON) ? TC_BINNING_AVG : TC_BINNING_ADD;
            m_BinningMode = mode;
            updateBinningMode(PrimaryCCD.getBinX(), mode);
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Cooler
        //////////////////////////////////////////////////////////////////////
        if (m_CoolerSP.isNameMatch(name))
        {
            if (m_CoolerSP.isUpdated(states, names, n))
            {
                m_CoolerSP.update(states, names, n);
                activateCooler(m_CoolerSP[INDI_ENABLED].getState() == ISS_ON);
            }
            else
            {
                m_CoolerSP.setState(IPS_OK);
                m_CoolerSP.apply();
                return true;
            }
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// High Fullwell
        //////////////////////////////////////////////////////////////////////
        if (m_HighFullwellSP.isNameMatch(name))
        {
            int prevIndex = m_HighFullwellSP.findOnSwitchIndex();
            if (m_HighFullwellSP.isUpdated(states, names, n))
            {
                m_HighFullwellSP.update(states, names, n);
                HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_HIGH_FULLWELL), m_HighFullwellSP[INDI_ENABLED].getState()));
                if (SUCCEEDED(rc))
                    m_HighFullwellSP.setState(IPS_OK);
                else
                {
                    LOGF_ERROR("Failed to set high fullwell %s. %s", m_HighFullwellSP[INDI_ENABLED].getState() == ISS_ON ? "ON" : "OFF",
                               errorCodes(rc).c_str());
                    m_HighFullwellSP.setState(IPS_ALERT);
                    m_HighFullwellSP.reset();
                    m_HighFullwellSP[prevIndex].setState(ISS_ON);
                }

                m_HighFullwellSP.apply();
                saveConfig(m_HighFullwellSP);
            }
            else
            {
                m_HighFullwellSP.setState(IPS_OK);
                m_HighFullwellSP.apply();
                return true;
            }
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Low Noise
        //////////////////////////////////////////////////////////////////////
        if (m_LowNoiseSP.isNameMatch(name))
        {
            int prevIndex = m_LowNoiseSP.findOnSwitchIndex();
            if (m_LowNoiseSP.isUpdated(states, names, n))
            {
                m_LowNoiseSP.update(states, names, n);
                HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_LOW_NOISE), m_LowNoiseSP[INDI_ENABLED].getState()));
                if (SUCCEEDED(rc))
                    m_LowNoiseSP.setState(IPS_OK);
                else
                {
                    LOGF_ERROR("Failed to set low noise %s. %s", m_LowNoiseSP[INDI_ENABLED].getState() == ISS_ON ? "ON" : "OFF",
                               errorCodes(rc).c_str());
                    m_LowNoiseSP.setState(IPS_ALERT);
                    m_LowNoiseSP.reset();
                    m_LowNoiseSP[prevIndex].setState(ISS_ON);
                }

                m_LowNoiseSP.apply();
                saveConfig(m_LowNoiseSP);
            }
            else
            {
                m_LowNoiseSP.setState(IPS_OK);
                m_LowNoiseSP.apply();
                return true;
            }
            return true;
        }

        // Tail Light
        if (m_TailLightSP.isNameMatch(name))
        {
            int prevIndex = m_TailLightSP.findOnSwitchIndex();
            if (m_TailLightSP.isUpdated(states, names, n))
            {
                m_TailLightSP.update(states, names, n);
                HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_TAILLIGHT), m_TailLightSP[INDI_ENABLED].getState()));
                if (SUCCEEDED(rc))
                    m_TailLightSP.setState(IPS_OK);
                else
                {
                    LOGF_ERROR("Failed to set tail light %s. %s", m_TailLightSP[INDI_ENABLED].getState() == ISS_ON ? "ON" : "OFF",
                               errorCodes(rc).c_str());
                    m_TailLightSP.setState(IPS_ALERT);
                    m_TailLightSP.reset();
                    m_TailLightSP[prevIndex].setState(ISS_ON);
                }
                m_TailLightSP.apply();
                saveConfig(m_TailLightSP);
            }
            else
            {
                m_TailLightSP.setState(IPS_OK);
                m_TailLightSP.apply();
                return true;
            }
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Auto Exposure
        //////////////////////////////////////////////////////////////////////
        if (m_AutoExposureSP.isNameMatch(name))
        {
            if (m_AutoExposureSP.isUpdated(states, names, n))
            {
                m_AutoExposureSP.update(states, names, n);
                m_AutoExposureSP.setState(IPS_OK);
                FP(put_AutoExpoEnable(m_Handle, m_AutoExposureSP[INDI_ENABLED].getState() == ISS_ON ? 1 : 0));
                m_AutoExposureSP.apply();
                saveConfig(m_AutoExposureSP);
            }
            else
            {
                m_AutoExposureSP.setState(IPS_OK);
                m_AutoExposureSP.apply();
                return true;
            }
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Conversion Gain
        //////////////////////////////////////////////////////////////////////
        if (m_GainConversionSP.isNameMatch(name))
        {
            if (m_GainConversionSP.isUpdated(states, names, n))
            {
                m_GainConversionSP.update(states, names, n);
                m_GainConversionSP.setState(IPS_OK);
                FP(put_Option(m_Handle, CP(OPTION_CG), m_GainConversionSP.findOnSwitchIndex()));
                m_GainConversionSP.apply();
                saveConfig(m_GainConversionSP);
            }
            else
            {
                m_GainConversionSP.setState(IPS_OK);
                m_GainConversionSP.apply();
                return true;
            }
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Resolution
        //////////////////////////////////////////////////////////////////////
        if (m_ResolutionSP.isNameMatch(name))
        {
            if (Streamer->isBusy())
            {
                m_ResolutionSP.setState(IPS_ALERT);
                LOG_ERROR("Cannot change resolution while streaming/recording");
                m_ResolutionSP.apply();
                return true;
            }

            int preIndex = m_ResolutionSP.findOnSwitchIndex();
            if (m_ResolutionSP.isUpdated(states, names, n))
            {
                m_ResolutionSP.update(states, names, n);
                int targetIndex = m_ResolutionSP.findOnSwitchIndex();
                // Stop capture
                FP(Stop(m_Handle));

                HRESULT rc = FP(put_eSize(m_Handle, targetIndex));
                if (FAILED(rc))
                {
                    m_ResolutionSP.setState(IPS_ALERT);
                    m_ResolutionSP.reset();
                    m_ResolutionSP[preIndex].setState(ISS_ON);
                    LOGF_ERROR("Failed to change resolution. %s", errorCodes(rc).c_str());
                }
                else
                {
                    m_ResolutionSP.setState(IPS_OK);
                    PrimaryCCD.setResolution(m_Instance->model->res[targetIndex].width, m_Instance->model->res[targetIndex].height);
                    PrimaryCCD.setFrame(0, 0, m_Instance->model->res[targetIndex].width, m_Instance->model->res[targetIndex].height);
                    LOGF_INFO("Resolution changed to %s", m_ResolutionSP[targetIndex].getLabel());
                    allocateFrameBuffer();
                    m_ConfigResolutionIndex = targetIndex;
                    saveConfig(m_ResolutionSP);
                }

                m_ResolutionSP.apply();

                // Restart capture
                rc = FP(StartPullModeWithCallback(m_Handle, &ToupBase::eventCB, this));
                if (FAILED(rc))
                    LOGF_ERROR("Failed to start camera. %s", errorCodes(rc).c_str());
            }
            else
            {
                m_ResolutionSP.setState(IPS_OK);
                m_ResolutionSP.apply();
                return true;
            }
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Auto White Balance
        //////////////////////////////////////////////////////////////////////
        if (m_WBAutoSP.isNameMatch(name))
        {
            if (m_WBAutoSP.isUpdated(states, names, n))
            {
                m_WBAutoSP.update(states, names, n);
                HRESULT rc = FP(AwbInit(m_Handle, nullptr, nullptr));
                m_WBAutoSP.reset();
                if (SUCCEEDED(rc))
                {
                    LOG_INFO("Auto white balance once");
                    m_WBAutoSP.setState(IPS_OK);
                }
                else
                {
                    LOGF_ERROR("Failed to auto white balance. %s", errorCodes(rc).c_str());
                    m_WBAutoSP.setState(IPS_ALERT);
                }

                m_WBAutoSP.apply();
                saveConfig(m_WBAutoSP);
            }
            else
            {
                m_WBAutoSP.setState(IPS_OK);
                m_WBAutoSP.apply();
                return true;
            }
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Auto Black Balance
        //////////////////////////////////////////////////////////////////////
        if (m_BBAutoSP.isNameMatch(name))
        {
            if (m_BBAutoSP.isUpdated(states, names, n))
            {
                m_BBAutoSP.update(states, names, n);
                HRESULT rc = FP(AbbOnce(m_Handle, nullptr, nullptr));
                m_BBAutoSP.reset();
                if (SUCCEEDED(rc))
                {
                    LOG_INFO("Auto black balance once");
                    m_BBAutoSP.setState(IPS_OK);
                }
                else
                {
                    LOGF_ERROR("Failed to auto black balance. %s", errorCodes(rc).c_str());
                    m_BBAutoSP.setState(IPS_ALERT);
                }

                m_BBAutoSP.apply();
                saveConfig(m_BBAutoSP);
            }
            else
            {
                m_BBAutoSP.setState(IPS_OK);
                m_BBAutoSP.apply();
            }
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Fan
        //////////////////////////////////////////////////////////////////////
        if (m_FanSP.isNameMatch(name))
        {
            if (m_FanSP.isUpdated(states, names, n))
            {
                m_FanSP.update(states, names, n);
                HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_FAN), m_FanSP.findOnSwitchIndex()));
                if (SUCCEEDED(rc))
                    m_FanSP.setState(IPS_OK);
                else
                {
                    m_FanSP.setState(IPS_ALERT);
                    LOGF_ERROR("Failed to set fan. %s", errorCodes(rc).c_str());
                }
                m_FanSP.apply();
                saveConfig(m_FanSP);
            }
            else
            {
                m_FanSP.setState(IPS_OK);
                m_FanSP.apply();
            }
            return true;
        }

        //////////////////////////////////////////////////////////////////////
        /// Heat
        //////////////////////////////////////////////////////////////////////
        if (m_HeatSP.isNameMatch(name))
        {
            if (m_HeatSP.isUpdated(states, names, n))
            {
                m_HeatSP.update(states, names, n);
                HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_HEAT), m_HeatSP.findOnSwitchIndex()));
                if (SUCCEEDED(rc))
                    m_HeatSP.setState(IPS_OK);
                else
                {
                    LOGF_ERROR("Failed to set heat. %s", errorCodes(rc).c_str());
                    m_HeatSP.setState(IPS_ALERT);
                }
                m_HeatSP.apply();
                saveConfig(m_HeatSP);
            }
            else
            {
                m_HeatSP.setState(IPS_OK);
                m_HeatSP.apply();
            }
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
    // JM 2023.11.21: Only activate cooler if the requested temperature is below current temperature
    if (temperature < TemperatureNP[0].getValue() && activateCooler(true) == false)
    {
        LOG_ERROR("Failed to toggle cooler.");
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
    int val = 0;
    auto isCoolerOn = false;
    HRESULT rc = FP(get_Option(m_Handle, CP(OPTION_TEC), &val));
    if (SUCCEEDED(rc))
        isCoolerOn = (val != 0);

    // If no state change, return.
    if ( (enable && isCoolerOn) || (!enable && !isCoolerOn))
        return true;

    rc = FP(put_Option(m_Handle, CP(OPTION_TEC), enable ? 1 : 0));
    m_CoolerSP.reset();
    if (FAILED(rc))
    {
        m_CoolerSP[enable ? INDI_DISABLED : INDI_ENABLED].setState(ISS_ON);
        m_CoolerSP.setState(IPS_ALERT);
        LOGF_ERROR("Failed to turn cooler %s. %s", enable ? "ON" : "OFF", errorCodes(rc).c_str());
        m_CoolerSP.apply();
        return false;
    }
    else
    {
        m_CoolerSP[enable ? INDI_ENABLED : INDI_DISABLED].setState(ISS_ON);
        m_CoolerSP.setState(enable ? IPS_BUSY : IPS_IDLE);
        m_CoolerSP.apply();

        /* turn on TEC may force to turn on the fan */
        if (enable && (m_Instance->model->flag & CP(FLAG_FAN)))
        {
            int fan = 0;
            FP(get_Option(m_Handle, CP(OPTION_FAN), &fan));
            m_FanSP.reset();
            for (unsigned i = 0; i <= m_Instance->model->maxfanspeed; ++i)
                m_FanSP[i].setState((fan == static_cast<int>(i)) ? ISS_ON : ISS_OFF);
            m_FanSP.apply();
        }

        return true;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupBase::StartExposure(float duration)
{
    // Abort any running exposure before starting a new one
    if (InExposure)
    {
        LOG_WARN("Exposure already in progress. Aborting previous exposure before starting a new one.");
        AbortExposure();
    }

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

    m_ExposureTimer.start();

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
        m_BinningModeSP.setState(IPS_ALERT);
        m_BinningModeSP.apply();
        return false;
    }
    else
    {
        m_BinningModeSP.setState(IPS_OK);
        m_BinningModeSP.apply();
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

    // Exposure timeout logic
    if (InExposure)
    {
        double const elapsed = m_ExposureTimer.elapsed() / 1000.0;
        double remaining = m_ExposureRequest > elapsed ? m_ExposureRequest - elapsed : 0;
        PrimaryCCD.setExposureLeft(remaining);

        auto factor = m_TimeoutFactorNP[TIMEOUT_FACTOR].getValue();
        // Timeout check. Do not timeout ever under MINIMAL_TIMEOUT second.
        if (factor > 0 && elapsed > std::max(m_TimeoutFactorNP[MINIMAL_TIMEOUT].getValue(), m_ExposureRequest * factor))
        {
            LOG_ERROR("Exposure timed out waiting for image frame.");
            InExposure = false;
            PrimaryCCD.setExposureFailed();
        }
    }

    if (m_Instance->model->flag & CP(FLAG_GETTEMPERATURE))
    {
        int16_t nTemperature = 0;
        HRESULT rc = FP(get_Temperature(m_Handle, &nTemperature));
        if (FAILED(rc))
        {
            if (TemperatureNP.getState() != IPS_ALERT)
            {
                TemperatureNP.setState(IPS_ALERT);
                TemperatureNP.apply();
                LOGF_ERROR("get Temperature error. %s", errorCodes(rc).c_str());
            }
        }
        else if (TemperatureNP.getState() == IPS_ALERT)
            TemperatureNP.setState(IPS_OK);

        TemperatureNP[0].setValue(nTemperature / 10.0);

        auto threshold = HasCooler() ? 0.1 : 0.2;

        switch (TemperatureNP.getState())
        {
            case IPS_IDLE:
            case IPS_OK:
            case IPS_BUSY:
                if (std::abs(TemperatureNP[0].getValue() - m_LastTemperature) > threshold)
                {
                    m_LastTemperature = TemperatureNP[0].getValue();
                    TemperatureNP.apply();
                }
                break;

            case IPS_ALERT:
                break;
        }
    }

    if (HasCooler() && (m_maxTecVoltage > 0))
    {
        int val = 0;
        HRESULT rc = FP(get_Option(m_Handle, CP(OPTION_TEC), &val));
        if (FAILED(rc))
        {
            if (m_CoolerNP.getState() != IPS_ALERT)
            {
                m_CoolerNP.setState(IPS_ALERT);
                m_CoolerNP.apply();
            }
        }
        else if (0 == val)
        {
            if (m_CoolerNP.getState() != IPS_IDLE)
            {
                m_CoolerNP.setState(IPS_IDLE);
                m_CoolerNP[0].setValue(0);
                m_CoolerNP.apply();
            }
        }
        else
        {
            rc = FP(get_Option(m_Handle, CP(OPTION_TEC_VOLTAGE), &val));
            if (FAILED(rc))
            {
                if (m_CoolerNP.getState() != IPS_ALERT)
                {
                    m_CoolerNP.setState(IPS_ALERT);
                    m_CoolerNP.apply();
                }
            }
            else if (val <= m_maxTecVoltage)
            {
                m_CoolerNP[0].setValue(val * 100.0 / m_maxTecVoltage);
                if (std::abs(m_CoolerNP[0].getValue() - m_LastCoolerPower) > 1)
                {
                    m_LastCoolerPower = m_CoolerNP[0].getValue();
                    m_CoolerNP.setState(IPS_BUSY);
                    m_CoolerNP.apply();
                }
            }
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState ToupBase::guidePulse(INDI::Timer &timer, float ms, eGUIDEDIRECTION dir)
{
    timer.stop();
    HRESULT rc = FP(ST4PlusGuide(m_Handle, dir, ms));
    if (FAILED(rc))
    {
        LOGF_ERROR("%s pulse guiding failed: %s", toString(dir), errorCodes(rc).c_str());
        return IPS_ALERT;
    }

    LOGF_DEBUG("Starting %s guide for %f ms.", toString(dir), ms);

    timer.callOnTimeout([this, dir]
    {
        // BUG in SDK. ST4 guiding pulses do not stop after duration.
        // N.B. Pulse guiding has to be explicitly stopped.
        FP(ST4PlusGuide(m_Handle, TOUPBASE_STOP, 0));
        LOGF_DEBUG("Stopped %s guide.", toString(dir));

        if (dir == TOUPBASE_NORTH || dir == TOUPBASE_SOUTH)
            GuideComplete(AXIS_DE);
        else if (dir == TOUPBASE_EAST || dir == TOUPBASE_WEST)
            GuideComplete(AXIS_RA);
    });

    if (ms < 1)
    {
        usleep(ms * 1000);
        timer.timeout();
        return IPS_OK;
    }

    timer.start(ms);
    return IPS_BUSY;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
void ToupBase::stopGuidePulse(INDI::Timer &timer)
{
    if (timer.isActive())
    {
        timer.stop();
        timer.timeout();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
const char *ToupBase::toString(eGUIDEDIRECTION dir)
{
    switch (dir)
    {
        case TOUPBASE_NORTH:
            return "North";
        case TOUPBASE_SOUTH:
            return "South";
        case TOUPBASE_EAST:
            return "East";
        case TOUPBASE_WEST:
            return "West";
        default:
            return "Stop";
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState ToupBase::GuideNorth(uint32_t ms)
{
    return guidePulse(mTimerNS, ms, TOUPBASE_NORTH);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState ToupBase::GuideSouth(uint32_t ms)
{
    return guidePulse(mTimerNS, ms, TOUPBASE_SOUTH);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState ToupBase::GuideEast(uint32_t ms)
{
    return guidePulse(mTimerWE, ms, TOUPBASE_EAST);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
IPState ToupBase::GuideWest(uint32_t ms)
{
    return guidePulse(mTimerWE, ms, TOUPBASE_WEST);
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
    m_ControlNP.apply();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
void ToupBase::addFITSKeywords(INDI::CCDChip * targetChip, std::vector<INDI::FITSRecord> &fitsKeywords)
{
    INDI::CCD::addFITSKeywords(targetChip, fitsKeywords);

    fitsKeywords.push_back({"GAIN", m_ControlNP[TC_GAIN].getValue(), 3, "Gain"});
    fitsKeywords.push_back({"OFFSET", m_OffsetNP[0].getValue(), 3, "Offset"});
    if (m_Instance->model->flag & CP(FLAG_LOW_NOISE))
        fitsKeywords.push_back({"LOWNOISE", m_LowNoiseSP[INDI_ENABLED].getState() == ISS_ON ? "ON" : "OFF", "Low Noise"});
    if (m_Instance->model->flag & CP(FLAG_HIGH_FULLWELL))
        fitsKeywords.push_back({"FULLWELL", m_HighFullwellSP[INDI_ENABLED].getState() == ISS_ON ? "ON" : "OFF", "High Fullwell"});
    fitsKeywords.push_back({"SN", m_CameraTP[TC_CAMERA_SN].getText(), "Serial Number"});
    fitsKeywords.push_back({"PRODATE", m_CameraTP[TC_CAMERA_DATE].getText(), "Production Date"});
    fitsKeywords.push_back({"FIRMVER", m_CameraTP[TC_CAMERA_FW_VERSION].getText(), "Firmware Version"});
    fitsKeywords.push_back({"HARDVER", m_CameraTP[TC_CAMERA_HW_VERSION].getText(), "Hardware Version"});
    fitsKeywords.push_back({"FPGAVER", m_CameraTP[TC_CAMERA_FPGA_VERSION].getText(), "FPGA Version"});
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupBase::saveConfigItems(FILE * fp)
{
    INDI::CCD::saveConfigItems(fp);

    m_TimeoutFactorNP.save(fp);

    m_ControlNP.save(fp);
    m_OffsetNP.save(fp);
    m_ResolutionSP.save(fp);
    m_BinningModeSP.save(fp);

    if (m_Instance->model->flag & CP(FLAG_LOW_NOISE))
        m_LowNoiseSP.save(fp);

    if (m_Instance->model->flag & CP(FLAG_HIGH_FULLWELL))
        m_HighFullwellSP.save(fp);

    if (m_Instance->model->flag & CP(FLAG_FAN))
        m_FanSP.save(fp);

    m_LevelRangeNP.save(fp);
    m_BlackBalanceNP.save(fp);
    m_OffsetNP.save(fp);
    if (m_MonoCamera == false)
    {
        m_WBNP.save(fp);
        m_WBAutoSP.save(fp);
    }
    if (m_SupportTailLight)
        m_TailLightSP.save(fp);
    m_AutoExposureSP.save(fp);
    if (m_Instance->model->flag & (CP(FLAG_CG) | CP(FLAG_CGHDR)))
        m_GainConversionSP.save(fp);
    m_BBAutoSP.save(fp);
    if (m_Instance->model->flag & CP(FLAG_HEAT))
        m_HeatSP.save(fp);

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
            m_ControlNP[TC_GAIN].setValue(expoGain);
            m_ControlNP.setState(IPS_OK);
            m_ControlNP.apply();
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
            m_WBNP[TC_WB_R].setValue(aGain[TC_WB_R]);
            m_WBNP[TC_WB_G].setValue(aGain[TC_WB_G]);
            m_WBNP[TC_WB_B].setValue(aGain[TC_WB_B]);
            m_WBNP.setState(IPS_OK);
            m_WBNP.apply();
        }
        break;
        case CP(EVENT_BLACK):
        {
            unsigned short aSub[3] = { 0 };
            FP(get_BlackBalance)(m_Handle, aSub);
            m_BlackBalanceNP[TC_BLACK_R].setValue(aSub[TC_BLACK_R]);
            m_BlackBalanceNP[TC_BLACK_G].setValue(aSub[TC_BLACK_G]);
            m_BlackBalanceNP[TC_BLACK_B].setValue(aSub[TC_BLACK_B]);
            m_BlackBalanceNP.setState(IPS_OK);
            m_BlackBalanceNP.apply();
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

        m_BitsPerPixel = index ? 8 : 16;
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
            BayerTP[2].setText(getBayerString());
            BayerTP.apply();
            m_BitsPerPixel = (m_maxBitDepth > 8) ? 16 : 8;
        }
    }

    m_CurrentVideoFormat = index;

    uint32_t nBitDepth = 0;
    FP(get_RawFormat(m_Handle, nullptr, &nBitDepth));
    m_ADCDepthNP[0].setValue(nBitDepth);

    int bLevelStep = 1;
    if (m_BitsPerPixel > 8)
        bLevelStep = 1 << (m_maxBitDepth - 8);
    m_OffsetNP[0].setMax(CP(BLACKLEVEL8_MAX) * bLevelStep);
    m_OffsetNP.updateMinMax();

    LOGF_DEBUG("Video Format: %d, BitsPerPixel: %d", index, m_BitsPerPixel);

    // Allocate memory
    allocateFrameBuffer();

    // Restart Capture
    HRESULT rc = FP(StartPullModeWithCallback(m_Handle, &ToupBase::eventCB, this));
    if (FAILED(rc))
        LOGF_ERROR("Failed to start camera. %s", errorCodes(rc).c_str());

    return true;
}

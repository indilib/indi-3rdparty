/*
 Toupcam & oem Filter Wheel Driver

 Copyright (C) 2018-2024 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "indi_toupwheel.h"
#include "config.h"
#include <unistd.h>
#include <deque>

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
static class Loader
{
        std::deque<std::unique_ptr<ToupWheel>> wheels;
        XP(DeviceV2) pWheelInfo[CP(MAX)];
    public:
        Loader()
        {
            const int iConnectedCount = FP(EnumV2(pWheelInfo));
            for (int i = 0; i < iConnectedCount; i++)
            {
                if (CP(FLAG_FILTERWHEEL) & pWheelInfo[i].model->flag)
                {
                    std::string name = std::string(DNAME) + " EFW";
                    if (iConnectedCount > 1)
                        name += " " + std::to_string(i + 1);
                    wheels.push_back(std::unique_ptr<ToupWheel>(new ToupWheel(&pWheelInfo[i], name.c_str())));
                }
            }
            if (wheels.empty())
                IDLog("No filter wheels detected.");
        }
} loader;

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
ToupWheel::ToupWheel(const XP(DeviceV2) *instance, const char *name) : m_Instance(instance)
{
    setVersion(TOUPBASE_VERSION_MAJOR, TOUPBASE_VERSION_MINOR);
    setDeviceName(name);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
const char *ToupWheel::getDefaultName()
{
    return DNAME;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupWheel::initProperties()
{
    INDI::FilterWheel::initProperties();

    VersionTP[TC_FW_VERSION].fill("FIRMWARE", "Firmware", nullptr);
    VersionTP[TC_HW_VERSION].fill("HARDWARE", "Hardware", nullptr);
    VersionTP[TC_REV].fill("REVISION", "Revision", nullptr);
    VersionTP[TC_SDK].fill("SDK", "SDK", FP(Version()));
    VersionTP.fill(getDeviceName(), "VERSION", "Version", INFO_TAB, IP_RO, 0, IPS_IDLE);

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupWheel::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        char tmpBuffer[64] = {0};
        uint16_t pRevision = 0;
        FP(get_FwVersion(m_Handle, tmpBuffer));
        IUSaveText(&VersionTP[TC_FW_VERSION], tmpBuffer);
        FP(get_HwVersion(m_Handle, tmpBuffer));
        IUSaveText(&VersionTP[TC_HW_VERSION], tmpBuffer);
        FP(get_Revision(m_Handle, &pRevision));
        snprintf(tmpBuffer, 32, "%d", pRevision);
        IUSaveText(&VersionTP[TC_REV], tmpBuffer);

        defineProperty(VersionTP);
    }
    else
    {
        deleteProperty(VersionTP);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupWheel::Connect()
{
    m_Handle = FP(Open(m_Instance->id));

    if (m_Handle == nullptr)
    {
        LOG_ERROR("Failed to connect filterwheel");
        return false;
    }

    int val = 5;
    HRESULT rc = FP(get_Option(m_Handle, CP(OPTION_FILTERWHEEL_SLOT), &val));
    if (SUCCEEDED(rc))
    {
        LOGF_INFO("Detected %d-slot filter wheel.", val);

        FilterSlotN[0].max = val;
        QueryFilter();
    }
    else
    {
        LOGF_ERROR("Failed to get # of filter slots: %s", errorCodes(rc).c_str());
    }

    LOGF_INFO("%s is connected.", getDeviceName());
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupWheel::Disconnect()
{
    FP(Close(m_Handle));
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
void ToupWheel::TimerHit()
{
    QueryFilter();

    if (CurrentFilter != TargetFilter)
    {
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        SelectFilterDone(CurrentFilter);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupWheel::SelectFilter(int targetFilter)
{
    HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_FILTERWHEEL_POSITION), targetFilter - 1));
    if (FAILED(rc))
    {
        LOGF_ERROR("Failed to select filter wheel %d. %s", targetFilter, errorCodes(rc).c_str());
        return false;
    }
    SetTimer(getCurrentPollingPeriod());
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
int ToupWheel::QueryFilter()
{
    HRESULT rc = FP(get_Option(m_Handle, CP(OPTION_FILTERWHEEL_POSITION), &CurrentFilter));
    if (FAILED(rc))
    {
        LOGF_ERROR("Failed to query filter wheel. %s", errorCodes(rc).c_str());
        return 0;
    }
    else
        CurrentFilter++;

    return CurrentFilter;
}

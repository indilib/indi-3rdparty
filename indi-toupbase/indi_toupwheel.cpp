/*
 Toupcam & oem Filter Wheel Driver

 Copyright (C) 2018 Jasem Mutlaq (mutlaqja@ikarustech.com)

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
                wheels.push_back(std::unique_ptr<ToupWheel>(new ToupWheel(&pWheelInfo[i])));
        }
        if (wheels.empty())
            IDLog("No filterwheel detected");
    }
} loader;

static const int SlotNum[SLOT_NUM] = { 5, 7, 8 };

ToupWheel::ToupWheel(const XP(DeviceV2) *instance) : m_Instance(instance)
{
    setVersion(TOUPBASE_VERSION_MAJOR, TOUPBASE_VERSION_MINOR);

    snprintf(this->m_name, MAXINDIDEVICE, "%s %s", getDefaultName(), m_Instance->model->name);
    setDeviceName(this->m_name);
}

const char *ToupWheel::getDefaultName()
{
    return DNAME;
}

bool ToupWheel::initProperties()
{
    INDI::FilterWheel::initProperties();
    
    for (int i = 0; i < SLOT_NUM; ++i)
        IUFillSwitch(&m_SlotS[i], std::to_string(SlotNum[i]).c_str(), std::to_string(SlotNum[i]).c_str(), ISS_OFF);
    IUFillSwitchVector(&m_SlotSP, m_SlotS, SLOT_NUM, getDeviceName(), "SLOTNUMBER", "Slot Number", FILTER_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    IUFillText(&m_VersionT[TC_FW_VERSION], "FIRMWARE", "Firmware", nullptr);
    IUFillText(&m_VersionT[TC_HW_VERSION], "HARDWARE", "Hardware", nullptr);
    IUFillText(&m_VersionT[TC_REV], "REVISION", "Revision", nullptr);
    IUFillText(&m_VersionT[TC_SDK], "SDK", "SDK", FP(Version()));
    IUFillTextVector(&m_VersionTP, m_VersionT, 4, getDeviceName(), "VERSION", "Version", INFO_TAB, IP_RO, 0, IPS_IDLE);
    
    return true;
}

bool ToupWheel::updateProperties()
{
    if (isConnected())
    {
        char tmpBuffer[64] = {0};
        uint16_t pRevision = 0;
        FP(get_FwVersion(m_Handle, tmpBuffer));
        IUSaveText(&m_VersionT[TC_FW_VERSION], tmpBuffer);
        FP(get_HwVersion(m_Handle, tmpBuffer));
        IUSaveText(&m_VersionT[TC_HW_VERSION], tmpBuffer);
        FP(get_Revision(m_Handle, &pRevision));
        snprintf(tmpBuffer, 32, "%d", pRevision);
        IUSaveText(&m_VersionT[TC_REV], tmpBuffer);
    
        int val = 0;
        FP(get_Option(m_Handle, CP(OPTION_FILTERWHEEL_SLOT), &val));
        for (int i = 0; i < SLOT_NUM; ++i)
            m_SlotS[i].s = (SlotNum[i] == val) ? ISS_ON : ISS_OFF;
        
        TargetFilter = 1;
        FilterSlotN[0].max = val;
        updateFilter();
    }

    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        defineProperty(&m_SlotSP);
        defineProperty(&m_VersionTP);
    }
    else
    {
        deleteProperty(m_SlotSP.name);
        deleteProperty(m_VersionTP.name);
    }

    return true;
}

void ToupWheel::updateFilter()
{
    CurrentFilter = QueryFilter();
    if (TargetFilter == CurrentFilter)
        SelectFilterDone(CurrentFilter);
    else if (FilterSlotNP.s != IPS_BUSY)
        FilterSlotNP.s = IPS_BUSY;
}

bool ToupWheel::Connect()
{
    m_Handle = FP(Open(m_Instance->id));

    if (m_Handle == nullptr)
    {
        LOG_ERROR("Failed to connect filterwheel");
        return false;
    }
    
    LOGF_INFO("%s connect", getDeviceName());
    return true;
}

bool ToupWheel::Disconnect()
{
    FP(Close(m_Handle));
    return true;
}

bool ToupWheel::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, m_SlotSP.name))
        {
            IUUpdateSwitch(&m_SlotSP, states, names, n);
            int val = SlotNum[IUFindOnSwitchIndex(&m_SlotSP)];
            HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_FILTERWHEEL_SLOT), val));
            if (SUCCEEDED(rc))
            {               
                LOGF_INFO("Set slot number: %d", val);
                
                m_SlotSP.s = IPS_OK;
                FilterSlotN[0].max = val;
                IUUpdateMinMax(&FilterSlotNP);
                
                TargetFilter = 1;
                updateFilter();
            }
            else
            {
                LOGF_ERROR("Failed to set slot number. %s", errorCodes(rc).c_str());
                m_SlotSP.s = IPS_ALERT;
            }
            IDSetSwitch(&m_SlotSP, nullptr);
            return true;
        }
    }
    
    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

void ToupWheel::TimerHit()
{
    if (FilterSlotNP.s == IPS_BUSY)
    {
        CurrentFilter = QueryFilter();
        if (TargetFilter == CurrentFilter)
            SelectFilterDone(CurrentFilter);
    }

    SetTimer(getCurrentPollingPeriod());
}

bool ToupWheel::SelectFilter(int targetFilter)
{
    HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_FILTERWHEEL_POSITION), targetFilter - 1));
    if (FAILED(rc))
    {
        LOGF_ERROR("Failed to select filter wheel %d. %s", targetFilter, errorCodes(rc).c_str());
        return false;
    }
    return true;
}

int ToupWheel::QueryFilter()
{
    int val = -1;
    HRESULT rc = FP(get_Option(m_Handle, CP(OPTION_FILTERWHEEL_POSITION), &val));
    if (FAILED(rc))
    {
        LOGF_ERROR("Failed to query filter wheel. %s", errorCodes(rc).c_str());
        return -1;
    }
    else if (val < 0)
        return val;
    else
        return (val + 1);
}

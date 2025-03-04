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

#define OPTION_EEPROMCFG                0x00001002      /* eeprom cfg support? */

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
void ToupWheel::ISGetProperties(const char *dev)
{
    INDI::FilterWheel::ISGetProperties(dev);
    defineProperty(SlotsSP);
    defineProperty(SpinningDirectionSP);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupWheel::initProperties()
{
    INDI::FilterWheel::initProperties();

    // Add Debug Control.
    addDebugControl();

    VersionTP[TC_FW_VERSION].fill("FIRMWARE", "Firmware", nullptr);
    VersionTP[TC_HW_VERSION].fill("HARDWARE", "Hardware", nullptr);
    VersionTP[TC_REV].fill("REVISION", "Revision", nullptr);
    VersionTP[TC_SDK].fill("SDK", "SDK", FP(Version()));
    VersionTP.fill(getDeviceName(), "VERSION", "Version", INFO_TAB, IP_RO, 0, IPS_IDLE);

    SlotsSP[SLOTS_5].fill("SLOTS_5", "5", ISS_ON);
    SlotsSP[SLOTS_7].fill("SLOTS_7", "7", ISS_OFF);
    SlotsSP[SLOTS_8].fill("SLOTS_8", "8", ISS_OFF);
    SlotsSP.fill(getDeviceName(), "SLOTS", "Slots", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
    SlotsSP.load();

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

        SpinningDirectionSP[TCFW_SD_CLOCKWISE].fill("CLOCKWISE", "Clockwise", ISS_ON);
        SpinningDirectionSP[TCFW_SD_AUTO].fill("AUTO", "Auto Direction", ISS_OFF);
        SpinningDirectionSP.fill(getDeviceName(), "SPINNINGDIRECTION", "Spinning Direction", FILTER_TAB, IP_RW, ISR_1OFMANY, 0,
                                 IPS_IDLE);
        SpinningDirectionSP.load();

        defineProperty(SpinningDirectionSP);
    }
    else
    {
        deleteProperty(VersionTP);
        deleteProperty(SpinningDirectionSP);
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

    int slot = 0;
    /* Is there builtin EEPROM to save config? */
    if (SUCCEEDED(FP(get_Option(m_Handle, OPTION_EEPROMCFG, nullptr))))
        FP(get_Option(m_Handle, CP(OPTION_FILTERWHEEL_SLOT), &slot));
    if ((5 == slot) || (7 == slot) || (8 == slot))
    {
        LOGF_INFO("%s: get slot number from builtin EEPROM, %d", getDeviceName(), slot);
        const char* names[] = { "SLOTS_5", "SLOTS_7", "SLOTS_8" };
        ISState states[3];
        states[0] = (5 == slot) ? ISS_ON : ISS_OFF;
        states[1] = (7 == slot) ? ISS_ON : ISS_OFF;
        states[2] = (8 == slot) ? ISS_ON : ISS_OFF;
        SlotsSP.update(states, names, 3);
    }
    else
    {
        auto currentSlot = SlotsSP.findOnSwitchIndex();
        if (currentSlot == SLOTS_7)
            slot = 7;
        else if (currentSlot == SLOTS_8)
            slot = 8;
        else
            slot = 5;
        LOGF_INFO("%s: get slot number from config file, %d", getDeviceName(), slot);
    }
    FilterSlotNP[0].setMax(slot);
    FilterSlotNP.updateMinMax();

    FP(put_Option(m_Handle, CP(OPTION_FILTERWHEEL_SLOT), slot));
    TargetFilter = 1; // if desconnected during spinning, TargetFilter must be initialize when reconnect.
    SelectFilter(0);

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
    if (!isConnected())
        return;

    QueryFilter();

    LOGF_DEBUG("TimerHit: CurrentFilter=%d, TargetFilter=%d", CurrentFilter, TargetFilter);

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
bool ToupWheel::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (SlotsSP.isNameMatch(name))
        {
            auto previousSlot = SlotsSP.findOnSwitchIndex();
            SlotsSP.update(states, names, n);
            SlotsSP.setState(IPS_OK);
            SlotsSP.apply();
            auto currentSlot = SlotsSP.findOnSwitchIndex();
            if (previousSlot != currentSlot && isConnected())
            {
                FP(put_Option(m_Handle, CP(OPTION_FILTERWHEEL_SLOT), currentSlot));
                FilterSlotNP[0].setMax(currentSlot);
                FilterSlotNP.updateMinMax();
            }
            saveConfig(SlotsSP);
            return true;
        }
        else if (SpinningDirectionSP.isNameMatch(name))
        {
            SpinningDirectionSP.update(states, names, n);
            SpinningDirectionSP.setState(IPS_OK);
            SpinningDirectionSP.apply();

            auto currentSpinningDirection = SpinningDirectionSP.findOnSwitchIndex();
            if (currentSpinningDirection == TCFW_SD_AUTO)
                SpinningDirection = 0x100; // auto direction spinning
            else
                SpinningDirection = 0; // clockwise spinning

            saveConfig(SpinningDirectionSP);
            return true;
        }
    }

    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupWheel::SelectFilter(int targetFilter)
{
    HRESULT rc = FP(put_Option(m_Handle, CP(OPTION_FILTERWHEEL_POSITION), SpinningDirection | (targetFilter - 1)));
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
    auto val = -1;
    HRESULT rc = FP(get_Option(m_Handle, CP(OPTION_FILTERWHEEL_POSITION), &val));
    if (FAILED(rc))
    {
        LOGF_ERROR("Failed to query filter wheel. %s", errorCodes(rc).c_str());
        return -1;
    }

    // Touptek uses 0 to N-1 positions.
    if (val >= 0)
        CurrentFilter = val + 1;

    return CurrentFilter;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////////////////
bool ToupWheel::saveConfigItems(FILE * fp)
{
    INDI::FilterWheel::saveConfigItems(fp);

    SlotsSP.save(fp);
    SpinningDirectionSP.save(fp);
    return true;
}

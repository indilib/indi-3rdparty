/*
    Astroasis Oasis Filter Wheel
    Copyright (C) 2013-2019 Jasem Mutlaq (mutlaqja@ikarustech.com)
    Copyright (C) 2023 Frank Chen (frank.chen@astroasis.com)

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
    USA

*/

#include "oasis_filter_wheel.h"
#include "config.h"
#include "indicom.h"
#include <regex>

static class Loader
{
        std::deque<std::unique_ptr<OasisFilterWheel>> filterWheels;
    public:
        Loader()
        {
            filterWheels.push_back(std::unique_ptr<OasisFilterWheel>(new OasisFilterWheel()));
        }
} loader;

OasisFilterWheel::OasisFilterWheel()
{
    setVersion(ASTROASIS_VERSION_MAJOR, ASTROASIS_VERSION_MINOR);
}

bool OasisFilterWheel::initProperties()
{
    INDI::FilterWheel::initProperties();

    // Speed
    IUFillSwitch(&SpeedS[0], "SPEED_FAST", "Fast", ISS_OFF);
    IUFillSwitch(&SpeedS[1], "SPEED_NORMAL", "Normal", ISS_OFF);
    IUFillSwitch(&SpeedS[2], "SPEED_SLOW", "Slow", ISS_OFF);
    IUFillSwitchVector(&SpeedSP, SpeedS, 3, getDeviceName(), "SPEED", "Speed",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Auto run on power up
    IUFillSwitch(&AutoRunS[INDI_ENABLED], "INDI_ENABLED", "Enable", ISS_OFF);
    IUFillSwitch(&AutoRunS[INDI_DISABLED], "INDI_DISABLED", "Disable", ISS_ON);
    IUFillSwitchVector(&AutoRunSP, AutoRunS, 2, getDeviceName(), "FILTER_AUTO_RUN", "Auto run on power up",
                       MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

    // Factory Reset
    IUFillSwitch(&FactoryResetS[0], "FACTORY_RESET", "Reset", ISS_OFF);
    IUFillSwitchVector(&FactoryResetSP, FactoryResetS, 1, getDeviceName(), "FACTORY_RESET", "Factory Reset",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    // Calibrate
    IUFillSwitch(&CalibrateS[0], "CALIBRATE", "Calibrate", ISS_OFF);
    IUFillSwitchVector(&CalibrateSP, CalibrateS, 1, getDeviceName(), "FILTER_CALIBRATION", "Calibrate",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    addAuxControls();
    setDefaultPollingPeriod(250);
    return true;
}

bool OasisFilterWheel::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        OFWConfig config;

        GetFilterNames();

        if (GetConfig(&config))
        {
            AutoRunS[INDI_ENABLED].s = config.autorun ? ISS_ON : ISS_OFF;
            AutoRunS[INDI_DISABLED].s = config.autorun ? ISS_OFF : ISS_ON;

            SpeedS[0].s = (config.speed == 0) ? ISS_ON : ISS_OFF;
            SpeedS[1].s = (config.speed == 1) ? ISS_ON : ISS_OFF;
            SpeedS[2].s = (config.speed == 2) ? ISS_ON : ISS_OFF;
        }
        else
        {
            AutoRunSP.s = IPS_ALERT;
        }

        defineProperty(&SpeedSP);
        defineProperty(&AutoRunSP);
        defineProperty(&FactoryResetSP);
        defineProperty(&CalibrateSP);
    }
    else
    {
        deleteProperty(SpeedSP.name);
        deleteProperty(AutoRunSP.name);
        deleteProperty(FactoryResetSP.name);
        deleteProperty(CalibrateSP.name);
    }

    return true;
}

const char *OasisFilterWheel::getDefaultName()
{
    return "Oasis Filter Wheel";
}

bool OasisFilterWheel::Connect()
{
    int number, ids[OFW_MAX_NUM];
    AOReturn ret;

    OFWScan(&number, ids);

    if (number <= 0)
    {
        LOG_INFO("Oasis filter wheel not found\n");
        return false;
    }

    // For now we always use the first found Oasis Filter Wheel
    mID = ids[0];

    ret = OFWOpen(mID);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("Failed to open Oasis filter wheel, ret = %d\n", ret);
        return false;
    }

    ret = OFWGetSlotNum(mID, &number);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("Failed to get Oasis filter wheel slot number, ret = %d\n", ret);
        OFWClose(mID);
        return false;
    }

    FilterSlotNP[0].setMin(1);
    FilterSlotNP[0].setMax(number);

    LOGF_INFO("Oasis filter wheel connected, %d slots\n", number);

    return true;
}

bool OasisFilterWheel::Disconnect()
{
    OFWClose(mID);

    return true;
}

bool OasisFilterWheel::GetFilterNames()
{
    char filterName[MAXINDINAME];
    char filterLabel[MAXINDILABEL];
    int MaxFilter = FilterSlotNP[0].getMax();
    IPState state = IPS_IDLE;

    FilterNameTP.setState(IPS_BUSY);
    FilterNameTP.resize(0);

    for (int i = 0; i < MaxFilter; i++)
    {
        char name[MAXINDINAME];
        AOReturn ret = OFWGetSlotName(mID, i + 1, name);

        snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", i + 1);
        snprintf(filterLabel, MAXINDILABEL, "Filter#%d", i + 1);

        INDI::WidgetText oneText;
        oneText.fill(filterName, filterLabel, (ret == AO_SUCCESS) ? name : filterLabel);
        FilterNameTP.push(std::move(oneText));

        if (ret != AO_SUCCESS)
        {
            LOGF_ERROR("Failed to get Oasis filter wheel slot name, ret = %d\n", ret);
            state = IPS_ALERT;
        }
    }

    FilterNameTP.fill(getDeviceName(), "FILTER_NAME", "Filter",
                      FilterSlotNP.getGroupName(), IP_RW, 0, state);
    FilterNameTP.shrink_to_fit();

    return true;
}

bool OasisFilterWheel::SetFilterNames()
{
    // Verify allowed filter names
    std::regex rx("^[A-Za-z0-9=.#/_%[:space:]-]{1,32}$");

    for (int i = 0; i < FilterSlotNP[0].getMax(); i++)
    {
        if (!std::regex_match(FilterNameTP[i].getText(), rx))
        {
            LOGF_ERROR("Filter #%d: the filter name is not valid. It should not have more than 32 chars", i + 1);
            LOGF_ERROR("Filter #%d: and the valid chars are A to Z, a to z, 0 to 9 = . # / - _ percent or space", i + 1);

            return false;
        }
    }

    for (int i = 0; i < FilterSlotNP[0].getMax(); i++)
    {
        // Create a non-const copy of the string since OFWSetSlotName expects char*
        char name[MAXINDINAME];
        strncpy(name, FilterNameTP[i].getText(), MAXINDINAME - 1);
        name[MAXINDINAME - 1] = '\0'; // Ensure null termination
        AOReturn ret = OFWSetSlotName(mID, i + 1, name);

        if (ret != AO_SUCCESS)
        {
            LOGF_ERROR("Failed to set Oasis filter wheel slot name, ret = %d\n", ret);
            return false;
        }
    }

    return true;
}

bool OasisFilterWheel::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && !strcmp(dev, getDeviceName()))
    {
        if (!strcmp(name, SpeedSP.name))
        {
            OFWConfig config;
            AOReturn ret;
            int prev, target;

            prev = IUFindOnSwitchIndex(&SpeedSP);
            IUUpdateSwitch(&SpeedSP, states, names, n);
            target = IUFindOnSwitchIndex(&SpeedSP);

            config.mask = MASK_SPEED;
            config.speed = target;

            ret = OFWSetConfig(mID, &config);

            if (ret == AO_SUCCESS)
            {
                SpeedSP.s = IPS_OK;
            }
            else
            {
                LOGF_ERROR("Failed to set Oasis filter wheel speed, ret = %d\n", ret);

                IUResetSwitch(&SpeedSP);

                if ((prev >= 0) && (prev < 3))
                    SpeedS[prev].s = ISS_ON;

                SpeedSP.s = IPS_ALERT;
            }

            IDSetSwitch(&SpeedSP, nullptr);

            return true;
        }

        if (!strcmp(name, AutoRunSP.name))
        {
            OFWConfig config;
            AOReturn ret;

            config.mask = MASK_AUTORUN;
            config.autorun = (IUFindOnSwitchIndex(&AutoRunSP) == INDI_ENABLED) ? 0 : 1;

            ret = OFWSetConfig(mID, &config);

            if (ret == AO_SUCCESS)
            {
                IUUpdateSwitch(&AutoRunSP, states, names, n);
                AutoRunSP.s = IPS_OK;
            }
            else
            {
                LOGF_ERROR("Failed to set Oasis filter wheel auto run, ret = %d\n", ret);
                AutoRunSP.s = IPS_ALERT;
            }

            IDSetSwitch(&AutoRunSP, nullptr);

            return true;
        }

        if (!strcmp(name, CalibrateSP.name))
        {
            AOReturn ret;

            CalibrateS[0].s = ISS_OFF;

            ret = OFWCalibrate(mID, 0);

            if (ret == AO_SUCCESS)
            {
                LOG_INFO("Oasis filter wheel calibrating...\n");
                CalibrateSP.s = IPS_BUSY;
                SetTimer(getCurrentPollingPeriod());
            }
            else
            {
                LOGF_ERROR("Failed to start Oasis filter wheel calibration, ret = %d\n", ret);
                CalibrateSP.s = IPS_ALERT;
            }

            IDSetSwitch(&CalibrateSP, nullptr);

            return true;
        }

        if (!strcmp(name, FactoryResetSP.name))
        {
            AOReturn ret;

            FactoryResetS[0].s = ISS_OFF;

            ret = OFWFactoryReset(mID);

            if (ret == AO_SUCCESS)
            {
                FactoryResetSP.s = IPS_OK;
            }
            else
            {
                LOGF_ERROR("Failed to factory reset Oasis filter wheel, ret = %d\n", ret);
                FactoryResetSP.s = IPS_ALERT;
            }

            IDSetSwitch(&FactoryResetSP, nullptr);

            return true;
        }
    }

    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

int OasisFilterWheel::QueryFilter()
{
    OFWStatus status;
    AOReturn ret;

    ret = OFWGetStatus(mID, &status);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("Failed to get Oasis filter wheel status, ret = %d\n", ret);
        return 0;
    }

    CurrentFilter = (status.filterStatus == STATUS_IDLE) ? status.filterPosition : 0;

    if (status.filterStatus == STATUS_IDLE)
    {
        LOGF_DEBUG("CurrentFilter: %d\n", CurrentFilter);
    }
    else
    {
        LOG_INFO("Oasis filter wheel moving...\n");
    }

    return CurrentFilter;
}

bool OasisFilterWheel::SelectFilter(int position)
{
    AOReturn ret;

    ret = OFWSetPosition(mID, position);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("Failed to set Oasis filter wheel position to %d\n", position);
        return false;
    }

    SetTimer(getCurrentPollingPeriod());

    return true;
}

void OasisFilterWheel::TimerHit()
{
    QueryFilter();

    if (CurrentFilter != TargetFilter)
    {
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        SelectFilterDone(CurrentFilter);

        CalibrateSP.s = IPS_OK;
        IDSetSwitch(&CalibrateSP, nullptr);
    }
}

bool OasisFilterWheel::saveConfigItems(FILE *fp)
{
    return INDI::FilterWheel::saveConfigItems(fp);
}

bool OasisFilterWheel::GetConfig(OFWConfig *config)
{
    AOReturn ret = OFWGetConfig(mID, config);

    if (ret != AO_SUCCESS)
    {
        LOGF_ERROR("Failed to get Oasis filter wheel configuration, ret = %d\n", ret);
        return false;
    }

    return true;
}

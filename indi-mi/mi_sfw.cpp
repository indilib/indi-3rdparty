/*
 Moravian Instruments INDI Driver

 Copyright (C) 2024 Moravian Instruments (info@gxccd.com)

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

#include "mi_sfw.h"

#include "config.h"

#include <math.h>
#include <deque>
#include <memory>
#include <utility>

#define MAX_DEVICES     4    /* Max device cameraCount */
#define MAX_ERROR_LEN   64   /* Max length of error buffer */

// There is _one_ binary for USB and ETH driver, but each binary is renamed
// to its variant (indi_mi_sfw_usb and indi_mi_sfw_eth). The main function will
// fetch from std args the binary name and ISInit will create the appropriate
// driver afterwards.
extern char *__progname;

static class Loader
{
        std::deque<std::unique_ptr<MISFW>> wheels;

    public:
        Loader();

    public:
        std::deque<std::pair<int /* id */, bool /* eth */>> initWheels;

} loader;

Loader::Loader()
{
    if (strstr(__progname, "indi_mi_sfw_eth"))
    {
        gxfw_enumerate_eth([](int id)
        {
            loader.initWheels.emplace_back(id, true);
        });
    }
    else
    {
        // "__progname" shoud be indi_mi_sfw_usb, however accept all names as USB
        gxfw_enumerate_usb([](int id)
        {
            loader.initWheels.emplace_back(id, false);
        });
    }

    for (const auto &args : initWheels)
    {
        wheels.push_back(std::unique_ptr<MISFW>(new MISFW(args.first, args.second)));
    }

    initWheels.clear();
}

MISFW::MISFW(int _wheelId, bool eth)
{
    if (isSimulation())
    {
        numFilters = 9;
        strncpy(name, "MI SFW Simulator", MAXINDINAME);
    }
    else
    {
        wheelId = _wheelId;
        isEth = eth;

        if (isEth)
            wheelHandle = gxfw_initialize_eth(wheelId);
        else
            wheelHandle = gxfw_initialize_usb(wheelId);
        if (!wheelHandle)
        {
            IDLog("Error connecting MI SFW!\n");
            return;
        }

        char sp[MAXINDINAME];
        if (gxfw_get_string_parameter(wheelHandle, FW_GSP_DESCRIPTION, sp, sizeof(sp)) < 0)
        {
            strncpy(name, "MI SFW", MAXINDIDEVICE);
            gxfw_get_last_error(wheelHandle, sp, sizeof(sp));
            IDLog("Error getting MI SFW info: %s.\n", sp);
        }
        else
        {
            strncpy(name, "MI ", MAXINDINAME);
            strncat(name, sp, MAXINDINAME - 3);
            IDLog("Detected SFW: %s.\n", name);
        }

        gxfw_get_integer_parameter(wheelHandle, FW_GIP_FILTERS, &numFilters);

        gxfw_release(wheelHandle);
        wheelHandle = nullptr;
    }

    setDeviceName(name);
    setVersion(INDI_MI_VERSION_MAJOR, INDI_MI_VERSION_MINOR);
}

MISFW::~MISFW()
{
    if (wheelHandle)
        gxfw_release(wheelHandle);
}

const char *MISFW::getDefaultName()
{
    return name;
}

bool MISFW::initProperties()
{
    INDI::FilterWheel::initProperties();

    FilterSlotNP[0].setMin(1);
    FilterSlotNP[0].setMax(numFilters);

    // Reinit FW
    IUFillSwitch(&ReinitS[0], "REINIT", "Reinit Filter Wheel", ISS_OFF);
    IUFillSwitchVector(&ReinitSP, ReinitS, 1, getDeviceName(), "SFW_REINIT", "Commands", MAIN_CONTROL_TAB, IP_WO, ISR_ATMOST1,
                       0, IPS_IDLE);

    IUFillText(&InfoT[0], "Model", "", "");
    IUFillText(&InfoT[1], "Firmware Rev.", "", "");
    IUFillText(&InfoT[2], "Serial No.", "", "");
    IUFillTextVector(&InfoTP, InfoT, 3, getDeviceName(), "Wheel Info", "Wheel Info", INFO_TAB, IP_RO, 60, IPS_IDLE);

    addAuxControls();

    return true;
}

bool MISFW::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        defineProperty(&ReinitSP);
        defineProperty(&InfoTP);
    }
    else
    {
        deleteProperty(ReinitSP.name);
        deleteProperty(InfoTP.name);
    }

    return true;
}

bool MISFW::Connect()
{
    if (isSimulation())
    {
        LOGF_INFO("Connected to %s", name);
        return true;
    }

    if (!wheelHandle)
    {
        if (isEth)
            wheelHandle = gxfw_initialize_eth(wheelId);
        else
            wheelHandle = gxfw_initialize_usb(wheelId);
    }
    if (!wheelHandle)
    {
        LOGF_ERROR("Error connecting to %s.", name);
        return false;
    }

    int fw_ver[4];
    char sp[MAXINDILABEL];

    gxfw_get_string_parameter(wheelHandle, FW_GSP_DESCRIPTION, sp, MAXINDILABEL);
    IUSaveText(&InfoT[0], sp);

    gxfw_get_integer_parameter(wheelHandle, FW_GIP_VERSION_1, &fw_ver[0]);
    gxfw_get_integer_parameter(wheelHandle, FW_GIP_VERSION_2, &fw_ver[1]);
    gxfw_get_integer_parameter(wheelHandle, FW_GIP_VERSION_3, &fw_ver[2]);
    gxfw_get_integer_parameter(wheelHandle, FW_GIP_VERSION_4, &fw_ver[3]);
    snprintf(sp, MAXINDILABEL, "%d.%d.%d.%d", fw_ver[0], fw_ver[1], fw_ver[2], fw_ver[3]);
    IUSaveText(&InfoT[1], sp);

    gxfw_get_string_parameter(wheelHandle, FW_GSP_SERIAL_NUMBER, sp, MAXINDILABEL);
    IUSaveText(&InfoT[2], sp);

    LOGF_INFO("Connected to %s.", name);
    return true;
}

bool MISFW::Disconnect()
{
    LOGF_INFO("Disconnected from %s.", name);
    if (!isSimulation())
    {
        gxfw_release(wheelHandle);
        wheelHandle = nullptr;
    }
    return true;
}

bool MISFW::SelectFilter(int position)
{
    if (!isSimulation() && gxfw_set_filter(wheelHandle, position - 1) < 0)
    {
        char errorStr[MAX_ERROR_LEN];
        gxfw_get_last_error(wheelHandle, errorStr, sizeof(errorStr));
        LOGF_ERROR("Setting filter failed: %s.", errorStr);
        return false;
    }

    CurrentFilter = position;
    SelectFilterDone(position);
    LOGF_DEBUG("Filter changed to %d", position);
    return true;
}

int MISFW::QueryFilter()
{
    return CurrentFilter;
}

bool MISFW::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        if (!strcmp(name, ReinitSP.name))
        {
            IUUpdateSwitch(&ReinitSP, states, names, n);
            IUResetSwitch(&ReinitSP);

            if (!isSimulation())
            {
                LOG_INFO("Reinitializing filter wheel...");
                if (gxfw_reinit_filter_wheel(wheelHandle, &numFilters) < 0)
                {
                    char errorStr[MAX_ERROR_LEN];
                    gxfw_get_last_error(wheelHandle, errorStr, sizeof(errorStr));
                    LOGF_ERROR("Wheel reinit failed: %s.", errorStr);
                    ReinitSP.s = IPS_ALERT;
                }
                else
                {
                    LOG_INFO("Done.");
                    ReinitSP.s = IPS_OK;
                    FilterSlotNP[0].setValue(1);
                    FilterSlotNP.apply();
                    updateFilterProperties();
                }
            }

            IDSetSwitch(&ReinitSP, nullptr);
            return true;
        }
    }

    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

bool MISFW::updateFilterProperties()
{
    if (FilterNameTP.size() != static_cast<size_t>(numFilters))
    {
        FilterSlotNP[0].setMax(numFilters);

        m_defaultDevice->deleteProperty(FilterNameTP);
        FilterNameTP.resize(0);

        char filterName[MAXINDINAME];
        char filterLabel[MAXINDILABEL];

        for (int i = 0; i < numFilters; i++)
        {
            snprintf(filterName, MAXINDINAME, "FILTER_SLOT_NAME_%d", i + 1);
            snprintf(filterLabel, MAXINDILABEL, "Filter#%d", i + 1);

            INDI::WidgetText oneText;
            oneText.fill(filterName, filterLabel, filterLabel);
            FilterNameTP.push(std::move(oneText));
        }

        FilterNameTP.fill(m_defaultDevice->getDeviceName(), "FILTER_NAME", "Filter",
                          FilterSlotNP.getGroupName(), IP_RW, 0, IPS_IDLE);
        FilterNameTP.shrink_to_fit();
        m_defaultDevice->defineProperty(FilterNameTP);

        // Try to load config filter labels
        for (int i = 0; i < numFilters; i++)
        {
            char oneFilter[MAXINDINAME] = {0};
            if (IUGetConfigText(m_defaultDevice->getDeviceName(), FilterNameTP.getName(), FilterNameTP[i].getName(), oneFilter,
                                MAXINDINAME) == 0)
            {
                FilterNameTP[i].setText(oneFilter);
            }
        }

        return true;
    }

    return false;
}

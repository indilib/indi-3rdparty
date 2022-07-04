/*******************************************************************************
  Copyright(c) 2022 Rick Bassham. All rights reserved.
  INDI NUT Weather Driver

  Modified for NetworkUPSToolsMonitor API by Jarno Paananen

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/

#include "indi_nut.h"
#include "config.h"

#include <memory>
#include <cstring>

// We declare an auto pointer to NetworkUPSToolsMonitor.
std::unique_ptr<NetworkUPSToolsMonitor> nutMonitor(new NetworkUPSToolsMonitor());

NetworkUPSToolsMonitor::NetworkUPSToolsMonitor()
{
    setVersion(NUT_VERSION_MAJOR, NUT_VERSION_MINOR);

    setWeatherConnection(CONNECTION_NONE);

    nutClient = new nut::TcpClient();
}

NetworkUPSToolsMonitor::~NetworkUPSToolsMonitor()
{
    delete nutClient;
    nutClient = nullptr;
}

const char *NetworkUPSToolsMonitor::getDefaultName()
{
    return (const char *)"NetworkUPSToolsMonitor";
}

bool NetworkUPSToolsMonitor::Connect()
{
    nutClient->connect(nutMonitorUrl[NUT_HOST].getText(), atoi(nutMonitorUrl[NUT_PORT].getText()));
    nutClient->authenticate(nutMonitorUrl[NUT_USER].getText(), nutMonitorUrl[NUT_PASSWORD].getText());

    return true;
}

bool NetworkUPSToolsMonitor::Disconnect()
{
    nutClient->disconnect();

    return true;
}

bool NetworkUPSToolsMonitor::initProperties()
{
    INDI::Weather::initProperties();

    nutMonitorUrl[NUT_HOST].fill("NUT_HOST", "NUT Monitor Host", nullptr);
    nutMonitorUrl[NUT_PORT].fill("NUT_PORT", "NUT Monitor Port", "3493");
    nutMonitorUrl[NUT_USER].fill("NUT_USER", "NUT Monitor User", nullptr);
    nutMonitorUrl[NUT_PASSWORD].fill("NUT_PASSWORD", "NUT Monitor Password", nullptr);

    nutMonitorUrl.fill(getDeviceName(), "NUT_MON_URL", "NetworkUPSToolsMonitor", OPTIONS_TAB, IP_RW, 60, IPS_IDLE);

    addParameter("WEATHER_CHARGE_REMAINING", "Charge Remaining", 50, 100, 0);

    setCriticalParameter("WEATHER_CHARGE_REMAINING");

    addDebugControl();

    return true;
}

void NetworkUPSToolsMonitor::ISGetProperties(const char *dev)
{
    INDI::Weather::ISGetProperties(dev);

    defineProperty(&nutMonitorUrl);
    loadConfig(true, nutMonitorUrl.getName());
}

bool NetworkUPSToolsMonitor::updateProperties()
{
    INDI::Weather::updateProperties();

    if (isConnected())
    {
        defineProperty(&nutMonitorUrl);
        SetTimer(getCurrentPollingPeriod());
    }
    else
    {
        deleteProperty(nutMonitorUrl.getName());
    }

    return true;
}

bool NetworkUPSToolsMonitor::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (nutMonitorUrl.isNameMatch(name))
        {
            nutMonitorUrl.update(texts, names, n);
            nutMonitorUrl.setState(IPS_OK);
            nutMonitorUrl.apply();
            return true;
        }
    }

    return INDI::Weather::ISNewText(dev, name, texts, names, n);
}

bool NetworkUPSToolsMonitor::updateLocation(double latitude, double longitude, double elevation)
{
    INDI_UNUSED(latitude);
    INDI_UNUSED(longitude);
    INDI_UNUSED(elevation);

    return true;
}

IPState NetworkUPSToolsMonitor::updateWeather()
{
    double charge = 0;

    auto devices = nutClient->getDeviceNames();

    for (auto &device : devices)
    {
        auto variables = nutClient->getDeviceVariableValue(device, "battery.charge");

        if (variables.size() == 0)
            continue;

        charge = atof(variables[0].c_str());
    }

    setParameterValue("WEATHER_CHARGE_REMAINING", charge);

    return IPS_OK;
}

bool NetworkUPSToolsMonitor::saveConfigItems(FILE *fp)
{
    INDI::Weather::saveConfigItems(fp);

    IUSaveConfigText(fp, &nutMonitorUrl);

    return true;
}

/*******************************************************************************
  Copyright(c) 2017 Jasem Mutlaq. All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/

#include "connectionhttp.h"

#include "indilogger.h"
#include "indistandardproperty.h"

#include <cstring>
#include <unistd.h>

namespace Connection
{
extern const char *CONNECTION_TAB;

HTTP::HTTP(INDI::DefaultDevice *dev) : Interface(dev, CONNECTION_CUSTOM)
{
    char defaultHostname[MAXINDINAME] = {0};

    IUGetConfigText(dev->getDeviceName(), AddressTP.name, "BASE_URL", defaultHostname, MAXINDINAME);

    // Address/Port
    IUFillText(&AddressT[0], "BASE_URL", "Address", defaultHostname);
    IUFillTextVector(&AddressTP, AddressT, 1, getDeviceName(), "DEVICE_BASE_URL", "Base URL", CONNECTION_TAB,
                     IP_RW, 60, IPS_IDLE);
}

bool HTTP::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp(dev, m_Device->getDeviceName()))
    {
        // HTTP Server settings
        if (!strcmp(name, AddressTP.name))
        {
            IUUpdateText(&AddressTP, texts, names, n);
            AddressTP.s = IPS_OK;
            IDSetText(&AddressTP, nullptr);
            return true;
        }
    }

    return false;
}

bool HTTP::Connect()
{
    if (AddressT[0].text == nullptr || AddressT[0].text[0] == '\0')
    {
        LOG_ERROR("Error! Server address is missing or invalid.");
        return false;
    }

    const char *baseurl = AddressT[0].text;

    LOGF_INFO("Connecting to %s ...", baseurl);

#if 0
    if (m_Device->isSimulation() == false)
    {
        client = new httplib::Client(baseurl);
    }
#else
        client = new httplib::Client(baseurl);
#endif

    bool rc = Handshake();

    if (rc)
    {
        LOGF_INFO("%s is online.", getDeviceName());
#if 0
        m_Device->saveConfig(true, "DEVICE_BASE_URL");
#endif
    }
    else
        LOG_DEBUG("Handshake failed.");

    return rc;
}

bool HTTP::Disconnect()
{
    delete client;
    client = nullptr;
    return true;
}

void HTTP::Activated()
{
    m_Device->defineProperty(&AddressTP);
#if 0
    m_Device->loadConfig(true, "DEVICE_BASE_URL");
#endif
}

void HTTP::Deactivated()
{
    m_Device->deleteProperty(AddressTP.name);
}

bool HTTP::saveConfigItems(FILE *fp)
{
    IUSaveConfigText(fp, &AddressTP);

    return true;
}

void HTTP::setDefaultHost(const char *addressHost)
{
    IUSaveText(&AddressT[0], addressHost);
}
}


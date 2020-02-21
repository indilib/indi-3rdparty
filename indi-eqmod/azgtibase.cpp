/*******************************************************************************
  Copyright(c) 2020 Jasem Mutlaq. All rights reserved.

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

#include "azgtibase.h"
#include <connectionplugins/connectiontcp.h>

AZGTIBase::AZGTIBase() : EQMod()
{
    setTelescopeConnection(CONNECTION_TCP);
}

const char * AZGTIBase::getDefaultName()
{
    return "AZ-GTi";
}

bool AZGTIBase::initProperties()
{
    EQMod::initProperties();

    tcpConnection->setDefaultHost("192.168.4.1");
    tcpConnection->setDefaultPort(11880);
    tcpConnection->setConnectionType(Connection::TCP::TYPE_UDP);

    for (auto oneProperty : *getProperties())
    {
        switch(oneProperty->getType())
        {
            case INDI_NUMBER:
                strncpy(static_cast<INumberVectorProperty*>(oneProperty->getProperty())->device, getDeviceName(), MAXINDIDEVICE);
                break;
            case INDI_TEXT:
                strncpy(static_cast<ITextVectorProperty*>(oneProperty->getProperty())->device, getDeviceName(), MAXINDIDEVICE);
                break;

            case INDI_SWITCH:
                strncpy(static_cast<ISwitchVectorProperty*>(oneProperty->getProperty())->device, getDeviceName(), MAXINDIDEVICE);
                break;

            case INDI_LIGHT:
                strncpy(static_cast<ILightVectorProperty*>(oneProperty->getProperty())->device, getDeviceName(), MAXINDIDEVICE);
                break;

            case INDI_BLOB:
                strncpy(static_cast<IBLOBVectorProperty*>(oneProperty->getProperty())->device, getDeviceName(), MAXINDIDEVICE);
                break;

            case INDI_UNKNOWN:
                break;
        }
    }

    return true;
}

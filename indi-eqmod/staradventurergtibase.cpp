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

#include "staradventurergtibase.h"
#include <connectionplugins/connectionserial.h>

StarAdventurerGTIBase::StarAdventurerGTIBase() : EQMod()
{
}

const char * StarAdventurerGTIBase::getDefaultName()
{
    return "Star Adventurer GTi";
}

bool StarAdventurerGTIBase::initProperties()
{
    EQMod::initProperties();
    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);
    for (auto oneProperty : *getProperties())
    {
        oneProperty.setDeviceName(getDeviceName());
    }

    return true;
}

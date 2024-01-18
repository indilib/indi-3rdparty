/*******************************************************************************
TicFocuser
Copyright (C) 2019 Sebastian Baberowski

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*******************************************************************************/

#ifndef USBCONNECTIONBASE_H
#define USBCONNECTIONBASE_H

#include <connectionplugins/connectioninterface.h>
#include "TicConnectionInterface.h"

class TicDriverInterface;

class UsbConnectionBase:
    public Connection::Interface,
    public TicConnectionInterface
{
    public:

        // serialNFieldName - each derived class must use its own name for UI fields
        UsbConnectionBase(const char* serialNFieldName, INDI::DefaultDevice *dev);
        ~UsbConnectionBase();
        
        bool Disconnect();
        void Activated();
        void Deactivated();

        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);

        bool saveConfigItems(FILE *fp);

        TicDriverInterface& getTicDriverInterface()   { return *ticDriverInterface; }
        
    protected:

        IText TicSerialNumberT[1] {};
        ITextVectorProperty TicSerialNumberTP;

        std::string requiredSerialNumber;

        TicDriverInterface* ticDriverInterface;
};

#endif // USBCONNECTIONBASE_H

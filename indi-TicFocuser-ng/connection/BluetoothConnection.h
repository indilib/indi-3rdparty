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

#ifndef BLUETOOTHCONNECTION_H
#define BLUETOOTHCONNECTION_H

#include <connectionplugins/connectioninterface.h>
#include "TicConnectionInterface.h"

class StreamBT;
class TicSerial;

class BluetoothConnection:
    public Connection::Interface,
    public TicConnectionInterface
{
    public:

        BluetoothConnection(INDI::DefaultDevice *dev);
        ~BluetoothConnection();

        std::string name() { return "Tic Bluetooth Connection"; };
        std::string label() { return "Bluetooth"; };
     
        bool Connect();
        bool Disconnect();
        void Activated();
        void Deactivated();

        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
        bool saveConfigItems(FILE *fp);

        TicDriverInterface& getTicDriverInterface()   { return *ticDriverInterface; }
        
    private:

        bool callHandshake();

        IText BtMacAddressT[1] {};
        ITextVectorProperty BtMacAddressTP;

        std::string requiredBtMacAddress;

        TicDriverInterface* ticDriverInterface;

        StreamBT* streamBT;
        TicSerial* ticSerial;
};

#endif // BLUETOOTHCONNECTION_H

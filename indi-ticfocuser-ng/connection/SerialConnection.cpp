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

#include "SerialConnection.h"

#include "driver_interfaces/TiclibInterface.h"
#include "ticlib/TicBase.h"
#include "ticlib/StreamSerial.h"

#include <indilogger.h>
#include <string.h>

SerialConnection::SerialConnection(INDI::DefaultDevice *dev):
    Connection::Serial(dev)
{
    streamSerial = new StreamSerial(PortFD);
    ticSerial = new TicSerial(*streamSerial);
    ticDriverInterface = new  TiclibInterface(*ticSerial);

    registerHandshake([&]()
    {
        return callHandshake();
    });
}

SerialConnection::~SerialConnection()
{
//    delete ticDriverInterface;
//    delete ticSerial;
//    delete streamBT;
}

bool SerialConnection::callHandshake()
{
    uint32_t uptime = ticSerial->getUpTime();
    return !ticSerial->getLastError() && uptime > 0;
}

#if 0

bool SerialConnection::Connect()
{
    bool res = Connection::Serial::Connect();

    if (res)
    {
        streamSerial->connect(PortFD);
//

    }

    return res;
}


bool BluetoothConnection::Disconnect()
{
    streamBT->disconnect();

    LOG_INFO("Bluetooth disconnected.");
    return true;
}

void BluetoothConnection::Activated()
{
    m_Device->defineProperty(&BtMacAddressTP);
}

void BluetoothConnection::Deactivated()
{
    m_Device->deleteProperty(BtMacAddressTP.name);
}

bool BluetoothConnection::saveConfigItems(FILE *fp) {

    if (!Connection::Interface::saveConfigItems(fp))
        return false;

    if (!requiredBtMacAddress.empty()) {

        // make sure we are storing requiredBtMacAddress as BtMacAddressT may contain connected BT mac
        char* tmpText = BtMacAddressT[0].text;
        BtMacAddressT[0].text = const_cast<char*>(requiredBtMacAddress.c_str());

        IUSaveConfigText(fp,&BtMacAddressTP);

        BtMacAddressT[0].text = tmpText;
    }

    return true;
}

bool BluetoothConnection::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp(dev, m_Device->getDeviceName()))
    {
        if (!strcmp(name, BtMacAddressTP.name)) {

            if (requiredBtMacAddress == texts[0])
                return true;

            requiredBtMacAddress = texts[0];

            if (m_Device->isConnected()) {

                if (requiredBtMacAddress.empty()) {
                    BtMacAddressTP.s = IPS_IDLE;
                }
                else {
                    LOG_WARN("Serial number selected. You must reconnect TicFocuser.");
                    BtMacAddressTP.s = IPS_BUSY;
                }

            }
            else {
                IUUpdateText(&BtMacAddressTP, texts, names, n);
                BtMacAddressTP.s = requiredBtMacAddress.empty()? IPS_IDLE: IPS_OK;
            }

            IDSetText(&BtMacAddressTP, nullptr);

            return true;
        }
    }

    return Connection::Interface::ISNewText(dev,name,texts,names,n);
}

#endif

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

#include "UsbConnectionBase.h"
#include "driver_interfaces/TicDriverInterface.h"

#include <indilogger.h>
#include <string.h>

UsbConnectionBase::UsbConnectionBase(const char* serialNFieldName, INDI::DefaultDevice *dev):
    Interface(dev, CONNECTION_USB)
{
    const size_t MAX_SERIAL_NUMBER = 20; // serial number has 8 characters, 20 is super safe
    char serialNumber[MAX_SERIAL_NUMBER];

    if (IUGetConfigText(dev->getDeviceName(), "TIC_SERIAL_TP", "TIC_SERIAL_NUMBER", serialNumber, MAX_SERIAL_NUMBER)) {
        serialNumber[0] = '\0';
    }
    else {
    	requiredSerialNumber = serialNumber;
    }

    std::string serialNFieldNameTP = serialNFieldName;
    serialNFieldNameTP += "_TP";

    IUFillText(TicSerialNumberT, serialNFieldName, "Tic Serial Number", serialNumber);
    IUFillTextVector(&TicSerialNumberTP, TicSerialNumberT, 1, getDeviceName(), serialNFieldNameTP.c_str(), "Tic Serial Number", CONNECTION_TAB,
                     IP_RW, 60, IPS_IDLE);
};

UsbConnectionBase::~UsbConnectionBase()
{
};

bool UsbConnectionBase::Disconnect()
{
    IUSaveText(TicSerialNumberT, requiredSerialNumber.c_str());
    TicSerialNumberTP.s = requiredSerialNumber.empty()? IPS_IDLE: IPS_OK;
    IDSetText(&TicSerialNumberTP, nullptr);

    return true; //Connection::Interface::Disconnect();
};

void UsbConnectionBase::Activated()
{
    m_Device->defineProperty(&TicSerialNumberTP);

    //Connection::Interface::Activated();
};

void UsbConnectionBase::Deactivated()
{
    m_Device->deleteProperty(TicSerialNumberTP.name);

    //Connection::Interface::Deactivated();
};

bool UsbConnectionBase::saveConfigItems(FILE *fp) {

    if (!Connection::Interface::saveConfigItems(fp))
        return false;

    if (!requiredSerialNumber.empty()) {

        // make sure we are storing requiredSerialNumber as TicSerialNumberT may contain connected TIC serial
        char* tmpText = TicSerialNumberT[0].text;
        TicSerialNumberT[0].text = const_cast<char*>(requiredSerialNumber.c_str());

        IUSaveConfigText(fp,&TicSerialNumberTP);

        TicSerialNumberT[0].text = tmpText;
    }

    return true;
}

bool UsbConnectionBase::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp(dev, m_Device->getDeviceName()))
    {
        if (!strcmp(name, TicSerialNumberTP.name)) {

            if (requiredSerialNumber == texts[0])
                return true;

            requiredSerialNumber = texts[0];

            if (m_Device->isConnected()) {

                if (requiredSerialNumber.empty()) {
                    TicSerialNumberTP.s = IPS_IDLE;
                }
                else {
                    LOG_WARN("Serial number selected. You must reconnect TicFocuser.");
                    TicSerialNumberTP.s = IPS_BUSY;
                }

            }
            else {
                IUUpdateText(&TicSerialNumberTP, texts, names, n);
                TicSerialNumberTP.s = requiredSerialNumber.empty()? IPS_IDLE: IPS_OK;
            }

            IDSetText(&TicSerialNumberTP, nullptr);

            return true;
        }
    }

    return Connection::Interface::ISNewText(dev,name,texts,names,n);
}

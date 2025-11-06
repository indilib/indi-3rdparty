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

#include "LibUsbConnection.h"

#include "driver_interfaces/TiclibInterface.h"
#include "ticlib/TicUsb.h"

#include <string.h>
#include <indilogger.h>

LibUsbConnection::LibUsbConnection(INDI::DefaultDevice *dev):
    UsbConnectionBase("LIBUSB_SERIAL_NUMBER",dev)

{
    ticUsb = new TicUsb();
    ticDriverInterface = new TiclibInterface(*ticUsb);
}

LibUsbConnection::~LibUsbConnection() 
{
    delete ticDriverInterface;
    delete ticUsb;
}

bool LibUsbConnection::Connect() 
{
    Disconnect();

    ticUsb->connect(requiredSerialNumber.c_str());
    if (ticUsb->getLastError())
    {
        LOGF_ERROR("TicUsb error: %s",ticUsb->getLastErrorMsg());
        if (requiredSerialNumber.empty())
            LOG_ERROR("No TIC device found.");
        else
            LOGF_ERROR("No TIC device found with serial: %s. You can set serial to empty to connect to the first found Tic device.", requiredSerialNumber.c_str());

        return false;
    }

    LOGF_INFO("Connected to Tic with serial: %s",ticUsb->getSerial());

    TicSerialNumberTP.s = requiredSerialNumber.empty()? IPS_IDLE: IPS_OK;
    IUSaveText(TicSerialNumberT, ticUsb->getSerial());
    IDSetText(&TicSerialNumberTP, nullptr);

    return true;
}

bool LibUsbConnection::Disconnect() 
{
    ticUsb->disconnect();
    return true;
}

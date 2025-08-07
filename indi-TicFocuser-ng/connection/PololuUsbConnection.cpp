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

#include "PololuUsbConnection.h"

#include "driver_interfaces/PololuUsbInterface.h"

#include <string.h>
#include <indilogger.h>
#include <tic.h>

PololuUsbConnection::PololuUsbConnection(INDI::DefaultDevice *dev):
    UsbConnectionBase("PUSB_SERIAL_NUMBER",dev),
    handle(NULL)
{
    ticDriverInterface = new PololuUsbInterface(handle);
}

PololuUsbConnection::~PololuUsbConnection() 
{
    if (handle)
        tic_handle_close(handle);

    delete ticDriverInterface;
}

bool PololuUsbConnection::Connect() 
{ 
    Disconnect();

    std::string devSerialNumber;
    tic_device** deviceList;

    tic_error* error = tic_list_connected_devices(&deviceList,NULL);
    if (error)
    {
        LOGF_ERROR("Cannot list connected devices. Error: %s", tic_error_get_message(error));
        tic_error_free(error);
        return false;
    }

    for (tic_device** d = deviceList; *d; ++d) {

        devSerialNumber = tic_device_get_serial_number(*d);

        if (requiredSerialNumber.empty() || requiredSerialNumber == devSerialNumber)
        {   
            error = tic_handle_open(*d,&handle);
            if (error)
            {
                LOGF_WARN("Cannot open tic device. Error: %s", tic_error_get_message(error));
                tic_error_free(error);
                continue;
            }

            break;
        }
    }

    for (tic_device** d = deviceList; *d; ++d)
        tic_device_free(*d);
    tic_list_free(deviceList);

    if (handle == NULL)
    {
        if (requiredSerialNumber.empty())
            LOG_ERROR("No TIC device found.");
        else
            LOGF_ERROR("No TIC device found with serial: %s. You can set serial to empty to connect to the first found Tic device.", requiredSerialNumber.c_str());
        return false;
    }

    LOGF_INFO("Connected to Tic with serial: %s",devSerialNumber.c_str());

    TicSerialNumberTP.s = requiredSerialNumber.empty()? IPS_IDLE: IPS_OK;
    IUSaveText(TicSerialNumberT, devSerialNumber.c_str());
    IDSetText(&TicSerialNumberTP, nullptr);

    return true;
}

bool PololuUsbConnection::Disconnect()
{
    bool ret = UsbConnectionBase::Disconnect();
    if (!ret)
        return ret;

    if (handle)
        tic_handle_close(handle);
    handle = NULL;

    return true;
}


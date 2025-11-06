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

#ifndef POLOLUUSBCONNECTION_H
#define POLOLUUSBCONNECTION_H

#include <connectionplugins/connectioninterface.h>
#include "UsbConnectionBase.h"

class tic_handle;

class PololuUsbConnection: public UsbConnectionBase
{
	tic_handle* handle;

    public:

        PololuUsbConnection(INDI::DefaultDevice *dev);
        ~PololuUsbConnection();
        
        bool Connect();
        bool Disconnect();

        std::string name() { return "Pololu USB Connection"; };
        std::string label() { return "PololuUSB"; };

};

#endif // POLOLUUSBCONNECTION_H

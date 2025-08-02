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

#ifndef SERIALCONNECTION_H
#define SERIALCONNECTION_H

#include <connectionplugins/connectionserial.h>
#include "TicConnectionInterface.h"

class StreamSerial;
class TicSerial;

class SerialConnection:
    public Connection::Serial,
    public TicConnectionInterface
{
    public:

        SerialConnection(INDI::DefaultDevice *dev);
        ~SerialConnection();

        std::string name() { return "Tic Serial Connection"; };
     
//        bool Connect();
//        bool Disconnect();
//        void Activated();
//        void Deactivated();
//
//        bool ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n);
//        bool saveConfigItems(FILE *fp);

        TicDriverInterface& getTicDriverInterface()   { return *ticDriverInterface; }
        
    private:

        bool callHandshake();

        TicDriverInterface* ticDriverInterface;
        StreamSerial* streamSerial;
        TicSerial* ticSerial;
};

#endif // SERIALCONNECTION_H

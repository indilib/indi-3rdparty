/*
    Celestron Aux Command

    Copyright (C) 2020 Paweł T. Jochym
    Copyright (C) 2020 Fabrizio Pollastri
    Copyright (C) 2021 Jasem Mutlaq

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

#pragma once

#include <vector>
#include <stdint.h>

typedef std::vector<unsigned char> AUXBuffer;

enum AUXCommands
{
    MC_GET_POSITION      = 0x01,
    MC_GOTO_FAST         = 0x02,
    MC_SET_POSITION      = 0x04,
    MC_SET_POS_GUIDERATE = 0x06,
    MC_SET_NEG_GUIDERATE = 0x07,
    MC_LEVEL_START       = 0x0b,
    MC_SLEW_DONE         = 0x13,
    MC_GOTO_SLOW         = 0x17,
    MC_SEEK_INDEX        = 0x19,
    MC_MOVE_POS          = 0x24,
    MC_MOVE_NEG          = 0x25,
    MC_AUX_GUIDE         = 0x26,
    MC_AUX_GUIDE_ACTIVE  = 0x27,
    MC_ENABLE_CORDWRAP   = 0x38,
    MC_DISABLE_CORDWRAP  = 0x39,
    MC_SET_CORDWRAP_POS  = 0x3a,
    MC_POLL_CORDWRAP     = 0x3b,
    MC_GET_CORDWRAP_POS  = 0x3c,
    MC_SET_AUTOGUIDE_RATE= 0x46,
    MC_GET_AUTOGUIDE_RATE= 0x47,
    GET_VER              = 0xfe,
    GPS_GET_LAT          = 0x01,
    GPS_GET_LONG         = 0x02,
    GPS_GET_DATE         = 0x03,
    GPS_GET_YEAR         = 0x04,
    GPS_GET_TIME         = 0x33,
    GPS_TIME_VALID       = 0x36,
    GPS_LINKED           = 0x37
};

enum AUXTargets
{
    ANY   = 0x00,
    MB    = 0x01,
    HC    = 0x04,
    HCP   = 0x0d,
    AZM   = 0x10,
    ALT   = 0x11,
    APP   = 0x20,
    GPS   = 0xb0,
    WiFi  = 0xb5,
    BAT   = 0xb6,
    CHG   = 0xb7,
    LIGHT = 0xbf
};

#define CAUX_DEFAULT_IP   "1.2.3.4"
#define CAUX_DEFAULT_PORT 2000

void logBytes(unsigned char *buf, int n, const char *deviceName, uint32_t debugLevel);

class AUXCommand
{
    public:
        AUXCommand();
        AUXCommand(const AUXBuffer &buf);
        AUXCommand(AUXCommands c, AUXTargets s, AUXTargets d, const AUXBuffer &dat);
        AUXCommand(AUXCommands c, AUXTargets s, AUXTargets d);

        ///////////////////////////////////////////////////////////////////////////////
        /// Buffer Management
        ///////////////////////////////////////////////////////////////////////////////
        void fillBuf(AUXBuffer &buf);
        void parseBuf(AUXBuffer buf);
        void parseBuf(AUXBuffer buf, bool do_checksum);

        ///////////////////////////////////////////////////////////////////////////////
        /// Position
        ///////////////////////////////////////////////////////////////////////////////
        long getPosition();
        void setPosition(long p);
        void setPosition(double p);

        ///////////////////////////////////////////////////////////////////////////////
        /// Rates
        ///////////////////////////////////////////////////////////////////////////////
        void setRate(unsigned char r);

        ///////////////////////////////////////////////////////////////////////////////
        /// Check sum
        ///////////////////////////////////////////////////////////////////////////////
        unsigned char checksum(AUXBuffer buf);

        ///////////////////////////////////////////////////////////////////////////////
        /// Logging
        ///////////////////////////////////////////////////////////////////////////////
        //void logCommand();
        const char * cmd_name(AUXCommands c);
        int response_data_size();
        const char * node_name(AUXTargets n);
        void logResponse();
        void logCommand();
        //void logResponse(AUXBuffer buf);
        static void setDebugInfo(const char *deviceName, uint8_t debugLevel);

        // TODO these should be private
        AUXCommands cmd;
        AUXTargets src, dst;
        AUXBuffer data;

        static uint8_t DEBUG_LEVEL;
        static char DEVICE_NAME[64];

    private:
        int len {0};
        bool valid {false};



};

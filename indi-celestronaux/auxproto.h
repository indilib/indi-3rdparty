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
#include <stddef.h>
#include <stdint.h>

typedef std::vector<uint8_t> AUXBuffer;

enum AUXCommands
{
    MC_GET_POSITION      = 0x01,
    MC_GOTO_FAST         = 0x02,
    MC_SET_POSITION      = 0x04,
    MC_GET_MODEL         = 0x05,
    MC_SET_POS_GUIDERATE = 0x06,
    MC_SET_NEG_GUIDERATE = 0x07,
    MC_LEVEL_START       = 0x0b,
    MC_LEVEL_DONE        = 0x12,
    MC_SLEW_DONE         = 0x13,
    MC_GOTO_SLOW         = 0x17,
    MC_SEEK_DONE         = 0x18,
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
    MC_SET_AUTOGUIDE_RATE = 0x46,
    MC_GET_AUTOGUIDE_RATE = 0x47,
    GET_VER              = 0xfe,
    GPS_GET_LAT          = 0x01,
    GPS_GET_LONG         = 0x02,
    GPS_GET_DATE         = 0x03,
    GPS_GET_YEAR         = 0x04,
    GPS_GET_TIME         = 0x33,
    GPS_TIME_VALID       = 0x36,
    GPS_LINKED           = 0x37,
    FOC_GET_HS_POSITIONS = 0x2c
};

enum AUXTargets
{
    ANY   = 0x00,
    MB    = 0x01,
    HC    = 0x04,
    HCP   = 0x0d,
    AZM   = 0x10,
    ALT   = 0x11,
    FOCUS = 0x12,
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
        AUXCommand(AUXCommands command, AUXTargets source, AUXTargets destination, const AUXBuffer &data);
        AUXCommand(AUXCommands command, AUXTargets source, AUXTargets destination);

        ///////////////////////////////////////////////////////////////////////////////
        /// Buffer Management
        ///////////////////////////////////////////////////////////////////////////////
        void fillBuf(AUXBuffer &buf);
        void parseBuf(AUXBuffer buf);
        void parseBuf(AUXBuffer buf, bool do_checksum);

        ///////////////////////////////////////////////////////////////////////////////
        /// Getters
        ///////////////////////////////////////////////////////////////////////////////
        const AUXTargets &source() const
        {
            return m_Source;
        }
        const AUXTargets &destination() const
        {
            return m_Destination;
        }
        const AUXBuffer &data() const
        {
            return m_Data;
        }
        AUXCommands command() const
        {
            return m_Command;
        }
        size_t dataSize() const
        {
            return m_Data.size();
        }
        const char * commandName() const
        {
            return commandName(m_Command);
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Set and Get data
        ///////////////////////////////////////////////////////////////////////////////
        /**
         * @brief getData Parses data packet and convert it to a 32bit unsigned integer
         * @param bytes How many bytes to interpret the data.
         * @return
         */
        uint32_t getData();
        void setData(uint32_t value, uint8_t bytes = 3);

        ///////////////////////////////////////////////////////////////////////////////
        /// Check sum
        ///////////////////////////////////////////////////////////////////////////////
        uint8_t checksum(AUXBuffer buf);

        ///////////////////////////////////////////////////////////////////////////////
        /// Logging
        ///////////////////////////////////////////////////////////////////////////////
        const char * commandName(AUXCommands command) const;
        const char * moduleName(AUXTargets n);
        int responseDataSize();
        void logResponse();
        void logCommand();
        static void setDebugInfo(const char *deviceName, uint8_t debugLevel);

        static uint8_t DEBUG_LEVEL;
        static char DEVICE_NAME[64];

    private:
        uint8_t len {0};
        bool valid {false};

        AUXCommands m_Command;
        AUXTargets m_Source, m_Destination;
        AUXBuffer m_Data;



};

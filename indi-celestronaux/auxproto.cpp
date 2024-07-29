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

#include "auxproto.h"

#include <indilogger.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define READ_TIMEOUT 1 		// s
#define CTS_TIMEOUT 100		// ms
#define RTS_DELAY 50		// ms

#define BUFFER_SIZE 512
int MAX_CMD_LEN = 32;

uint8_t AUXCommand::DEBUG_LEVEL = 0;
char AUXCommand::DEVICE_NAME[64] = {0};
//////////////////////////////////////////////////
/////// Utility functions
//////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void logBytes(unsigned char *buf, int n, const char *deviceName, uint32_t debugLevel)
{
    char hex_buffer[BUFFER_SIZE] = {0};
    for (int i = 0; i < n; i++)
        sprintf(hex_buffer + 3 * i, "%02X ", buf[i]);

    if (n > 0)
        hex_buffer[3 * n - 1] = '\0';

    DEBUGFDEVICE(deviceName, debugLevel, "[%s]", hex_buffer);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void AUXCommand::logResponse()
{
    char hex_buffer[BUFFER_SIZE] = {0}, part1[BUFFER_SIZE] = {0}, part2[BUFFER_SIZE] = {0}, part3[BUFFER_SIZE] = {0};
    for (size_t i = 0; i < m_Data.size(); i++)
        sprintf(hex_buffer + 3 * i, "%02X ", m_Data[i]);

    if (m_Data.size() > 0)
        hex_buffer[3 * m_Data.size() - 1] = '\0';

    const char * c = commandName(m_Command);
    const char * s = moduleName(m_Source);
    const char * d = moduleName(m_Destination);

    if (c != nullptr)
        snprintf(part1, BUFFER_SIZE, "<%12s>", c);
    else
        snprintf(part1, BUFFER_SIZE, "<%02x>", m_Command);

    if (s != nullptr)
        snprintf(part2, BUFFER_SIZE, "%5s ->", s);
    else
        snprintf(part2, BUFFER_SIZE, "%02x ->", m_Source);

    if (s != nullptr)
        snprintf(part3, BUFFER_SIZE, "%5s", d);
    else
        snprintf(part3, BUFFER_SIZE, "%02x", m_Destination);

    if (m_Data.size() > 0)
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "RES %s%s%s [%s]", part1, part2, part3, hex_buffer);
    else
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "RES %s%s%s", part1, part2, part3);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void AUXCommand::logCommand()
{
    char hex_buffer[BUFFER_SIZE] = {0}, part1[BUFFER_SIZE] = {0}, part2[BUFFER_SIZE] = {0}, part3[BUFFER_SIZE] = {0};
    for (size_t i = 0; i < m_Data.size(); i++)
        sprintf(hex_buffer + 3 * i, "%02X ", m_Data[i]);

    if (m_Data.size() > 0)
        hex_buffer[3 * m_Data.size() - 1] = '\0';

    const char * c = commandName(m_Command);
    const char * s = moduleName(m_Source);
    const char * d = moduleName(m_Destination);

    if (c != nullptr)
        snprintf(part1, BUFFER_SIZE, "<%12s>", c);
    else
        snprintf(part1, BUFFER_SIZE, "<%02x>", m_Command);

    if (s != nullptr)
        snprintf(part2, BUFFER_SIZE, "%5s ->", s);
    else
        snprintf(part2, BUFFER_SIZE, "%02x ->", m_Source);

    if (s != nullptr)
        snprintf(part3, BUFFER_SIZE, "%5s", d);
    else
        snprintf(part3, BUFFER_SIZE, "%02x", m_Destination);

    if (m_Data.size() > 0)
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "CMD %s%s%s [%s]", part1, part2, part3, hex_buffer);
    else
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "CMD %s%s%s", part1, part2, part3);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void AUXCommand::setDebugInfo(const char *deviceName, uint8_t debugLevel)
{
    strncpy(DEVICE_NAME, deviceName, 64);
    DEBUG_LEVEL = debugLevel;
}
////////////////////////////////////////////////
//////  AUXCommand class
////////////////////////////////////////////////

AUXCommand::AUXCommand()
{
    m_Data.reserve(MAX_CMD_LEN);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
AUXCommand::AUXCommand(const AUXBuffer &buf)
{
    m_Data.reserve(MAX_CMD_LEN);
    parseBuf(buf);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
AUXCommand::AUXCommand(AUXCommands command, AUXTargets source, AUXTargets destination, const AUXBuffer &data)
{
    m_Command = command;
    m_Source = source;
    m_Destination = destination;
    m_Data.reserve(MAX_CMD_LEN);
    m_Data = data;
    len  = 3 + m_Data.size();
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
AUXCommand::AUXCommand(AUXCommands command, AUXTargets source, AUXTargets destination)
{
    m_Command = command;
    m_Source = source;
    m_Destination = destination;
    m_Data.reserve(MAX_CMD_LEN);
    len = 3 + m_Data.size();
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
const char * AUXCommand::commandName(AUXCommands command) const
{
    if (m_Source == GPS || m_Destination == GPS)
    {
        switch (command)
        {
            case GPS_GET_LAT:
                return "GPS_GET_LAT";
            case GPS_GET_LONG:
                return "GPS_GET_LONG";
            case GPS_GET_DATE:
                return "GPS_GET_DATE";
            case GPS_GET_YEAR:
                return "GPS_GET_YEAR";
            case GPS_GET_TIME:
                return "GPS_GET_TIME";
            case GPS_TIME_VALID:
                return "GPS_TIME_VALID";
            case GPS_LINKED:
                return "GPS_LINKED";
            case GET_VER:
                return "GET_VER";
            default :
                return nullptr;
        }
    }
    else
    {
        switch (command)
        {
            case MC_GET_POSITION:
                return "MC_GET_POSITION";
            case MC_GOTO_FAST:
                return "MC_GOTO_FAST";
            case MC_SET_POSITION:
                return "MC_SET_POSITION";
            case MC_SET_POS_GUIDERATE:
                return "MC_SET_POS_GUIDERATE";
            case MC_SET_NEG_GUIDERATE:
                return "MC_SET_NEG_GUIDERATE";
            case MC_LEVEL_START:
                return "MC_LEVEL_START";
            case MC_SLEW_DONE:
                return "MC_SLEW_DONE";
            case MC_GOTO_SLOW:
                return "MC_GOTO_SLOW";
            case MC_SEEK_DONE:
                return "MC_SEEK_DONE";
            case MC_SEEK_INDEX:
                return "MC_SEEK_INDEX";
            case MC_MOVE_POS:
                return "MC_MOVE_POS";
            case MC_MOVE_NEG:
                return "MC_MOVE_NEG";
            case MC_ENABLE_CORDWRAP:
                return "MC_ENABLE_CORDWRAP";
            case MC_DISABLE_CORDWRAP:
                return "MC_DISABLE_CORDWRAP";
            case MC_SET_CORDWRAP_POS:
                return "MC_SET_CORDWRAP_POS";
            case MC_POLL_CORDWRAP:
                return "MC_POLL_CORDWRAP";
            case MC_GET_CORDWRAP_POS:
                return "MC_GET_CORDWRAP_POS";
            case GET_VER:
                return "GET_VER";
            case MC_LEVEL_DONE:
                return "MC_LEVEL_DONE";
            case MC_AUX_GUIDE:
                return "MC_AUX_GUIDE";
            case MC_AUX_GUIDE_ACTIVE:
                return "MC_AUX_GUIDE_ACTIVE";
            case MC_SET_AUTOGUIDE_RATE:
                return "MC_SET_AUTOGUIDE_RATE";
            case MC_GET_AUTOGUIDE_RATE:
                return "MC_GET_AUTOGUIDE_RATE";
            case FOC_GET_HS_POSITIONS:
                return "FOC_GET_HS_POSITIONS";
            default :
                return nullptr;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
int AUXCommand::responseDataSize()
{
    if (m_Source == GPS || m_Destination == GPS)
    {
        switch (m_Command)
        {
            case GPS_GET_LAT:
            case GPS_GET_LONG:
            case GPS_GET_TIME:
                return 3;
            case GPS_GET_DATE:
            case GPS_GET_YEAR:
            case GET_VER:
                return 2;
            case GPS_TIME_VALID:
            case GPS_LINKED:
                return 1;
            default :
                return -1;
        }
    }
    else
    {
        switch (m_Command)
        {
            case FOC_GET_HS_POSITIONS:
                return 8;
            case MC_GET_POSITION:
            case MC_GET_CORDWRAP_POS:
                return 3;
            case GET_VER:
                return 4;
            case MC_GET_MODEL:
                return 2;
	    case MC_SLEW_DONE:
            case MC_SEEK_DONE:
            case MC_LEVEL_DONE:
            case MC_POLL_CORDWRAP:
            case MC_AUX_GUIDE_ACTIVE:
            case MC_GET_AUTOGUIDE_RATE:
                return 1;
            case MC_GOTO_FAST:
            case MC_SET_POSITION:
            case MC_SET_POS_GUIDERATE:
            case MC_SET_NEG_GUIDERATE:
            case MC_LEVEL_START:
            case MC_GOTO_SLOW:
            case MC_MOVE_POS:
            case MC_MOVE_NEG:
            case MC_ENABLE_CORDWRAP:
            case MC_DISABLE_CORDWRAP:
            case MC_SET_CORDWRAP_POS:
            case MC_SET_AUTOGUIDE_RATE:
            case MC_AUX_GUIDE:
                return 0;
            case MC_SEEK_INDEX:
                return -1;
            default :
                return -1;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
const char * AUXCommand::moduleName(AUXTargets n)
{
    switch (n)
    {
        case ANY :
            return "ANY";
        case MB :
            return "MB";
        case HC :
            return "HC";
        case HCP :
            return "HC+";
        case AZM :
            return "AZM";
        case ALT :
            return "ALT";
        case APP :
            return "APP";
        case GPS :
            return "GPS";
        case FOCUS :
            return "FOCUS";
        case WiFi:
            return "WiFi";
        case BAT :
            return "BAT";
        case CHG :
            return "CHG";
        case LIGHT :
            return "LIGHT";
        default :
            return nullptr;
    }
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void AUXCommand::fillBuf(AUXBuffer &buf)
{
    buf.resize(len + 3);

    buf[0] = 0x3b;
    buf[1] = len;
    buf[2] = m_Source;
    buf[3] = m_Destination;
    buf[4] = m_Command;
    for (uint32_t i = 0; i < m_Data.size(); i++)
    {
        buf[i + 5] = m_Data[i];
    }
    buf.back() = checksum(buf);
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void AUXCommand::parseBuf(AUXBuffer buf)
{
    len   = buf[1];
    m_Source   = (AUXTargets)buf[2];
    m_Destination   = (AUXTargets)buf[3];
    m_Command   = (AUXCommands)buf[4];
    m_Data  = AUXBuffer(buf.begin() + 5, buf.end() - 1);
    valid = (checksum(buf) == buf.back());
    if (valid == false)
    {
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "Checksum error: %02x vs. %02x", checksum(buf), buf.back());
    };
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
void AUXCommand::parseBuf(AUXBuffer buf, bool do_checksum)
{
    (void)do_checksum;

    len   = buf[1];
    m_Source   = (AUXTargets)buf[2];
    m_Destination   = (AUXTargets)buf[3];
    m_Command   = (AUXCommands)buf[4];
    if (buf.size() > 5)
        m_Data  = AUXBuffer(buf.begin() + 5, buf.end());
}

/////////////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////////////
unsigned char AUXCommand::checksum(AUXBuffer buf)
{
    int l  = buf[1];
    int cs = 0;
    for (int i = 1; i < l + 2; i++)
    {
        cs += buf[i];
    }
    return (unsigned char)(((~cs) + 1) & 0xFF);
}

/////////////////////////////////////////////////////////////////////////////////////
/// Return 8, 16, or 24bit value as dictacted by the data response size.
/////////////////////////////////////////////////////////////////////////////////////
uint32_t AUXCommand::getData()
{
    uint32_t value = 0;
    switch (m_Data.size())
    {
        case 3:
            value = (m_Data[0] << 16) | (m_Data[1] << 8) | m_Data[2];
            break;

        case 2:
            value = (m_Data[0] << 8) | m_Data[1];
            break;

        case 1:
            value = m_Data[0];
            break;
    }

    return value;
}

/////////////////////////////////////////////////////////////////////////////////////
/// Set encoder position in steps.
/////////////////////////////////////////////////////////////////////////////////////
void AUXCommand::setData(uint32_t value, uint8_t bytes)
{
    m_Data.resize(bytes);
    switch (bytes)
    {
        case 1:
            len = 4;
            m_Data[0] = static_cast<uint8_t>(value >> 0 & 0xff);
            break;
        case 2:
            len = 5;
            m_Data[1] = static_cast<uint8_t>(value >>  0 & 0xff);
            m_Data[0] = static_cast<uint8_t>(value >>  8 & 0xff);
            break;

        case 3:
        default:
            len = 6;
            m_Data[2] = static_cast<uint8_t>(value >>  0 & 0xff);
            m_Data[1] = static_cast<uint8_t>(value >>  8 & 0xff);
            m_Data[0] = static_cast<uint8_t>(value >> 16 & 0xff);
            break;
    }
}

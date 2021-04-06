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


void logBytes(unsigned char *buf, int n, const char *deviceName, uint32_t debugLevel)
{
    char hex_buffer[BUFFER_SIZE] = {0};
    for (int i = 0; i < n; i++)
        sprintf(hex_buffer + 3 * i, "%02X ", buf[i]);

    if (n > 0)
        hex_buffer[3 * n - 1] = '\0';

    DEBUGFDEVICE(deviceName, debugLevel, "[%s]", hex_buffer);
}

//void AUXCommand::logResponse(AUXBuffer buf)
//{
//    char hex_buffer[BUFFER_SIZE] = {0};
//    for (size_t i = 0; i < buf.size(); i++)
//        sprintf(hex_buffer + 3 * i, "%02X ", buf[i]);

//    if (buf.size() > 0)
//        hex_buffer[3 * buf.size() - 1] = '\0';

//    DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "MSG: %s", hex_buffer);
//}

//void AUXCommand::logCommand()
//{
//    char hex_buffer[BUFFER_SIZE] = {0};
//    for (size_t i = 0; i < data.size(); i++)
//        sprintf(hex_buffer + 3 * i, "%02X ", data[i]);

//    if (data.size() > 0)
//        hex_buffer[3 * data.size() - 1] = '\0';

//    DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "<%02x> %02x -> %02x: %s", cmd, src, dst, hex_buffer);
//}

void AUXCommand::logResponse()
{
    char hex_buffer[BUFFER_SIZE] = {0}, part1[BUFFER_SIZE] = {0}, part2[BUFFER_SIZE] = {0}, part3[BUFFER_SIZE] = {0};
    for (size_t i = 0; i < data.size(); i++)
        sprintf(hex_buffer + 3 * i, "%02X ", data[i]);

    if (data.size() > 0)
        hex_buffer[3 * data.size() - 1] = '\0';

    const char * c = cmd_name(cmd);
    const char * s = node_name(src);
    const char * d = node_name(dst);

    if (c != nullptr)
        snprintf(part1, BUFFER_SIZE, "<%12s>", c);
    else
        snprintf(part1, BUFFER_SIZE, "<%02x>", cmd);

    if (s != nullptr)
        snprintf(part2, BUFFER_SIZE, "%5s ->", s);
    else
        snprintf(part2, BUFFER_SIZE, "%02x ->", src);

    if (s != nullptr)
        snprintf(part3, BUFFER_SIZE, "%5s", d);
    else
        snprintf(part3, BUFFER_SIZE, "%02x", dst);

    if (data.size() > 0)
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "RES %s%s%s [%s]", part1, part2, part3, hex_buffer);
    else
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "RES %s%s%s", part1, part2, part3);
}

void AUXCommand::logCommand()
{
    char hex_buffer[BUFFER_SIZE] = {0}, part1[BUFFER_SIZE] = {0}, part2[BUFFER_SIZE] = {0}, part3[BUFFER_SIZE] = {0};
    for (size_t i = 0; i < data.size(); i++)
        sprintf(hex_buffer + 3 * i, "%02X ", data[i]);

    if (data.size() > 0)
        hex_buffer[3 * data.size() - 1] = '\0';

    const char * c = cmd_name(cmd);
    const char * s = node_name(src);
    const char * d = node_name(dst);

    if (c != nullptr)
        snprintf(part1, BUFFER_SIZE, "<%12s>", c);
    else
        snprintf(part1, BUFFER_SIZE, "<%02x>", cmd);

    if (s != nullptr)
        snprintf(part2, BUFFER_SIZE, "%5s ->", s);
    else
        snprintf(part2, BUFFER_SIZE, "%02x ->", src);

    if (s != nullptr)
        snprintf(part3, BUFFER_SIZE, "%5s", d);
    else
        snprintf(part3, BUFFER_SIZE, "%02x", dst);

    if (data.size() > 0)
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "CMD %s%s%s [%s]", part1, part2, part3, hex_buffer);
    else
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "CMD %s%s%s", part1, part2, part3);
}

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
    data.reserve(MAX_CMD_LEN);
}

AUXCommand::AUXCommand(const AUXBuffer &buf)
{
    data.reserve(MAX_CMD_LEN);
    parseBuf(buf);
}

AUXCommand::AUXCommand(AUXCommands c, AUXTargets s, AUXTargets d, const AUXBuffer &dat)
{
    cmd = c;
    src = s;
    dst = d;
    data.reserve(MAX_CMD_LEN);
    data = dat;
    len  = 3 + data.size();
}

AUXCommand::AUXCommand(AUXCommands c, AUXTargets s, AUXTargets d)
{
    cmd = c;
    src = s;
    dst = d;
    data.reserve(MAX_CMD_LEN);
    len = 3 + data.size();
}

const char * AUXCommand::cmd_name(AUXCommands c)
{
    if (src == GPS || dst == GPS)
    {
        switch (c)
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
        switch (c)
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
            default :
                return nullptr;
        }
    }
}

int AUXCommand::response_data_size()
{
    if (src == GPS || dst == GPS)
        switch (cmd)
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
    else
        switch (cmd)
        {
            case MC_GET_POSITION:
            case MC_GET_CORDWRAP_POS:
                return 3;
            case GET_VER:
                return 4;
            case MC_SLEW_DONE:
            case MC_POLL_CORDWRAP:
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
                return 0;
            case MC_SEEK_INDEX:
                return -1;
            default :
                return -1;
        }
}


const char * AUXCommand::node_name(AUXTargets n)
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

void AUXCommand::fillBuf(AUXBuffer &buf)
{
    buf.resize(len + 3);
    buf[0] = 0x3b;
    buf[1] = (unsigned char)len;
    buf[2] = (unsigned char)src;
    buf[3] = (unsigned char)dst;
    buf[4] = (unsigned char)cmd;
    for (unsigned int i = 0; i < data.size(); i++)
    {
        buf[i + 5] = data[i];
    }
    buf.back() = checksum(buf);
}

void AUXCommand::parseBuf(AUXBuffer buf)
{
    len   = buf[1];
    src   = (AUXTargets)buf[2];
    dst   = (AUXTargets)buf[3];
    cmd   = (AUXCommands)buf[4];
    data  = AUXBuffer(buf.begin() + 5, buf.end() - 1);
    valid = (checksum(buf) == buf.back());
    if (valid == false)
    {
        DEBUGFDEVICE(DEVICE_NAME, DEBUG_LEVEL, "Checksum error: %02x vs. %02x", checksum(buf), buf.back());
        //logResponse(buf);
        //logCommand();
    };
}

void AUXCommand::parseBuf(AUXBuffer buf, bool do_checksum)
{
    (void)do_checksum;

    len   = buf[1];
    src   = (AUXTargets)buf[2];
    dst   = (AUXTargets)buf[3];
    cmd   = (AUXCommands)buf[4];
    if (buf.size() > 5)
        data  = AUXBuffer(buf.begin() + 5, buf.end());
}


unsigned char AUXCommand::checksum(AUXBuffer buf)
{
    int l  = buf[1];
    int cs = 0;
    for (int i = 1; i < l + 2; i++)
    {
        cs += buf[i];
    }
    //fprintf(stderr,"CS: %x  %x  %x\n", cs, ~cs, ~cs+1);
    return (unsigned char)(((~cs) + 1) & 0xFF);
    //return ((~sum([ord(c) for c in msg]) + 1) ) & 0xFF
}

// One definition rule (ODR) constants
// AUX commands use 24bit integer as a representation of angle in units of
// fractional revolutions. Thus 2^24 steps makes full revolution.
const long STEPS_PER_REVOLUTION = 16777216;
const double STEPS_PER_DEGREE   = STEPS_PER_REVOLUTION / 360.0;

long AUXCommand::getPosition()
{
    if (data.size() == 3)
    {
        unsigned int a = (unsigned int)data[0] << 16 | (unsigned int)data[1] << 8 | (unsigned int)data[2];
        //fprintf(stderr,"Angle: %d %08x = %d => %f\n", sizeof(a), a, a, a/pow(2,24));
        return (long)a % STEPS_PER_REVOLUTION;
    }
    else
    {
        return 0;
    }
}

void AUXCommand::setPosition(double p)
{
    setPosition(long(p * STEPS_PER_DEGREE));
}

void AUXCommand::setPosition(long p)
{
    //int a=(int)p; fprintf(stderr,"Angle: %08x = %d => %f\n", a, a, a/pow(2,24));
    data.resize(3);
    // Fold the value to 0-STEPS_PER_REVOLUTION range
    if (p < 0)
    {
        p += STEPS_PER_REVOLUTION;
    }
    p = p % STEPS_PER_REVOLUTION;
    for (int i = 2; i > -1; i--)
    {
        data[i] = (unsigned char)(p & 0xff);
        p >>= 8;
    }
    len = 6;
}

void AUXCommand::setRate(unsigned char r)
{
    data.resize(1);
    len     = 4;
    data[0] = r;
}

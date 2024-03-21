/*
    QHY QFocuser

    Copyright (C) 2024 Chen Jiaqi (cjq@qhyccd.com)

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

#include "config.h"
#include "qhy_focuser.h"

#include <memory>
#include <cstring>
#include <termios.h>
#include <iostream>
#include <thread>

#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif

using json = nlohmann::json;

#include <indicom.h>
#include <connectionplugins/connectionserial.h>

#define MAX_CMD 128
#define TIMEOUT 3

#define currentPosition         FocusAbsPosN[0].value
#define currentRelativeMovement FocusRelPosN[0].value
#define currentAbsoluteMovement FocusAbsPosN[0].value

// We declare an auto pointer to MyCustomDriver.
static std::unique_ptr<QFocuser> qFocus(new QFocuser());

QFocuser::QFocuser()
{
    setVersion(INDI_QHY_VERSION_MAJOR, INDI_QHY_VERSION_MINOR);

    // Here we tell the base Focuser class what types of connections we can support
    setSupportedConnections(CONNECTION_SERIAL);

    // And here we tell the base class about our focuser's capabilities.
    SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_REVERSE |
                  FOCUSER_CAN_SYNC);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
const char *QFocuser::getDefaultName()
{
    return "QFocuser";
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool QFocuser::initProperties()
{
    // initialize the parent's properties first
    INDI::Focuser::initProperties();

    TemperatureNP[0].fill("TEMPERATURE", "Celsius", "%0.0f", 0, 65000., 0., 10000.);
    TemperatureNP.fill(getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    TemperatureChipNP[0].fill("TEMPERATURE", "Celsius", "%0.0f", 0, 65000., 0., 10000.);
    TemperatureChipNP.fill(getDeviceName(), "CHIP_TEMPERATURE", "Chip Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    VoltageNP[0].fill("VOLTAGE", "Volt", "%0.0f", 0, 12., 0., 0.);
    VoltageNP.fill(getDeviceName(), "FOCUS_VOLTAGE", "Voltage", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    FOCUSVersionNP[0].fill("VERSION", "Version", "%0.0f", 0, 99999999., 0., 0.);
    FOCUSVersionNP.fill(getDeviceName(), "FOCUS_VERSION", "Focus", CONNECTION_TAB, IP_RO, 60, IPS_OK);

    BOARDVersionNP[0].fill("VERSION", "Version", "%0.0f", 0, 65000., 0., 0.);
    BOARDVersionNP.fill(getDeviceName(), "BOARD_VERSION", "Board", CONNECTION_TAB, IP_RO, 60, IPS_OK);

    FocusSpeedNP[0].fill("FOCUS_SPEED_VALUE", "Focus Speed", "%0.0f", 0.0, 8.0, 1.0, 0.0);
    FocusSpeedNP.fill(getDeviceName(), "FOCUS_SPEED", "Speed", MAIN_CONTROL_TAB, IP_RW, 60, IPS_OK);

    FocusAbsPosN[0].min   = -64000.;
    FocusAbsPosN[0].max   = 64000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = 1000;

    FocusMaxPosN[0].max = 2e6;

    FocusSpeedMin = 0;
    FocusSpeedMax = 8;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool QFocuser::updateProperties()
{
    INDI::Focuser::updateProperties();

    if (isConnected())
    {
        // TODO: Call define* for any custom properties only visible when connected.
        defineProperty(TemperatureNP);
        defineProperty(TemperatureChipNP);
        defineProperty(FocusSpeedNP);
        defineProperty(VoltageNP);
        defineProperty(FOCUSVersionNP);
        defineProperty(BOARDVersionNP);
    }
    else
    {
        // TODO: Call deleteProperty for any custom properties only visible when connected.
        deleteProperty(TemperatureNP);
        deleteProperty(TemperatureChipNP);
        deleteProperty(FocusSpeedNP);
        deleteProperty(VoltageNP);
        deleteProperty(FOCUSVersionNP);
        deleteProperty(BOARDVersionNP);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
std::string create_cmd(int cmd_idx, bool dir, int value)
{
    json response;
    response["cmd_id"] = cmd_idx;

    switch (cmd_idx)
    {
        case 1:     // Get version
        {
            break;
        }

        case 2:     // Relative Move
        {
            response["dir"]  = dir ? 1 : -1;
            response["step"] = value;
            break;
        }

        case 3:     // AbortFocuser
        {
            break;
        }

        case 4:     // Temperature
        {
            break;
        }

        case 5:     // Get Position
        {
            break;
        }

        case 6:     // Absolute Move
        {
            response["tar"] = value;
            break;
        }

        case 7:     // Set Reverse
        {
            response["rev"] = value;
            break;
        }

        case 11:    // Set Position
        {
            response["init_val"] = value;
            break;
        }

        case 13:    // Set Speed
        {
            response["speed"] = value;
            break;
        }

        case 16:    // Set Hold
        {
            response["ihold"] = 0;
            response["irun"] = 5;
            break;
        }

        default:
            break;
    }

    return response.dump();
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
int QFocuser::SendCommand(char *cmd_line)
{
    int nbytes_written = 0, err_code = 0;
    char qfocus_error[MAXRBUF];

    tcflush(PortFD, TCIOFLUSH);

    if ((err_code = tty_write(PortFD, cmd_line, strlen(cmd_line), &nbytes_written) != TTY_OK))
    {
        tty_error_msg(err_code, qfocus_error, MAXRBUF);
        LOGF_ERROR("TTY error detected: %s", qfocus_error);
        return -1;
    }

    return nbytes_written;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
int QFocuser::ReadResponse(char *buf)
{
    char qfocus_error[MAXRBUF];

    int bytesRead = 0;
    int err_code;

    while (true)
    {
        if ((err_code = tty_read_section(PortFD, buf, '}', TIMEOUT, &bytesRead)) != TTY_OK)
        {
            tty_error_msg(err_code, qfocus_error, MAXRBUF);
            LOGF_ERROR("TTY read error detected: %s", qfocus_error);
            return -1;
        }

        memset(buff, 0, USB_CDC_RX_LEN);
        memcpy(buff, buf, bytesRead);

        try
        {
            json cmd_json = json::parse(buff);

            if (cmd_json.find("idx") != cmd_json.end())
            {
                int cmd_id = cmd_json["idx"];
                if (cmd_id == 2 || cmd_id == 3 || cmd_id == 6 || cmd_id == 7 || cmd_id == 11 || cmd_id == 13 || cmd_id == 16)
                {
                    LOGF_DEBUG("<RES> %s", cmd_json.dump().c_str());
                    return bytesRead;
                }
                else if (cmd_id == 1)
                {
                    LOGF_DEBUG("<RES> %s", cmd_json.dump().c_str());
                    if (cmd_json.find("id") != cmd_json.end() && cmd_json.find("version") != cmd_json.end() && cmd_json.find("bv") != cmd_json.end())
                    {
                        cmd_version  = cmd_json["version"];
                        cmd_version_board  = cmd_json["bv"];
                        return bytesRead;
                    }
                }
                else if (cmd_id == 4)
                {
                    if (cmd_json.find("o_t") != cmd_json.end() && cmd_json.find("c_t") != cmd_json.end() && cmd_json.find("c_r") != cmd_json.end())
                    {
                        cmd_out_temp  = cmd_json["o_t"];
                        cmd_chip_temp = cmd_json["c_t"];
                        cmd_voltage = cmd_json["c_r"];
                        return bytesRead;
                    }
                }
                else if (cmd_id == 5)
                {
                    if (cmd_json.find("pos") != cmd_json.end())
                    {
                        cmd_position = cmd_json["pos"];
                        return bytesRead;
                    }
                }
                else if (cmd_id == -1)
                {
                    isReboot = true;
                    return bytesRead;
                }
            }

            //LOGF_INFO("r ----cmd_index pass  %f", 2.9);
        }
        catch (const json::parse_error &e)
        {
            LOGF_ERROR("TTY parse read error detected: %s", e.what());
            return -1;
        }

        break;
    }

    //LOGF_INFO("r -------------------------------x  %f", 2.10);
    return -1;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool QFocuser::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // Make sure it is for us.
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        if (FocusSpeedNP.isNameMatch(name))
        {
            FocusSpeedNP.update(values, names, n);
            int current_speed = FocusSpeedNP[0].getValue();

            // LOGF_INFO("FocusSpeedNP: %d", FocusSpeedNP[0].getValue());

            if (SetFocuserSpeed(FocusSpeedNP[0].getValue()) == false)
            {
                FocusSpeedNP[0].setValue(current_speed);
                FocusSpeedNP.setState(IPS_ALERT);
                FocusSpeedNP.apply();
                saveConfig(true, FocusSpeedNP.getName());
                return false;
            }

            FocusSpeedNP.setState(IPS_OK);
            FocusSpeedNP.apply();
            saveConfig(true, FocusSpeedNP.getName());
            return true;
        }

    }

    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool QFocuser::Handshake()
{
    // TODO: Any initial communciation needed with our focuser, we have an active connection.
    char ret_cmd[MAX_CMD] = {0};
    std::string command = create_cmd(1, true, 0);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("handshake read error %d", ret_chk);
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("handshake read error %d", ret_chk);
        return false;
    }

    int value = cmd_version;
    LOGF_INFO("version: %d", value);

    FOCUSVersionNP[0].setValue(cmd_version);
    BOARDVersionNP[0].setValue(cmd_version_board);

    LOGF_DEBUG("FOCUSVersionNP: %d", FOCUSVersionNP[0].getValue());
    LOGF_DEBUG("BOARDVersionNP: %d", BOARDVersionNP[0].getValue());

    updateTemperature();

    if(cmd_voltage == 0)
    {
        std::string command_ = create_cmd(16, true, 0);
        LOGF_DEBUG("<CMD> %s", command_.c_str());
        ret_chk = SendCommand(const_cast<char *>(command_.c_str()));

        if (ret_chk < 0)
        {
            LOGF_ERROR("Hold set error %d", ret_chk);
            return false;
        }
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void QFocuser::TimerHit()
{
    if (!isConnected())
        return;

    double prevPos = currentPosition;
    double newPos  = 0;

    int rc = updatePosition(&newPos);
    if (rc >= 0)
    {
        currentPosition = newPos;
        if (prevPos != currentPosition)
            IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    if (initTargetPos == true)
    {
        targetPos     = currentPosition;
        initTargetPos = false;
    }

    if (currentPosition == targetPos)
    {
        FocusAbsPosNP.s = IPS_OK;

        if (FocusRelPosNP.s == IPS_BUSY)
        {
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusRelPosNP, nullptr);
        }
    }

    IDSetNumber(&FocusAbsPosNP, nullptr);
    if (FocusAbsPosNP.s == IPS_BUSY)
    {
        timerID = SetTimer(1000);
        return;
    }

    GetFocusParams();

    timerID = SetTimer(getCurrentPollingPeriod());
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState QFocuser::MoveAbsFocuser(uint32_t targetTicks)
{
    targetPos = targetTicks;

    int ret = -1;

    if (targetTicks < FocusAbsPosN[0].min || targetTicks > FocusAbsPosN[0].max)
    {
        LOG_DEBUG("Error, requested position is out of range.");
        return IPS_ALERT;
    }

    if ((ret = updatePositionAbsolute(targetTicks)) < 0)
    {
        LOGF_DEBUG("Read out of the absolute movement failed %3d", ret);
        return IPS_ALERT;
    }

    RemoveTimer(timerID);
    timerID = SetTimer(250);
    return IPS_BUSY;
}

bool QFocuser::ReverseFocuser(bool enabled)
{
    int ret = -1;
    int val = enabled ? 1 : 0;

    if ((ret = updateSetReverse(val)) < 0)
    {
        LOGF_ERROR("updateSetReverse failed %3d", ret);
        return false;
    }

    FocusReverseS[0].s = enabled ? ISS_ON : ISS_OFF;
    IDSetSwitch(&FocusReverseSP, nullptr);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState QFocuser::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int ret = -1;
    if (dir == FOCUS_INWARD)
    {
        targetPos = targetPos + ticks;
        if ((ret = updatePositionRelativeInward(ticks)) < 0)
        {
            LOGF_ERROR("updatePositionRelativeInward failed %3d", ret);
            return IPS_ALERT;
        }
    }
    else
    {
        targetPos = targetPos - ticks;
        if ((ret = updatePositionRelativeOutward(ticks)) < 0)
        {
            LOGF_ERROR("updatePositionRelativeOutward failed %3d", ret);
            return IPS_ALERT;
        }
    }

    RemoveTimer(timerID);
    timerID = SetTimer(250); // Set a timer to call the function TimerHit after 250 milliseconds
    return IPS_BUSY;
}

/////////////////////////////////////////////////////////////////////////////
/// Max:8  Min:0 (The fastest speed is 0, the slowest is 8)
/////////////////////////////////////////////////////////////////////////////
bool QFocuser::SetFocuserSpeed(int speed)
{
    if (speed < FocusSpeedMin || speed > FocusSpeedMax)
    {
        LOG_ERROR("Error, requested speed value is out of range(Min:0, Max:8).");
        return false;
    }

    int ret = -1;

    if ((ret = updateSetSpeed(speed)) < 0)
    {
        LOGF_ERROR("Read out of the SetSpeed movement failed %3d", ret);
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool QFocuser::SyncFocuser(uint32_t ticks)
{
    int ret = -1;

    if (ticks < FocusAbsPosN[0].min || ticks > FocusAbsPosN[0].max)
    {
        LOGF_ERROR("Error, requested ticks value is out of range(Max: %d, Min: %d).", FocusAbsPosN[0].max,
                   FocusAbsPosN[0].min);
        return false;
    }

    if ((ret = updateSetPosition(ticks)) < 0)
    {
        LOGF_ERROR("Set Focuser Position failed %3d", ret);
        return false;
    }

    targetPos = ticks;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool QFocuser::AbortFocuser()
{
    char ret_cmd[MAX_CMD] = {0};
    std::string command = create_cmd(3, false, 0);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("AbortFocuser error %d", ret_chk);
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("AbortFocuser error %d", ret_chk);
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Update State to Focuser
/////////////////////////////////////////////////////////////////////////////
int QFocuser::updatePositionAbsolute(double value)
{
    char ret_cmd[MAX_CMD];
    std::string command = create_cmd(6, true, value);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("updatePositionAbsolute %.f error %d", value, ret_chk);
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("updatePositionAbsolute %.f error %d", value, ret_chk);
        return false;
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
int QFocuser::updateSetSpeed(int value)
{
    char ret_cmd[MAX_CMD];
    std::string command = create_cmd(13, true, 4 - value);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("updateSetSpeed %d error %d", value, ret_chk);
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("updateSetSpeed %d error %d", value, ret_chk);
        return false;
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
int QFocuser::updatePositionRelativeInward(double value)
{
    bool dir_param = false;
    char ret_cmd[MAX_CMD] = {0};
    std::string command = create_cmd(2, dir_param, value);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("updatePositionRelativeInward %.f error %d", value, ret_chk);
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("updatePositionRelativeInward %.f error %d", value, ret_chk);
        return false;
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
int QFocuser::updatePositionRelativeOutward(double value) //MoveRelFocuser
{
    bool dir_param = true;
    char ret_cmd[MAX_CMD] = {0};
    std::string command = create_cmd(2, dir_param, value);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("updatePositionRelativeOutward %.f error %d", value, ret_chk);
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("updatePositionRelativeOutward %.f error %d", value, ret_chk);
        return false;
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
int QFocuser::updatePosition(double *value)
{
    char ret_cmd[MAX_CMD] = {0};
    std::string command = create_cmd(5, true, 0);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("updatePosition error %d", ret_chk);
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("updatePosition error %d", ret_chk);
        return false;
    }

    *value = cmd_position;
    return 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
int QFocuser::updateTemperature()
{
    char ret_cmd[MAX_CMD] = {0};
    std::string command = create_cmd(4, true, 0);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("updateTemperature Send error %d", ret_chk);
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("updateTemperature Read error %d", ret_chk);
        return false;
    }

    TemperatureNP[0].setValue((cmd_out_temp) / 1000.0);
    TemperatureChipNP[0].setValue(cmd_chip_temp / 1000.0);
    VoltageNP[0].setValue(cmd_voltage / 10.0);

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
int QFocuser::updateSetPosition(int value)
{
    char ret_cmd[MAX_CMD] = {0};
    std::string command = create_cmd(11, true, value);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("updateSetPosition %d error %d", value, ret_chk);
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("updateSetPosition %d error %d", value, ret_chk);
        return false;
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
int QFocuser::updateSetReverse(int value)
{
    char ret_cmd[MAX_CMD] = {0};
    std::string command = create_cmd(7, true, value);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("updateSetReverse %d error %d", value, ret_chk);
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("updateSetReverse %d error %d", value, ret_chk);
        return false;
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void QFocuser::GetFocusParams()
{
    int ret = -1;

    if ((ret = updatePosition(&currentPosition)) < 0)
    {
        FocusAbsPosNP.s = IPS_ALERT;
        LOGF_ERROR("Unknown error while reading  position: %d", ret);
        IDSetNumber(&FocusAbsPosNP, nullptr);
        return;
    }

    if ((ret = updateTemperature()) < 0)
    {
        TemperatureNP.setState(IPS_ALERT);
        TemperatureChipNP.setState(IPS_ALERT);
        LOG_ERROR("Unknown error while reading temperature.");
        TemperatureNP.apply();
        TemperatureChipNP.apply();
        return;
    }

    FocusAbsPosNP.s = IPS_OK;
    IDSetNumber(&FocusAbsPosNP, nullptr);

    TemperatureNP.setState(IPS_OK);
    TemperatureChipNP.setState(IPS_OK);
    VoltageNP.setState(IPS_OK);
    FOCUSVersionNP.setState(IPS_OK);
    BOARDVersionNP.setState(IPS_OK);
    TemperatureNP.apply();
    TemperatureChipNP.apply();
    VoltageNP.apply();
    FOCUSVersionNP.apply();
    BOARDVersionNP.apply();
}

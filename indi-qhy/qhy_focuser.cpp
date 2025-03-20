/*
    QHY QFocuser

    Copyright (C) 2024 Chen Jiaqi (cjq@qhyccd.com)

    Copyright (C) 2025 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

    // Configure the inherited FocusSpeedN property
    FocusSpeedNP[0].setMin(0);
    FocusSpeedNP[0].setMax(8);
    FocusSpeedNP[0].setStep(1);
    FocusSpeedNP[0].setValue(0);

    FocusAbsPosNP[0].setMin(-64000.);
    FocusAbsPosNP[0].setMax(64000.);
    FocusAbsPosNP[0].setValue(0);
    FocusAbsPosNP[0].setStep(1000);

    FocusMaxPosNP[0].setMax(2e6);

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
        defineProperty(VoltageNP);
        defineProperty(FOCUSVersionNP);
        defineProperty(BOARDVersionNP);
    }
    else
    {
        // TODO: Call deleteProperty for any custom properties only visible when connected.
        deleteProperty(TemperatureNP);
        deleteProperty(TemperatureChipNP);
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
int QFocuser::ReadResponse(char *buf, int &cmd_id)
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
                cmd_id = cmd_json["idx"];  // 获取 cmd_id

                if (cmd_id == 2 || cmd_id == 3 || cmd_id == 6 || cmd_id == 7 || cmd_id == 11 || cmd_id == 13 || cmd_id == 16)
                {
                    LOGF_DEBUG("<RES> %s", cmd_json.dump().c_str());
                    return bytesRead;  // 返回字节数
                }
                else if (cmd_id == 1)
                {
                    LOGF_DEBUG("<RES> %s", cmd_json.dump().c_str());

                    if (cmd_json.find("id") == cmd_json.end())
                    {
                        return -1;
                    }
                    if (cmd_json.find("version") == cmd_json.end())
                    {
                        return -1;
                    }

                    cmd_version = cmd_json["version"];

                    if (cmd_json.find("bv") != cmd_json.end())
                    {
                        cmd_version_board = cmd_json["bv"];
                    }

                    return bytesRead;  // 返回字节数
                }
                else if (cmd_id == 4)
                {
                    if (cmd_json.find("o_t") != cmd_json.end() && cmd_json.find("c_t") != cmd_json.end()
                            && cmd_json.find("c_r") != cmd_json.end())
                    {
                        cmd_out_temp = cmd_json["o_t"];
                        cmd_chip_temp = cmd_json["c_t"];
                        cmd_voltage = cmd_json["c_r"];
                        return bytesRead;  // 返回字节数
                    }
                }
                else if (cmd_id == 5)
                {
                    if (cmd_json.find("pos") != cmd_json.end())
                    {
                        cmd_position = cmd_json["pos"];
                        return bytesRead;  // 返回字节数
                    }
                }
                else if (cmd_id == -1)
                {
                    isReboot = true;
                    return bytesRead;  // 返回字节数
                }
            }
        }
        catch (const json::parse_error &e)
        {
            LOGF_ERROR("TTY parse read error detected: %s", e.what());
            return -1;
        }

        break;
    }

    return -1;  // 如果未处理任何情况，返回 -1
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

    int cmd_id = 0;
    ret_chk = ReadResponse(ret_cmd, cmd_id);
    if (ret_chk < 0)
    {
        LOGF_ERROR("handshake read error %d", ret_chk);
        return false;
    }

    if(cmd_id != 1)
    {
        ret_chk = SendCommand(const_cast<char *>(command.c_str()));
        if (ret_chk < 0)
        {
            LOGF_ERROR("handshake read error %d", ret_chk);
            return false;
        }

        ret_chk = ReadResponse(ret_cmd, cmd_id);
        if (ret_chk < 0)
        {
            LOGF_ERROR("handshake read error %d", ret_chk);
            return false;
        }
    }

    int value = cmd_version;
    LOGF_INFO("QFocuser Version: %d", value);

    FOCUSVersionNP[0].setValue(cmd_version);
    BOARDVersionNP[0].setValue(cmd_version_board);

    LOGF_DEBUG("FOCUSVersionNP: %d", FOCUSVersionNP[0].getValue());
    LOGF_DEBUG("BOARDVersionNP: %d", BOARDVersionNP[0].getValue());

    double newPos = 0;
    if (getPosition(newPos))
    {
        FocusAbsPosNP[0].setValue(newPos);
        lastPosition = newPos;
        LOGF_INFO("QFocuser current Position: %f", newPos);

        // Initialize target position to current position
        targetPos = FocusAbsPosNP[0].getValue();

        // Update client with initial position
        FocusAbsPosNP.apply();
    }

    getTemperature();
    lastOutTemp = TemperatureNP[0].getValue();
    lastChipTemp = TemperatureChipNP[0].getValue();
    lastVoltage = VoltageNP[0].getValue();

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

    double prevPos = FocusAbsPosNP[0].getValue();
    double newPos  = 0;
    IPState prevState = FocusAbsPosNP.getState();

    if (getPosition(newPos))
    {
        FocusAbsPosNP[0].setValue(newPos);
    }


    if (FocusAbsPosNP[0].getValue() == targetPos && FocusAbsPosNP.getState() == IPS_BUSY)
    {
        FocusAbsPosNP.setState(IPS_OK);

        if (FocusRelPosNP.getState() == IPS_BUSY)
        {
            FocusRelPosNP.setState(IPS_OK);
            FocusRelPosNP.apply();
        }
    }

    // Only update the client if position or state changed
    if (prevPos != FocusAbsPosNP[0].getValue() || prevState != FocusAbsPosNP.getState())
    {
        FocusAbsPosNP.apply();
    }

    if (FocusAbsPosNP.getState() != IPS_BUSY)
        GetFocusParams();

    SetTimer(getCurrentPollingPeriod());
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState QFocuser::MoveAbsFocuser(uint32_t targetTicks)
{
    if (targetTicks < FocusAbsPosNP[0].getMin() || targetTicks > FocusAbsPosNP[0].getMax())
    {
        LOG_DEBUG("Error, requested position is out of range.");
        return IPS_ALERT;
    }

    if (!setAbsolutePosition(targetTicks))
    {
        return IPS_ALERT;
    }

    targetPos = targetTicks;
    return IPS_BUSY;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool QFocuser::ReverseFocuser(bool enabled)
{
    int val = enabled ? 1 : 0;

    if (!setReverseDirection(val))
    {
        return false;
    }

    FocusReverseSP[0].setState(enabled ? ISS_ON : ISS_OFF);
    FocusReverseSP.apply();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
IPState QFocuser::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    int32_t newPosition = 0;

    if (dir == FOCUS_INWARD)
        newPosition = FocusAbsPosNP[0].getValue() - ticks;
    else
        newPosition = FocusAbsPosNP[0].getValue() + ticks;

    // Clamp
    newPosition = std::max(static_cast<int32_t>(FocusAbsPosNP[0].getMin()),
                           std::min(static_cast<int32_t>(FocusAbsPosNP[0].getMax()), newPosition));

    // Use MoveAbsFocuser to handle the absolute movement
    IPState result = MoveAbsFocuser(newPosition);

    if (result == IPS_BUSY)
    {
        FocusRelPosNP[0].setValue(ticks);
        FocusRelPosNP.setState(IPS_BUSY);
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////////
/// Max:8  Min:0 (The fastest speed is 0, the slowest is 8)
/////////////////////////////////////////////////////////////////////////////
bool QFocuser::SetFocuserSpeed(int speed)
{
    char ret_cmd[MAX_CMD];
    std::string command = create_cmd(13, true, 4 - speed);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("setSpeed %d error %d", speed, ret_chk);
        return false;
    }

    int cmd_id = 0;
    ret_chk = ReadResponse(ret_cmd, cmd_id);
    if (ret_chk < 0)
    {
        LOGF_ERROR("setSpeed %d error %d", speed, ret_chk);
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool QFocuser::SyncFocuser(uint32_t ticks)
{

    if (ticks < FocusAbsPosNP[0].getMin() || ticks > FocusAbsPosNP[0].getMax())
    {
        LOGF_ERROR("Error, requested ticks value is out of range(Max: %d, Min: %d).", FocusAbsPosNP[0].getMax(),
                   FocusAbsPosNP[0].getMin());
        return false;
    }

    if (!syncPosition(ticks))
    {
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

    int cmd_id = 0;
    ret_chk = ReadResponse(ret_cmd, cmd_id);
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
bool QFocuser::setAbsolutePosition(double position)
{
    char ret_cmd[MAX_CMD];
    std::string command = create_cmd(6, true, position);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("setAbsolutePosition %.f error %d", position, ret_chk);
        return false;
    }

    int cmd_id = 0;
    ret_chk = ReadResponse(ret_cmd, cmd_id);
    if (ret_chk < 0)
    {
        LOGF_ERROR("setAbsolutePosition %.f error %d", position, ret_chk);
        return false;
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool QFocuser::getPosition(double &position)
{
    char ret_cmd[MAX_CMD] = {0};
    std::string command = create_cmd(5, true, 0);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("getPosition send error %d", ret_chk);
        return false;
    }

    int cmd_id = 0;
    ret_chk = ReadResponse(ret_cmd, cmd_id);
    if (ret_chk < 0)
    {
        LOGF_ERROR("getPosition read error %d", ret_chk);
        return false;
    }

    if(cmd_id != 5)
    {
        ret_chk = SendCommand(const_cast<char *>(command.c_str()));
        if (ret_chk < 0)
        {
            LOGF_ERROR("getPosition send error %d", ret_chk);
            return false;
        }

        ret_chk = ReadResponse(ret_cmd, cmd_id);
        if (ret_chk < 0)
        {
            LOGF_ERROR("getPosition read error %d", ret_chk);
            return false;
        }
    }

    position = cmd_position;

    FocusAbsPosNP[0].setValue(cmd_position);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool QFocuser::getTemperature()
{
    char ret_cmd[MAX_CMD] = {0};
    std::string command = create_cmd(4, true, 0);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("getTemperature Send error %d", ret_chk);
        return false;
    }

    int cmd_id = 0;
    ret_chk = ReadResponse(ret_cmd, cmd_id);
    if (ret_chk < 0)
    {
        LOGF_ERROR("getTemperature Read error %d", ret_chk);
        return false;
    }

    TemperatureNP[0].setValue((cmd_out_temp) / 1000.0);
    TemperatureChipNP[0].setValue(cmd_chip_temp / 1000.0);
    VoltageNP[0].setValue(cmd_voltage / 10.0);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool QFocuser::syncPosition(int position)
{
    char ret_cmd[MAX_CMD] = {0};
    std::string command = create_cmd(11, true, position);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("syncPosition %d error %d", position, ret_chk);
        return false;
    }

    int cmd_id = 0;
    ret_chk = ReadResponse(ret_cmd, cmd_id);
    if (ret_chk < 0)
    {
        LOGF_ERROR("syncPosition %d error %d", position, ret_chk);
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool QFocuser::setReverseDirection(int enabled)
{
    char ret_cmd[MAX_CMD] = {0};
    std::string command = create_cmd(7, true, enabled);
    LOGF_DEBUG("<CMD> %s", command.c_str());

    auto ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR("setReverseDirection %d error %d", enabled, ret_chk);
        return false;
    }

    int cmd_id = 0;
    ret_chk = ReadResponse(ret_cmd, cmd_id);
    if (ret_chk < 0)
    {
        LOGF_ERROR("setReverseDirection %d error %d", enabled, ret_chk);
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
void QFocuser::GetFocusParams()
{

    if (!getTemperature())
    {
        TemperatureNP.setState(IPS_ALERT);
        TemperatureChipNP.setState(IPS_ALERT);
        TemperatureNP.apply();
        TemperatureChipNP.apply();
        return;
    }

    // Only update temperature and voltage if they've changed significantly
    if (std::abs(lastOutTemp - TemperatureNP[0].getValue()) >= 0.1)
    {
        lastOutTemp = TemperatureNP[0].getValue();
        TemperatureNP.setState(IPS_OK);
        TemperatureNP.apply();
    }

    if (std::abs(lastChipTemp - TemperatureChipNP[0].getValue()) >= 0.1)
    {
        lastChipTemp = TemperatureChipNP[0].getValue();
        TemperatureChipNP.setState(IPS_OK);
        TemperatureChipNP.apply();
    }

    if (std::abs(lastVoltage - VoltageNP[0].getValue()) >= 0.1)
    {
        lastVoltage = VoltageNP[0].getValue();
        VoltageNP.setState(IPS_OK);
        VoltageNP.apply();
    }
}

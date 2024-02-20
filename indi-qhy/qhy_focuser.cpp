#include "config.h"
#include "qhy_focuser.h"
#include <indipropertynumber.h>

#include <memory>
#include <cstring>
#include <termios.h>
#include <iostream>
#include <thread>

#include "libindi/indicom.h"
#include "libindi/connectionplugins/connectionserial.h"

#define MAX_CMD 128
#define TIMEOUT 3

#define currentPosition         FocusAbsPosN[0].value
#define currentTemperature      //TemperatureNP[0].getValue()
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
                  FOCUSER_CAN_SYNC); //FOCUSER_HAS_VARIABLE_SPEED | FOCUSER_HAS_BACKLASH
}

const char *QFocuser::getDefaultName()
{
    return "QFocuser"; 
}

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

    FocusSpeedMin = 0;
    FocusSpeedMax = 8;

    simulatedTemperature = 600.0;
    simulatedPosition    = 20000;


    return true;
}

void QFocuser::ISGetProperties(const char *dev)
{
    INDI::Focuser::ISGetProperties(dev);

    // TODO: Call define* for any custom properties.
}

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

int QFocuser::ReadResponse(char *buf)
{
    char qfocus_error[MAXRBUF];

    int bytesRead = 0;
    int err_code;
    int cmd_index = -1;

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

            cmd_index = -1;

            if (cmd_json.find("idx") != cmd_json.end())
            {
                int cmd_id = cmd_json["idx"];
                if (cmd_id == 2 || cmd_id == 3 || cmd_id == 6 || cmd_id == 7 || cmd_id == 11 || cmd_id == 13|| cmd_id == 16)
                {
                    LOGF_INFO("ReadResponse: %s.", cmd_json.dump().c_str());
                    return bytesRead;
                }
                else if (cmd_id == 1)
                {
                    LOGF_INFO("ReadResponse: %s.", cmd_json.dump().c_str());
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
                    // LOG_INFO("Reboot over");
                    return bytesRead;
                }
            }

            LOGF_INFO("r ----cmd_index pass  %f", 2.9);
        }
        catch (const json::parse_error &e)
        {
            LOGF_ERROR("TTY parse read error detected: %s", e.what());
            return -1;
        }

        break;
    }

    LOGF_INFO("r -------------------------------x  %f", 2.10);
    return -1;
}

bool QFocuser::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    LOGF_INFO("ISNewNumber:[%s]", name);
    // Make sure it is for us.
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // TODO: Check to see if this is for any of my custom Number properties.

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

    // Nobody has claimed this, so let the parent handle it
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

bool QFocuser::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    LOGF_INFO("ISNewSwitch:[%s]", name);
    // Make sure it is for us.
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // TODO: Check to see if this is for any of my custom Switch properties.
    }

    // Nobody has claimed this, so let the parent handle it
    return INDI::Focuser::ISNewSwitch(dev, name, states, names, n);
}

bool QFocuser::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    LOGF_INFO("ISNewText:[%s]", name);
    // Make sure it is for us.
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // TODO: Check to see if this is for any of my custom Text properties.
    }

    // Nobody has claimed this, so let the parent handle it
    return INDI::Focuser::ISNewText(dev, name, texts, names, n);
}

bool QFocuser::ISSnoopDevice(XMLEle *root)
{
    // TODO: Check to see if this is for any of my custom Snoops. Fo shizzle.

    return INDI::Focuser::ISSnoopDevice(root);
}

bool QFocuser::saveConfigItems(FILE *fp)
{
    INDI::Focuser::saveConfigItems(fp);

    // TODO: Call IUSaveConfig* for any custom properties I want to save.

    return true;
}

bool QFocuser::Handshake()
{
    LOG_INFO("Hello QFocuser!");
    if (isSimulation())
    {
        LOGF_INFO("Connected successfuly to simulated %s.", getDeviceName());
        return true;
    }

    // TODO: Any initial communciation needed with our focuser, we have an active connection.
    int ret_chk;
    char ret_cmd[MAX_CMD];

    std::string command = create_cmd(1, true, 0);
    LOGF_INFO("SendCommand: %s", command.c_str());
    ret_chk             = SendCommand(const_cast<char *>(command.c_str()));
    // free(cmd_str);
    if (ret_chk < 0)
    {
        LOGF_ERROR("handshake send error %s", "");
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("handshake read error %s", "");
        return false;
    }

    int value = (int)cmd_version;

    LOGF_INFO("version: %d", value);

    FOCUSVersionNP[0].setValue((int)cmd_version);
    BOARDVersionNP[0].setValue((int)cmd_version_board);

    LOGF_INFO("FOCUSVersionNP: %d", FOCUSVersionNP[0].getValue());
    LOGF_INFO("BOARDVersionNP: %d", BOARDVersionNP[0].getValue());

    updateTemperature(0);

    if(cmd_voltage == 0)
    {
        std::string command_ = create_cmd(16, true, 0);
        LOGF_INFO("SendCommand: %s", command_.c_str());
        ret_chk             = SendCommand(const_cast<char *>(command_.c_str()));
        
        if (ret_chk < 0)
        {
            LOGF_ERROR("Hold set error %s", "");
            return false;
        }
    }

    return true;
}

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

    // If you don't call SetTimer, we'll never get called again, until we disconnect and reconnect.
    SetTimer(POLLMS);
}

IPState QFocuser::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    // NOTE: This is needed if we don't specify FOCUSER_CAN_ABS_MOVE
    // TODO: Actual code to move the focuser. You can use IEAddTimer to do a
    // callback after "duration" to stop your focuser.
    LOGF_INFO("MoveFocuser: %d %d %d", dir, speed, duration);

    return IPS_OK;
}

IPState QFocuser::MoveAbsFocuser(uint32_t targetTicks)
{
    LOGF_INFO("MoveAbsFocuser: %d", targetTicks);

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
    int val = 0;
    if (enabled)
    {
        val = 1;
    }

    if ((ret = updateSetReverse(val)) < 0)
    {
        LOGF_DEBUG("updateSetReverse failed %3d", ret);

        return false;
    }

    if (enabled)
    {
        FocusReverseS[0].s = ISS_ON;
        IDSetSwitch(&FocusReverseSP, nullptr);
    }
    else
    {
        FocusReverseS[0].s = ISS_OFF;
        IDSetSwitch(&FocusReverseSP, nullptr);
    }

    return true;
}

IPState QFocuser::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    LOGF_INFO("MoveRelFocuser: %d %d", dir, ticks);

    int ret = -1;
    if (dir == FOCUS_INWARD)
    {
        targetPos = targetPos + ticks;
        if ((ret = updatePositionRelativeInward(ticks)) < 0)
        {
            LOGF_DEBUG("updatePositionRelativeInward failed %3d", ret);
            return IPS_ALERT;
        }
    }
    else
    {
        targetPos = targetPos - ticks;
        if ((ret = updatePositionRelativeOutward(ticks)) < 0)
        {
            LOGF_DEBUG("updatePositionRelativeOutward failed %3d", ret);
            return IPS_ALERT;
        }
    }

    RemoveTimer(timerID);
    timerID = SetTimer(250); // Set a timer to call the function TimerHit after 250 milliseconds
    return IPS_BUSY;
}

bool QFocuser::SetFocuserSpeed(int speed) // Max:8  Min:0 (The fastest speed is 0, the slowest is 8)
{
    LOGF_INFO("SetFocuserSpeed: %d", speed);

    if (speed < FocusSpeedMin || speed > FocusSpeedMax)
    {
        LOG_DEBUG("Error, requested speed value is out of range(Min:0, Max:8).");
        return false;
    }

    int ret = -1;

    if ((ret = updateSetSpeed(speed)) < 0)
    {
        LOGF_DEBUG("Read out of the SetSpeed movement failed %3d", ret);
        return false;
    }

    return true;
}

bool QFocuser::SyncFocuser(uint32_t ticks)
{
    LOG_INFO("SyncFocuser");

    int ret = -1;

    if (ticks < FocusAbsPosN[0].min || ticks > FocusAbsPosN[0].max)
    {
        LOGF_DEBUG("Error, requested ticks value is out of range(Max: %d, Min: %d).", FocusAbsPosN[0].max,
                   FocusAbsPosN[0].min);
        return false;
    }

    if ((ret = updateSetPosition(ticks)) < 0)
    {
        LOGF_DEBUG("Set Focuser Position failed %3d", ret);
        return false;
    }

    targetPos = ticks;

    return true;
}

bool QFocuser::AbortFocuser()
{
    LOG_INFO("AbortFocuser");

    int ret_chk;

    char ret_cmd[MAX_CMD];

    std::string command = create_cmd(3, false, 0);
    LOGF_INFO("SendCommand: %s", command.c_str());
    ret_chk             = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR(" abort send error %s", "");
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("abort read error %s", "");
        return false;
    }

    LOGF_INFO("%s : pass", "abort");
    return true;
}

//---------------------------------------------------------------------------------------------------
//--------------------------------------Update State to Focuser--------------------------------------
//---------------------------------------------------------------------------------------------------

int QFocuser::updatePositionAbsolute(double value) //MoveAbsFocuser
{
    int ret_chk;

    char ret_cmd[MAX_CMD];

    LOGF_INFO("Run abs... %f", value);

    std::string command = create_cmd(6, true, value);
    LOGF_INFO("SendCommand: %s", command.c_str());
    ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR(" run abs send error %s", "");
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("run abs read error %s", "");
        return false;
    }

    LOGF_INFO("Run abs: %g", value);
    return 0;
}

int QFocuser::updateSetSpeed(int value) //SetFocuserSpeed
{
    int ret_chk;

    char ret_cmd[MAX_CMD];

    LOGF_INFO("Set speed... %d", 4 - value);

    std::string command = create_cmd(13, true, 4 - value);
    LOGF_INFO("SendCommand: %s", command.c_str());
    ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR(" Set speed send error %s", "");
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("Set speed read error %s", "");
        return false;
    }

    LOGF_INFO("Set speed: %d", 4 - value);
    return 0;
}

int QFocuser::updatePositionRelativeInward(double value) //MoveRelFocuser
{
    int ret_chk;
    bool dir_param = false;

    char ret_cmd[MAX_CMD];

    LOGF_INFO("Run in...%f", value);

    // if (FocusReverseS[0].s == ISS_ON)
    // {
    //     dir_param = !dir_param;
    //     LOGF_INFO("Run in  Reverse: %d", dir_param);
    // }
    
    std::string command = create_cmd(2, dir_param, value);
    LOGF_INFO("SendCommand: %s", command.c_str());
    ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR(" run in send error %s", "");
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("run in read error %s", "");
        return false;
    }

    LOGF_INFO("Run in: %g", value);
    return 0;
}

int QFocuser::updatePositionRelativeOutward(double value) //MoveRelFocuser
{
    int ret_chk;
    bool dir_param = true;

    char ret_cmd[MAX_CMD];

    LOGF_INFO("Run out...%f", value);

    // if (FocusReverseS[0].s == ISS_ON)
    // {
    //     dir_param = !dir_param;
    //     LOGF_INFO("Run in  Reverse: %d", dir_param);
    // }

    std::string command = create_cmd(2, dir_param, value);
    LOGF_INFO("SendCommand: %s", command.c_str());
    ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR(" run out send error %s", "");
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("run out read error %s", "");
        return false;
    }

    LOGF_INFO("Run out: %g", value);
    return 0;
}

int QFocuser::updatePosition(double *value)
{
    int ret_chk;

    char ret_cmd[MAX_CMD];

    LOG_DEBUG("get pos ");

    if (isSimulation())
    {
        *value = simulatedPosition;
        return 0;
    }
    std::string command = create_cmd(5, true, 0);
    // LOGF_INFO("SendCommand: %s", command.c_str());
    ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR(" pos send error %s", "");
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR(" pos read error %s", "");
        return false;
    }

    *value = (double)cmd_position;

    return 0;
}

int QFocuser::updateTemperature(double *value)
{
    int ret_chk;
    char ret_cmd[MAX_CMD];

    if (isSimulation())
    {
        *value = simulatedPosition;
        return 0;
    }
    std::string command = create_cmd(4, true, 0);
    // LOGF_INFO("SendCommand: %s", command.c_str());
    ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR(" temp send error %s", "");
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR(" temp read error %s", "");
        return false;
    }

    TemperatureNP[0].setValue(((double)cmd_out_temp) / 1000);
    TemperatureChipNP[0].setValue(((double)cmd_chip_temp) / 1000);
    VoltageNP[0].setValue(((int)cmd_voltage) / 10);
    

    return 0;
}

int QFocuser::updateSetPosition(int value)
{
    int ret_chk;

    char ret_cmd[MAX_CMD];

    std::string command = create_cmd(11, true, value);
    LOGF_INFO("SendCommand: %s", command.c_str());
    ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR(" Set Position send error %s", "");
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("Set position read error %s", "");
        return false;
    }

    LOGF_INFO("Set Position: %d", value);
    return 0;
}

int QFocuser::updateSetReverse(int value)
{
    int ret_chk;

    char ret_cmd[MAX_CMD];

    std::string command = create_cmd(7, true, value);
    LOGF_INFO("SendCommand: %s", command.c_str());
    ret_chk = SendCommand(const_cast<char *>(command.c_str()));
    if (ret_chk < 0)
    {
        LOGF_ERROR(" Set Reverse send error %s", "");
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR("Set Reverse read error %s", "");
        return false;
    }

    LOGF_INFO("Set Reverse: %d", value);
    return 0;
}

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

    FocusAbsPosNP.s = IPS_OK;
    IDSetNumber(&FocusAbsPosNP, nullptr);

    // if ((ret = updateTemperature(&currentTemperature)) < 0)
    // {
    //     TemperatureNP.setState(IPS_ALERT);
    //     TemperatureChipNP.setState(IPS_ALERT);
    //     VoltageNP.setState(IPS_ALERT);
    //     FOCUSVersionNP.setState(IPS_ALERT);
    //     BOARDVersionNP.setState(IPS_ALERT);
    //     LOG_ERROR("Unknown error while reading temperature.");
    //     TemperatureNP.apply();
    //     TemperatureChipNP.apply();
    //     VoltageNP.apply();
    //     FOCUSVersionNP.apply();
    //     BOARDVersionNP.apply();
    //     return;
    // }

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

#include "config.h"
#include "qhy_focuser.h"

#include <memory>
#include <cstring>
#include <termios.h>

#include "libindi/indicom.h"
#include "libindi/connectionplugins/connectionserial.h"

#include "cJSON.h"

#define MAX_CMD 64
#define TIMEOUT 3

#define currentPosition         FocusAbsPosN[0].value
#define currentTemperature      TemperatureN[0].value
#define currentRelativeMovement FocusRelPosN[0].value
#define currentAbsoluteMovement FocusAbsPosN[0].value

// We declare an auto pointer to MyCustomDriver.
static std::unique_ptr<QFocuser> qFocus(new QFocuser());

QFocuser::QFocuser()
{
    setVersion(INDI_QHY_VERSION_MAJOR, INDI_QHY_VERSION_MAJOR);

    // Here we tell the base Focuser class what types of connections we can support
    setSupportedConnections(CONNECTION_SERIAL);

    // And here we tell the base class about our focuser's capabilities.
    SetCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_CAN_REVERSE | FOCUSER_CAN_SYNC | FOCUSER_HAS_BACKLASH); //FOCUSER_HAS_VARIABLE_SPEED | 
}

const char *QFocuser::getDefaultName()
{
    return "QFocuser";
}

bool QFocuser::initProperties()
{
    // initialize the parent's properties first
    INDI::Focuser::initProperties();

    // TODO: Add any custom properties you need here.
    IUFillNumber(&FocusSpeedN[0], "FOCUS_SPEED_VALUE", "Focus Speed", "%3.0f", 0.0, 8.0, 1.0, 0.0);
    IUFillNumberVector(&FocusSpeedNP, FocusSpeedN, 1, getDeviceName(), "FOCUS_SPEED", "Speed", MAIN_CONTROL_TAB, IP_RW, 60, IPS_OK);

    IUFillNumber(&TemperatureN[0], "TEMPERATURE", "Celsius", "%0.0f", 0, 65000., 0., 10000.);
    IUFillNumberVector(&TemperatureNP, TemperatureN, 1, getDeviceName(), "FOCUS_TEMPERATURE", "Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    IUFillNumber(&TemperatureChip[0], "TEMPERATURE", "Celsius", "%0.0f", 0, 65000., 0., 10000.);
    IUFillNumberVector(&TemperatureChipVP, TemperatureChip, 1, getDeviceName(), "CHIP_TEMPERATURE", "Chip Temperature", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    FocusAbsPosN[0].min   = -64000.;
    FocusAbsPosN[0].max   = 64000.;
    FocusAbsPosN[0].value = 0;
    FocusAbsPosN[0].step  = 1000;

    FocusSpeedN[0].min = 0;
    FocusSpeedN[0].max = 8;

    simulatedTemperature = 600.0;
    simulatedPosition    = 20000;

    // serialConnection = new Connection::Serial(this);
    // serialConnection->registerHandshake([&]() { return Handshake(); });
    // serialConnection->setDefaultBaudRate(Connection::Serial::B_9600);
    // serialConnection->setDefaultPort("/dev/ttyACM0");
    // registerConnection(serialConnection);

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
        defineProperty(&TemperatureNP);
        defineProperty(&TemperatureChipVP);
        defineProperty(&FocusSpeedNP);

        // GetFocusParams();
    }
    else
    {
        // TODO: Call deleteProperty for any custom properties only visible when connected.
        deleteProperty(TemperatureNP.name);
        deleteProperty(TemperatureChipVP.name);
        deleteProperty(FocusSpeedNP.name);
    }

    return true;
}

char *create_cmd(int cmd_idx, bool dir, int value)
{
    char *string    = NULL;
    cJSON *response = cJSON_CreateObject();
    cJSON *cmd_item = cJSON_CreateNumber(cmd_idx);
    cJSON_AddItemToObject(response, "cmd_id", cmd_item);

    switch (cmd_idx)
    {
        case 1:
        {
            break;
        }
        case 2:
        {
            cJSON *number_dir         = cJSON_CreateNumber(dir ? 1 : -1);
            cJSON *number_target_step = cJSON_CreateNumber(value);
            cJSON_AddItemToObject(response, "dir", number_dir);
            cJSON_AddItemToObject(response, "step", number_target_step);
            break;
        }
        case 6:
        {
            cJSON *number_target_step = cJSON_CreateNumber(value);
            cJSON_AddItemToObject(response, "tar", number_target_step);
            break;
        }
        case 3:
        {
            break;
        }
        case 4:
        {
            break;
        }
        case 5:
        {
            break;
        }
        case 11:
        {
            cJSON *number_target_init = cJSON_CreateNumber(value);
            cJSON_AddItemToObject(response,"init_val",number_target_init);
            break;
        }
        case 13:
        {
            cJSON *number_target_speed = cJSON_CreateNumber(value);
            cJSON_AddItemToObject(response,"speed",number_target_speed);
            break;
        }
        default:
        {
            break;
        }
    }

    string = cJSON_PrintUnformatted(response);

    cJSON_Delete(response);
    return string;
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

        cJSON *cmd_json = cJSON_Parse((const char *)buff);
        if (cmd_json == NULL)
        {
            const char *error_ptr = cJSON_GetErrorPtr();
            if (error_ptr != NULL)
            {
                LOGF_ERROR("TTY parse read error detected: %s", error_ptr);
                continue;
            }
        }
        if (cmd_json != NULL)
        {
            cmd_index     = -1;
            cJSON *cmd_id = cJSON_GetObjectItemCaseSensitive(cmd_json, "idx");
            if (cmd_id == NULL)
            {
                LOGF_ERROR("TTY idx parse error detected: %s", "null");
                continue;
            }

            if (cmd_id->valueint == 1)
            {
                return bytesRead;
            }
            else if (cmd_id->valueint == 2)
            {
                return bytesRead;
            }
            else if (cmd_id->valueint == 3)
            {
                return bytesRead;
            }
            else if (cmd_id->valueint == 4)
            {
                cJSON *out_temp  = cJSON_GetObjectItemCaseSensitive(cmd_json, "o_t");
                cJSON *chip_temp = cJSON_GetObjectItemCaseSensitive(cmd_json, "c_t");
                cmd_out_temp     = out_temp->valueint;
                cmd_chip_temp    = chip_temp->valueint;
                return bytesRead;
            }
            else if (cmd_id->valueint == 5)
            {
                cJSON *pos   = cJSON_GetObjectItemCaseSensitive(cmd_json, "pos");
                cmd_position = pos->valueint;
                return bytesRead;
            }
            else if (cmd_id->valueint == 6)
            {
                return bytesRead;
            }
            else if (cmd_id->valueint == 11)
            {
                return bytesRead;
            }
            else
            {
                return bytesRead;
            }
        }
        LOGF_INFO("r ----cmd_index pass  %f", 2.9);
        cJSON_Delete(cmd_json);
        break;
    }
    LOGF_INFO("r -------------------------------x  %f", 2.10);
    return -1;
}

bool QFocuser::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    LOGF_INFO("ISNewNumber ---------%s", name);
    // Make sure it is for us.
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // TODO: Check to see if this is for any of my custom Number properties. 
    }

    // Set variable focus speed
    if (!strcmp(name, FocusSpeedNP.name))
    {
        FocusSpeedNP.s    = IPS_OK;
        int current_speed = FocusSpeedN[0].value;
        IUUpdateNumber(&FocusSpeedNP, values, names, n);

        if (SetFocuserSpeed(FocusSpeedN[0].value) == false)
        {
            FocusSpeedN[0].value = current_speed;
            FocusSpeedNP.s       = IPS_ALERT;
            saveConfig(true, FocusSpeedNP.name);
        }

        //  Update client display
        IDSetNumber(&FocusSpeedNP, nullptr);
        return true;
    }

    // Nobody has claimed this, so let the parent handle it
    return INDI::Focuser::ISNewNumber(dev, name, values, names, n);
}

bool QFocuser::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
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
    if (isSimulation())
    {
        LOGF_INFO("Connected successfuly to simulated %s.", getDeviceName());
        return true;
    }

    // NOTE: PortFD is set by the base class.
    // PortFD = serialConnection->getPortFD();

    // TODO: Any initial communciation needed with our focuser, we have an active connection.
    int ret_chk;
    char ret_cmd[MAX_CMD];

    char *cmd_str = create_cmd(1, true, 0);
    ret_chk       = SendCommand(cmd_str);
    free(cmd_str);
    if (ret_chk < 0)
    {
        LOGF_ERROR(" handshake send error %s", "");
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if (ret_chk < 0)
    {
        LOGF_ERROR(" handshake read error %s", "");
        return false;
    }

    LOGF_INFO(" handshake  pass %s", "");

    return true;
}

void QFocuser::TimerHit()
{
    if (!isConnected())
        return;

    // TODO: Poll your device if necessary. Otherwise delete this method and it's declaration in the header file.

    // LOG_INFO("timer hit---------1");

    double prevPos = currentPosition;
    double newPos  = 0;

    int rc = updatePosition(&newPos);
    if (rc >= 0)
    {
        currentPosition = newPos;
        if (prevPos != currentPosition)
            IDSetNumber(&FocusAbsPosNP, nullptr);
    }

    if(initTargetPos == true)
    {
        targetPos = currentPosition;
        initTargetPos = false;
    }

    // LOG_INFO("timer hit---------2");
    // LOGF_INFO("TimerHit----pos: %f , %f",currentPosition,targetPos);
    if (currentPosition == targetPos)
    {
        FocusAbsPosNP.s = IPS_OK;
        // LOG_INFO("timer hit---------2.1");
        if (FocusRelPosNP.s == IPS_BUSY)
        {
            FocusRelPosNP.s = IPS_OK;
            IDSetNumber(&FocusRelPosNP, nullptr);
        }
    }
    // LOG_INFO("timer hit---------3");
    IDSetNumber(&FocusAbsPosNP, nullptr);
    if (FocusAbsPosNP.s == IPS_BUSY)
    {
        timerID = SetTimer(1000);
        return;
    }
    // LOG_INFO("timer hit---------4");
    GetFocusParams();

    timerID = SetTimer(getCurrentPollingPeriod());

    // LOGF_INFO("getCurrentPollingPeriod: %d",getCurrentPollingPeriod());

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
    // NOTE: This is needed if we do specify FOCUSER_CAN_ABS_MOVE
    // TODO: Actual code to move the focuser.
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

IPState QFocuser::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    // NOTE: This is needed if we do specify FOCUSER_CAN_REL_MOVE
    // TODO: Actual code to move the focuser.
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
    timerID = SetTimer(250);     // Set a timer to call the function TimerHit after 250 milliseconds
    return IPS_BUSY;
}

bool QFocuser::SetFocuserSpeed(int speed)           // Max:8  Min:0 (The fastest speed is 0, the slowest is 8)
{
    // NOTE: This is needed if we do specify FOCUSER_HAS_VARIABLE_SPEED
    // TODO: Actual code to stop the focuser.
    LOGF_INFO("SetFocuserSpeed: %d", speed);

    if (speed < FocusSpeedN[0].min || speed > FocusSpeedN[0].max)
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

    // RemoveTimer(timerID);
    // timerID = SetTimer(250);
    return true;
}

bool QFocuser::SyncFocuser(uint32_t ticks)
{
    LOG_INFO("SyncFocuser");

    int ret = -1;

    if (ticks < FocusAbsPosN[0].min || ticks > FocusAbsPosN[0].max)
    {
        LOGF_DEBUG("Error, requested ticks value is out of range(Max: %d, Min: %d).",FocusAbsPosN[0].max,FocusAbsPosN[0].min);
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
    // NOTE: This is needed if we do specify FOCUSER_CAN_ABORT
    // TODO: Actual code to stop the focuser.
    LOG_INFO("AbortFocuser");

    int ret_chk;
    char *cmd_str = create_cmd(3, false, 0);
    ret_chk       = SendCommand(cmd_str);
    free(cmd_str);
    if (ret_chk < 0)
    {
        LOGF_ERROR(" abort send error %s", "");
        return false;
    }
    LOGF_INFO("%s : pass", "abort");
    return true;
}

//---------------------------------------------------------------------------------------------------
//--------------------------------------Update State to Focuser--------------------------------------
//---------------------------------------------------------------------------------------------------

int QFocuser::updatePositionAbsolute(double value)          //MoveAbsFocuser
{
    int ret_chk;

    LOGF_INFO("Run abs... %f", value);

    char *cmd_str = create_cmd(6, true, value);

    ret_chk = SendCommand(cmd_str);
    free(cmd_str);
    if (ret_chk < 0)
    {
        LOGF_ERROR(" run abs send error %s", "");
        return false;
    }
    /***
    ret_chk = ReadResponse(ret_cmd);
    if(ret_chk<0){
        LOGF_ERROR(" run abs read error %s","");
        return false;
    }
***/
    LOGF_INFO("Run abs: %g", value);
    return 0;
}

int QFocuser::updateSetSpeed(int value)           //SetFocuserSpeed
{
    int ret_chk;

    LOGF_INFO("Set speed... %d", 4 - value );

    char *cmd_str = create_cmd(13, true, 4 - value );

    ret_chk = SendCommand(cmd_str);
    free(cmd_str);
    if (ret_chk < 0)
    {
        LOGF_ERROR(" Set speed send error %s", "");
        return false;
    }

    LOGF_INFO("Set speed: %d", 4 - value );
    return 0;
}

int QFocuser::updatePositionRelativeInward(double value)    //MoveRelFocuser
{
    int ret_chk;
    bool dir_param = true;

    LOGF_INFO("Run in...%f", value);

    if (RevertDirS[0].s == ISS_ON)
    {
        dir_param = !dir_param;
        LOGF_INFO("Run in  Reverse...%d", dir_param);
    }
    LOGF_INFO("............Run in  Reverse...%d", dir_param);
    char *cmd_str = create_cmd(2, dir_param, value);

    ret_chk = SendCommand(cmd_str);
    free(cmd_str);
    if (ret_chk < 0)
    {
        LOGF_ERROR(" run in send error %s", "");
        return false;
    }

    LOGF_INFO("Run in: %g", value);
    return 0;
}

int QFocuser::updatePositionRelativeOutward(double value)   //MoveRelFocuser
{
    int ret_chk;
    bool dir_param = false;

    LOGF_INFO("Run out...%f", value);

    if (RevertDirS[0].s == ISS_ON)
    {
        dir_param = !dir_param;
    }
    char *cmd_str = create_cmd(2, dir_param, value);

    ret_chk = SendCommand(cmd_str);
    free(cmd_str);
    if (ret_chk < 0)
    {
        LOGF_ERROR(" run out send error %s", "");
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
    char* cmd_str = create_cmd(5,true,0);


    ret_chk = SendCommand(cmd_str);
    free(cmd_str);
    if(ret_chk<0){
        LOGF_ERROR(" pos send error %s","");
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if(ret_chk<0){
        LOGF_ERROR(" pos read error %s","");
        return false;
    }

   
    *value = (double)cmd_position;

    // LOGF_INFO("pos: %g", *value);

    return 0;
}

int QFocuser::updateTemperature(double *value)
{
    int ret_chk;
    char ret_cmd[MAX_CMD];
    // LOGF_INFO("get temp...%s","");

    if (isSimulation())
    {
        *value = simulatedPosition;
        return 0;
    }
    char* cmd_str = create_cmd(4,true,0);


    ret_chk = SendCommand(cmd_str);
    free(cmd_str);
    if(ret_chk<0){
        LOGF_ERROR(" temp send error %s","");
        return false;
    }

    ret_chk = ReadResponse(ret_cmd);
    if(ret_chk<0){
        LOGF_ERROR(" temp read error %s","");
        return false;
    }

   
    // *value = ((double)cmd_out_temp)/1000;
    TemperatureN[0].value = ((double)cmd_out_temp)/1000;
    TemperatureChip[0].value = ((double)cmd_chip_temp)/1000;

    // LOGF_INFO("get temp: %g", TemperatureN[0].value);

    return 0;
}

int QFocuser::updateSetPosition(int value)
{
     int ret_chk;

    LOGF_INFO("Set Position... %d", value);

    char *cmd_str = create_cmd(11, true, value);

    ret_chk = SendCommand(cmd_str);
    free(cmd_str);
    if (ret_chk < 0)
    {
        LOGF_ERROR(" Set Position send error %s", "");
        return false;
    }

    LOGF_INFO("Set Position: %d", value);
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

    if ((ret = updateTemperature(&currentTemperature)) < 0)
    {
        TemperatureNP.s = IPS_ALERT;
        TemperatureChipVP.s = IPS_ALERT;
        LOG_ERROR("Unknown error while reading temperature.");
        IDSetNumber(&TemperatureNP, nullptr);
        IDSetNumber(&TemperatureChipVP, nullptr);
        return;
    }
    // LOGF_INFO("TemperatureNP.name: %s", TemperatureNP.name);
    TemperatureNP.s = IPS_OK;
    IDSetNumber(&TemperatureNP, nullptr);
    TemperatureChipVP.s = IPS_OK;
    IDSetNumber(&TemperatureChipVP, nullptr);
}

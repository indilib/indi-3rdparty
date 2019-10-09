/*******************************************************************************
 NexDome

 Copyright(c) 2019 Jasem Mutlaq. All rights reserved.

 NexDome Driver for Firmware v3+

 Change Log:

 2019.10.07: Driver is completely re-written to work with Firmware v3 since
 Firmware v1 is obsolete from NexDome.
 2017.01.01: Driver for Firmware v1 is developed by Rozeware Development Ltd.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License version 2 as published by the Free Software Foundation.
 .
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 .
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
*******************************************************************************/
#include "nex_dome.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <memory>
#include <regex>
#include <termios.h>

#include <indicom.h>

#include "config.h"

static std::unique_ptr<NexDome> nexDome(new NexDome());

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
void ISGetProperties(const char *dev)
{
    nexDome->ISGetProperties(dev);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    nexDome->ISNewSwitch(dev, name, states, names, num);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
void ISNewText(	const char *dev, const char *name, char *texts[], char *names[], int num)
{
    nexDome->ISNewText(dev, name, texts, names, num);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    nexDome->ISNewNumber(dev, name, values, names, num);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
void ISSnoopDevice (XMLEle *root)
{
    nexDome->ISSnoopDevice(root);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
NexDome::NexDome()
{
    setVersion(INDI_NEXDOME_VERSION_MAJOR, INDI_NEXDOME_VERSION_MINOR);

    SetDomeCapability(DOME_CAN_ABORT |
                      DOME_CAN_ABS_MOVE |
                      DOME_CAN_PARK |
                      DOME_HAS_SHUTTER |
                      DOME_CAN_SYNC);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::initProperties()
{
    INDI::Dome::initProperties();

    SetParkDataType(PARK_AZ);

    ///////////////////////////////////////////////////////////////////////////////
    /// Operations (Home + Cabliration)
    ///////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&OperationS[OP_HOME], "OP_HOME", "Home", ISS_OFF);
    IUFillSwitch(&OperationS[OP_CALIBRATE], "OP_CALIBRATE", "Calibrate", ISS_OFF);
    IUFillSwitchVector(&OperationSP, OperationS, 2, getDeviceName(), "DOME_OPERATION", "Operation", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Home Position
    ///////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&HomePositionN[0], "HOME_POSITON", "degrees", "%.f", 0.0, 360.0, 0.0, 0);
    IUFillNumberVector(&HomePositionNP, HomePositionN, 1, getDeviceName(), "HOME_POS", "Home Az", SITE_TAB, IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Battery
    ///////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&BatteryLevelN[ND::ROTATOR], "BATTERY_ROTATOR", "Rotator", "%.2f", 0.0, 16.0, 0.0, 0);
    IUFillNumber(&BatteryLevelN[ND::SHUTTER], "BATTERY_SHUTTER", "Shutter", "%.2f", 0.0, 16.0, 0.0, 0);
    IUFillNumberVector(&BatteryLevelNP, BatteryLevelN, 2, getDeviceName(), "BATTERY", "Battery Level", ND::SHUTTER_TAB.c_str(), IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Firmware Info
    ///////////////////////////////////////////////////////////////////////////////
    IUFillText(&FirmwareVersionT[0], "FIRMWARE_VERSION", "Version", "");
    IUFillTextVector(&FirmwareVersionTP, FirmwareVersionT, 1, getDeviceName(), "FIRMWARE", "Firmware", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Close Shutter on Park?
    ///////////////////////////////////////////////////////////////////////////////
    IUFillSwitch(&CloseShutterOnParkS[ND::ENABLED], "ENABLED", "Enabled", ISS_ON);
    IUFillSwitch(&CloseShutterOnParkS[ND::DISABLED], "DISABLED", "Disabled", ISS_OFF);
    IUFillSwitchVector(&CloseShutterOnParkSP, CloseShutterOnParkS, 2, getDeviceName(), "DOME_CLOSE_SHUTTER_ON_PARK", "Close Shutter on Park",
                       ND::SHUTTER_TAB.c_str(), IP_RW, ISR_ATMOST1, 0, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Rotator Settings
    ///////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&RotatorSettingsN[ACCELERATION_RAMP], "ACCELERATION_RAMP", "Acceleration Ramp (ms)", "%.f", 0.0, 5000, 1000.0, 0);
    IUFillNumber(&RotatorSettingsN[DEAD_ZONE], "DEAD_ZONE", "Dead Zone (steps)", "%.f", 0.0, 10000, 100.0, 300);
    IUFillNumber(&RotatorSettingsN[TRAVEL_RANGE], "TRAVEL_RANGE", "Travel Range (steps)", "%.f", 0.0, 64000, 1000.0, 0);
    IUFillNumber(&RotatorSettingsN[VELOCITY], "VELOCITY", "Velocity (step/s)", "%.f", 0.0, 5000, 1000.0, 0);
    IUFillNumberVector(&RotatorSettingsNP, RotatorSettingsN, 4, getDeviceName(), "ROTATOR_SETTINGS", "Rotator", ND::ROTATOR_TAB.c_str(),
                       IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////
    /// Shutter Settings
    ///////////////////////////////////////////////////////////////////////////////
    IUFillNumber(&ShutterSettingsN[ACCELERATION_RAMP], "ACCELERATION_RAMP", "Acceleration Ramp (ms)", "%.f", 0.0, 5000, 1000.0, 0);
    IUFillNumber(&ShutterSettingsN[DEAD_ZONE], "DEAD_ZONE", "Dead Zone (steps)", "%.f", 0.0, 10000, 100.0, 300);
    IUFillNumber(&ShutterSettingsN[TRAVEL_RANGE], "TRAVEL_RANGE", "Travel Range (steps)", "%.f", 0.0, 64000, 1000.0, 0);
    IUFillNumber(&ShutterSettingsN[VELOCITY], "VELOCITY", "Velocity (step/s)", "%.f", 0.0, 5000, 1000.0, 0);
    IUFillNumberVector(&ShutterSettingsNP, ShutterSettingsN, 4, getDeviceName(), "Shutter_SETTINGS", "Shutter", ND::SHUTTER_TAB.c_str(),
                       IP_RW, 60, IPS_IDLE);

    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::Handshake()
{
    std::string value;

    if (getParameter(ND::SEMANTIC_VERSION, ND::ROTATOR, value))
    {
        LOGF_INFO("Detected firmware version %s", value.c_str());
        if (value < ND::MINIMUM_VERSION)
        {
            LOGF_ERROR("Version %s is not supported. Please upgrade to version %s or higher.", value.c_str(), ND::MINIMUM_VERSION.c_str());
            return false;
        }

        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
const char * NexDome::getDefaultName()
{
    return "NexDome";
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        getStartupValues();

        defineSwitch(&OperationSP);
        defineNumber(&HomePositionNP);
        defineNumber(&BatteryLevelNP);
        defineText(&FirmwareVersionTP);
        if (HasShutter())
            defineSwitch(&CloseShutterOnParkSP);
    }
    else
    {
        deleteProperty(OperationSP.name);
        deleteProperty(HomePositionNP.name);
        deleteProperty(BatteryLevelNP.name);
        deleteProperty(FirmwareVersionTP.name);
        if (HasShutter())
            deleteProperty(CloseShutterOnParkSP.name);
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(!strcmp(dev, getDeviceName()))
    {
        ///////////////////////////////////////////////////////////////////////////////
        /// Operation Command
        ///////////////////////////////////////////////////////////////////////////////
        if(!strcmp(name, OperationSP.name))
        {
            return true;
        }

        ///////////////////////////////////////////////////////////////////////////////
        /// Close Shutter on Park
        ///////////////////////////////////////////////////////////////////////////////
        if (!strcmp(name, CloseShutterOnParkSP.name))
        {
            IUUpdateSwitch(&CloseShutterOnParkSP, states, names, n);
            CloseShutterOnParkSP.s = IPS_OK;
            IDSetSwitch(&CloseShutterOnParkSP, nullptr);
            return true;
        }
    }
    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

///////////////////////////////////////////////////////////////////////////////
/// Sync
///////////////////////////////////////////////////////////////////////////////
bool NexDome::Sync(double az)
{
    return false;
}

///////////////////////////////////////////////////////////////////////////////
/// Timer
///////////////////////////////////////////////////////////////////////////////
void NexDome::TimerHit()
{
    SetTimer(POLLMS);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState NexDome::MoveAbs(double az)
{
    return DomeAbsPosNP.s;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState NexDome::Park()
{
    MoveAbs(GetAxis1Park());

    if (HasShutter() && IUFindOnSwitchIndex(&CloseShutterOnParkSP) == ND::ENABLED)
        ControlShutter(ShutterOperation::SHUTTER_CLOSE);

    return IPS_BUSY;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState NexDome::UnPark()
{

    return IPS_OK;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState NexDome::ControlShutter(ShutterOperation operation)
{
    // Check if shutter is open or close.


    switch (operation)
    {
        case SHUTTER_OPEN:

            break;

        case SHUTTER_CLOSE:
            break;
    }

    return IPS_BUSY;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::Abort()
{
    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::SetCurrentPark()
{
    SetAxis1Park(DomeAbsPosN[0].value);
    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::SetDefaultPark()
{
    // default park position is pointed south
    SetAxis1Park(180);
    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::getStartupValues()
{
    return false;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::saveConfigItems(FILE * fp)
{
    INDI::Dome::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &CloseShutterOnParkSP);

    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::setParameter(ND::Commands command, ND::Targets target, int32_t value)
{
    std::ostringstream cmd;
    cmd << "@";
    cmd << ND::CommandsMap.at(command) + "W" + ((target == ND::ROTATOR) ? "R" : "S");
    cmd << ",";
    cmd << value;

    return sendCommand(cmd.str().c_str());
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::getParameter(ND::Commands command, ND::Targets target, std::string value)
{
    char res[ND::DRIVER_LEN] = {0};

    std::string verb = ND::CommandsMap.at(command) + "R" + ((target == ND::ROTATOR) ? "R" : "S");

    std::ostringstream cmd;
    cmd << "@";
    cmd << verb;
    cmd << ",";
    cmd << value;

    if (sendCommand(cmd.str().c_str(), res))
    {
        std::string response(res);
        std::regex re(":" + verb + "(.+)#");
        std::smatch match;
        if (std::regex_match(response, match, re))
        {
            value = match.str(1);
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::checkEvents(std::string &response)
{
    int nbytes_read = 0;
    char res[ND::DRIVER_LEN] = {0};

    int rc = tty_nread_section(PortFD, res, ND::DRIVER_LEN, ND::DRIVER_STOP_CHAR, ND::DRIVER_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
        return false;

    if (nbytes_read < 3)
        return false;

    response = res;
    // Remove ":" and "#"
    response = response.substr(1, response.size() - 1);
    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::processEvent(const std::string &event)
{



}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool NexDome::sendCommand(const char * cmd, char * res, int cmd_len, int res_len)
{
    int nbytes_written = 0, nbytes_read = 0, rc = -1;

    tcflush(PortFD, TCIOFLUSH);

    if (cmd_len > 0)
    {
        char hex_cmd[ND::DRIVER_LEN * 3] = {0};
        hexDump(hex_cmd, cmd, cmd_len);
        LOGF_DEBUG("CMD <%s>", hex_cmd);
        rc = tty_write(PortFD, cmd, cmd_len, &nbytes_written);
    }
    else
    {
        LOGF_DEBUG("CMD <%s>", cmd);
        char cmd_terminated[ND::DRIVER_LEN * 2] = {0};
        snprintf(cmd_terminated, ND::DRIVER_LEN * 2, "%s\r\n", cmd);
        rc = tty_write_string(PortFD, cmd_terminated, &nbytes_written);
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial write error: %s.", errstr);
        return false;
    }

    if (res == nullptr)
        return true;

    if (res_len > 0)
        rc = tty_read(PortFD, res, res_len, ND::DRIVER_TIMEOUT, &nbytes_read);
    else
        rc = tty_nread_section(PortFD, res, ND::DRIVER_LEN, ND::DRIVER_STOP_CHAR, ND::DRIVER_TIMEOUT, &nbytes_read);

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    if (res_len > 0)
    {
        char hex_res[ND::DRIVER_LEN * 3] = {0};
        hexDump(hex_res, res, res_len);
        LOGF_DEBUG("RES <%s>", hex_res);
    }
    else
    {
        res[nbytes_read - 1] = 0;
        LOGF_DEBUG("RES <%s>", res);
    }

    tcflush(PortFD, TCIOFLUSH);

    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
void NexDome::hexDump(char * buf, const char * data, int size)
{
    for (int i = 0; i < size; i++)
        sprintf(buf + 3 * i, "%02X ", static_cast<uint8_t>(data[i]));

    if (size > 0)
        buf[3 * size - 1] = '\0';
}

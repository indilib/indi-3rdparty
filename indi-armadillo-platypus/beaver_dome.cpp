/*
    NexDome Beaver Controller

    Copyright (C) 2021 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "beaver_dome.h"

#include "indicom.h"
#include "connectionplugins/connectiontcp.h"

#include <cmath>
#include <cstring>
#include <cassert>
#include <memory>
#include <regex>

#include <termios.h>
#include <unistd.h>
#include "config.h"

static std::unique_ptr<Beaver> dome(new Beaver());

Beaver::Beaver()
{
    setVersion(LUNATICO_VERSION_MAJOR, LUNATICO_VERSION_MINOR);
    SetDomeCapability(DOME_CAN_ABORT |
                      DOME_CAN_ABS_MOVE |
                      DOME_CAN_REL_MOVE |
                      DOME_CAN_SYNC);
    setDomeConnection(CONNECTION_TCP | CONNECTION_SERIAL);
}

bool Beaver::initProperties()
{
    INDI::Dome::initProperties();

    SetParkDataType(PARK_AZ);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // #1 Calibration
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Rotator
    RotatorCalibrationSP[ROTATOR_HOME_FIND].fill("ROTATOR_HOME_FIND", "Find Home", ISS_OFF);
    RotatorCalibrationSP[ROTATOR_HOME_MEASURE].fill("ROTATOR_HOME_MEASURE", "Measure Home", ISS_OFF);
    RotatorCalibrationSP[ROTATOR_HOME_GOTO].fill("ROTATOR_HOME_GOTO", "Goto Home", ISS_OFF);
    RotatorCalibrationSP.fill(getDefaultName(), "ROTATOR_CALIBRATION", "Rotator", MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60,
                              IPS_IDLE);

    // Shutter
    ShutterCalibrationSP[SHUTTER_HOME_FIND].fill("SHUTTER_HOME_FIND", "Find home", ISS_OFF);
    ShutterCalibrationSP.fill(getDeviceName(), "SHUTTER_CALIBRATION", "Shutter", SHUTTER_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Shutter Settings
    ShutterSettingsNP[SHUTTER_MAX_SPEED].fill("SHUTTER_MAX_SPEED", "Max Speed (m/s)", "%.f", 1, 10, 1, 0);
    ShutterSettingsNP[SHUTTER_MIN_SPEED].fill("SHUTTER_MIN_SPEED", "Min Speed (m/s)", "%.f", 1, 10, 1, 0);
    ShutterSettingsNP[SHUTTER_ACCELERATION].fill("SHUTTER_ACCELERATION", "Acceleration (m/s^2)", "%.f", 1, 10, 1, 0);
    ShutterSettingsNP[SHUTTER_TIMEOUT].fill("SHUTTER_TIMEOUT", "Timeout (s)", "%.f", 1, 10, 1, 0);
    ShutterSettingsNP[SHUTTER_SAFE_VOLTAGE].fill("SHUTTER_SAFE_VOLTAGE", "Safe Voltage", "%.f", 1, 10, 1, 0);
    ShutterSettingsNP.fill(getDeviceName(), "SHUTTER_SETTINGS", "Settings", SHUTTER_TAB, IP_RW, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // #3 Communication & Firmware
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Firmware Version
    FirmwareVersionTP[0].fill("VERSION", "Version", "");
    FirmwareVersionTP.fill(getDeviceName(), "DOME_FIRMWARE", "Firmware", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // #5 Misc.
    ///////////////////////////////////////////////////////////////////////////////////////////////
    //    tcpConnection->setDefaultHost("192.168.1.1");
    //    tcpConnection->setDefaultPort(10000);
    //    tcpConnection->setConnectionType(Connection::TCP::TYPE_UDP);
    //    tty_set_generic_udp_format(1);
    addDebugControl();
    return true;
}

bool Beaver::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        InitPark();

        defineProperty(FirmwareVersionTP);
        defineProperty(RotatorCalibrationSP);
        defineProperty(ShutterCalibrationSP);
        defineProperty(ShutterSettingsNP);
    }
    else
    {
        deleteProperty(FirmwareVersionTP);
        deleteProperty(RotatorCalibrationSP);
        deleteProperty(ShutterCalibrationSP);
        deleteProperty(ShutterSettingsNP);
    }

    return true;
}


//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool Beaver::Handshake()
{
    if (echo())
    {
        // Check if shutter is online
        if (shutterIsUp())
            SetDomeCapability(GetDomeCapability() | DOME_HAS_SHUTTER);

        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
const char *Beaver::getDefaultName()
{
    return  "Beaver Dome";
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool Beaver::echo()
{
    double res = 0;
    if (sendCommand("!seletek tversion#", res))
    {
        char firmwareText[MAXINDILABEL] = {0};
        snprintf(firmwareText, MAXINDILABEL, "%.f", res);
        FirmwareVersionTP[0].setText(firmwareText);
        LOGF_INFO("Detected firmware version %s", firmwareText);
        return true;
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool Beaver::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /////////////////////////////////////////////
        // Rotator Calibration
        /////////////////////////////////////////////
        if (RotatorCalibrationSP.isNameMatch(name))
        {
            RotatorCalibrationSP.update(states, names, n);
            bool rc = false;
            switch (RotatorCalibrationSP.findOnSwitchIndex())
            {
                case ROTATOR_HOME_FIND:
                    rc = rotatorFindHome();
                    break;

                case ROTATOR_HOME_MEASURE:
                    rc = rotatorMeasureHome();
                    break;

                case ROTATOR_HOME_GOTO:
                    rc = rotatorGotoHome();
                    break;
            }

            RotatorCalibrationSP.setState(rc ? IPS_BUSY : IPS_ALERT);
            RotatorCalibrationSP.apply();
            return true;
        }

        /////////////////////////////////////////////
        // Shutter Calibration
        /////////////////////////////////////////////
        if (ShutterCalibrationSP.isNameMatch(name))
        {
            ShutterCalibrationSP.update(states, names, n);
            bool rc = shutterFindHome();
            if (rc)
                setShutterState(SHUTTER_MOVING);
            ShutterCalibrationSP.setState(rc ? IPS_BUSY : IPS_ALERT);
            ShutterCalibrationSP.apply();
            return true;
        }
    }

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool Beaver::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /////////////////////////////////////////////
        // Shutter Settings
        /////////////////////////////////////////////
        if (ShutterSettingsNP.isNameMatch(name))
        {
            ShutterSettingsNP.update(values, names, n);
            ShutterSettingsNP.setState(shutterSetSettings(ShutterSettingsNP[SHUTTER_MAX_SPEED].getValue(),
                                       ShutterSettingsNP[SHUTTER_MIN_SPEED].getValue(),
                                       ShutterSettingsNP[SHUTTER_ACCELERATION].getValue(),
                                       ShutterSettingsNP[SHUTTER_TIMEOUT].getValue(),
                                       ShutterSettingsNP[SHUTTER_SAFE_VOLTAGE].getValue()) ? IPS_OK : IPS_ALERT);
            ShutterCalibrationSP.apply();
            return true;
        }
    }

    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

///////////////////////////////////////////////////////////////////////////
/// Read position and update accordingly
///////////////////////////////////////////////////////////////////////////
void Beaver::TimerHit()
{
    if (!isConnected())
        return;

    // Query staus
    rotatorGetStatus();
    shutterGetStatus();

    // Get Position
    rotatorGetAz();

    // Check rotator
    if (getDomeState() == DOME_MOVING || getDomeState() == DOME_PARKING || getDomeState() == DOME_UNPARKING)
    {
        // TODO
    }

    // Check shutter
    if (getShutterState() == SHUTTER_MOVING)
    {
        // TODO
    }

    SetTimer(getCurrentPollingPeriod());
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState Beaver::MoveAbs(double az)
{
    if (rotatorGotoAz(az))
    {
        m_TargetRotatorAz = az;
        return IPS_BUSY;
    }

    return IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState Beaver::MoveRel(double azDiff)
{
    m_TargetRotatorAz = DomeAbsPosN[0].value + azDiff;

    if (m_TargetRotatorAz < DomeAbsPosN[0].min)
        m_TargetRotatorAz += DomeAbsPosN[0].max;
    if (m_TargetRotatorAz > DomeAbsPosN[0].max)
        m_TargetRotatorAz -= DomeAbsPosN[0].max;

    // It will take a few cycles to reach final position
    return MoveAbs(m_TargetRotatorAz);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool Beaver::Sync(double az)
{
    return rotatorSyncAZ(az);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState Beaver::ControlShutter(ShutterOperation operation)
{
    double res = 0;
    if (operation == SHUTTER_OPEN)
    {
        if (sendCommand("!dome openshutter#", res))
            return IPS_BUSY;
    }
    else if (operation == SHUTTER_CLOSE)
    {
        if (sendCommand("!dome closeshutter#", res))
            return IPS_BUSY;
    }
    return IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool Beaver::Abort()
{
    return abortAll();
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState Beaver::Park()
{
    return rotatorGotoPark() ? IPS_BUSY : IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState Beaver::UnPark()
{
    return IPS_OK;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool Beaver::saveConfigItems(FILE *fp)
{
    INDI::Dome::saveConfigItems(fp);
    ShutterSettingsNP.save(fp);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorGotoAz(double az)
{
    char cmd[DRIVER_LEN] = {0};
    double res = 0;
    snprintf(cmd, DRIVER_LEN, "!dome gotoaz %.2f#", az);
    return sendCommand(cmd, res);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorGetAz()
{
    double res = 0;
    if (sendCommand("!dome getaz#", res))
    {
        DomeAbsPosN[0].value = res;
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorSyncAZ(double az)
{
    char cmd[DRIVER_LEN] = {0};
    double res = 0;
    snprintf(cmd, DRIVER_LEN, "!dome setaz %.2f#", az);
    return sendCommand(cmd, res);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorGotoPark()
{
    double res = 0;
    return sendCommand("!dome gopark#", res);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorGotoHome()
{
    double res = 0;
    return sendCommand("!dome gohome#", res);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorMeasureHome()
{
    double res = 0;
    return sendCommand("!dome autocalrot 1#", res);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorFindHome()
{
    double res = 0;
    return sendCommand("!dome autocalrot 0#", res);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorIsHome()
{
    double res = 0;
    if (sendCommand("!dome athome#", res))
    {
        return res == 1;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorIsParked()
{
    double res = 0;
    if (sendCommand("!dome atpark#", res))
    {
        return res == 1;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::rotatorGetStatus()
{
    // TODO
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::shutterGetStatus()
{
    // TODO
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::shutterIsUp()
{
    double res = 0;
    if (sendCommand("!dome shutterisup#", res))
    {
        return res == 1;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::abortAll()
{
    double res = 0;
    return sendCommand("!dome abort 1 1 1#", res);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::shutterAbort()
{
    double res = 0;
    return sendCommand("!dome abort 0 0 1#", res);
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::shutterSetSettings(uint32_t maxSpeed, uint32_t minSpeed, uint32_t acceleration, uint32_t timeout,
                                uint32_t voltage)
{
    // TODO
    INDI_UNUSED(maxSpeed);
    INDI_UNUSED(minSpeed);
    INDI_UNUSED(acceleration);
    INDI_UNUSED(timeout);
    INDI_UNUSED(voltage);
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::shutterGetSettings()
{
    // TODO
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/////////////////////////////////////////////////////////////////////////////
bool Beaver::shutterFindHome()
{
    double res = 0;
    return sendCommand("!dome autocalshutter#", res);
}


/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool Beaver::sendCommand(const char * cmd, double &res)
{
    int rc = TTY_OK;
    for (int i = 0; i < 3; i++)
    {
        int nbytes_written = 0, nbytes_read = 0;
        char response[DRIVER_LEN] = {0};

        LOGF_DEBUG("CMD <%s>", cmd);

        rc = tty_write_string(PortFD, cmd, &nbytes_written);

        if (rc != TTY_OK)
        {
            char errstr[MAXRBUF] = {0};
            tty_error_msg(rc, errstr, MAXRBUF);
            LOGF_ERROR("Serial write error: %s.", errstr);
            return false;
        }

        rc = tty_nread_section(PortFD, response, DRIVER_LEN, DRIVER_STOP_CHAR, DRIVER_TIMEOUT, &nbytes_read);

        if (rc != TTY_OK)
        {
            usleep(100000);
            continue;
        }

        // Remove extra #
        response[nbytes_read - 1] = 0;
        LOGF_DEBUG("RES <%s>", response);


        std::regex rgx(R"(.*:(\d+))");
        std::smatch match;
        std::string input(response);

        if (std::regex_search(input, match, rgx))
        {
            try
            {
                res = std::stof(match.str(1));
                return true;
            }
            catch (...)
            {
                LOGF_ERROR("Failed to process response: %s.", response);
                return false;
            }
        }
    }

    if (rc != TTY_OK)
    {
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
    }

    return false;
}

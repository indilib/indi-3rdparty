/*
    Seletek Rotator

    Copyright (C) 2020 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "seletek_rotator.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <regex>

#include <termios.h>
#include <unistd.h>
#include "config.h"

static std::unique_ptr<SeletekRotator> rotator(new SeletekRotator());

void ISGetProperties(const char *dev)
{
    rotator->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    rotator->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    rotator->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    rotator->ISNewNumber(dev, name, values, names, n);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
               char *names[], int n)
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

void ISSnoopDevice(XMLEle *root)
{
    rotator->ISSnoopDevice(root);
}

SeletekRotator::SeletekRotator()
{
    setVersion(LUNATICO_VERSION_MAJOR, LUNATICO_VERSION_MINOR);
    RI::SetCapability(ROTATOR_CAN_ABORT |
                      ROTATOR_CAN_SYNC  |
                      ROTATOR_CAN_REVERSE |
                      ROTATOR_HAS_BACKLASH);
}

bool SeletekRotator::initProperties()
{
    INDI::Rotator::initProperties();

    // Peripheral Port
    IUFillSwitch(&PerPortS[PORT_MAIN], "PORT_MAIN", "Main", ISS_ON );
    IUFillSwitch(&PerPortS[PORT_EXP], "PORT_EXP", "Exp", ISS_OFF );
    IUFillSwitch(&PerPortS[PORT_THIRD], "PORT_THIRD", "Third", ISS_OFF );
    IUFillSwitchVector(&PerPortSP, PerPortS, 3, getDeviceName(), "SELETEK_PORT", "Port", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    // HalfStep
    IUFillSwitch(&HalfStepS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&HalfStepS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&HalfStepSP, HalfStepS, 2, getDeviceName(), "ROTATOR_HALF_STEP", "Half Step", SETTINGS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // Wiring
    IUFillSwitch(&WiringS[WIRING_LUNATICO_NORMAL], "WIRING_LUNATICO_NORMAL", "Lunatico Normal", ISS_ON);
    IUFillSwitch(&WiringS[WIRING_LUNATICO_REVERSED], "WIRING_LUNATICO_REVERSED", "Lunatico Reverse", ISS_OFF);
    IUFillSwitch(&WiringS[WIRING_RFMOONLITE_NORMAL], "WIRING_RFMOONLITE_NORMAL", "RF/Moonlite Normal", ISS_OFF);
    IUFillSwitch(&WiringS[WIRING_RFMOONLITE_REVERSED], "WIRING_RFMOONLITE_REVERSED", "RF/Moonlite Reverse", ISS_OFF);
    IUFillSwitchVector(&WiringSP, WiringS, 4, getDeviceName(), "ROTATOR_WIRING", "Wiring", SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0,
                       IPS_IDLE);

    // Max Speed
    // our internal speed is in usec/step, with a reasonable range from 500.000.usec for dc motors simulating steps to
    // 50 usec optimistic speed for very small steppers
    // So our range is 10.000.-
    // and the conversion is usec/step = 500000 - ((INDISpeed - 1) * 50)
    // with our default and standard 10.000usec being 9800 (9801 actually)
    IUFillNumber(&SettingN[PARAM_MIN_SPEED], "PARAM_MIN_SPEED", "Min Speed", "%.f", 1., 10000., 100., 9800.);
    IUFillNumber(&SettingN[PARAM_MAX_SPEED], "PARAM_MAX_SPEED", "Max Speed", "%.f", 1., 10000., 100., 9800.);
    IUFillNumber(&SettingN[PARAM_MIN_LIMIT], "PARAM_MIN_LIMIT", "Min Limit", "%.2f", -180., -90., 10., -179.5);
    IUFillNumber(&SettingN[PARAM_MAX_LIMIT], "PARAM_MAX_LIMIT", "Max Limit", "%.2f", 90, 180., 10., 179.5);
    IUFillNumber(&SettingN[PARAM_STEPS_DEGREE], "PARAM_STEPS_DEGREE", "Steps/Degree", "%.2f", 1., 10000., 500., 1000.);
    IUFillNumberVector(&SettingNP, SettingN, 5, getDeviceName(), "ROTATOR_SETTINGS", "Parameters", SETTINGS_TAB, IP_RW, 0,
                       IPS_OK);

    // Motor Types
    IUFillSwitch(&MotorTypeS[MOTOR_UNIPOLAR], "MOTOR_UNIPOLAR", "Unipolar", ISS_ON);
    IUFillSwitch(&MotorTypeS[MOTOR_BIPOLAR], "MOTOR_BIPOLAR", "Bipolar", ISS_OFF);
    IUFillSwitch(&MotorTypeS[MOTOR_DC], "MOTOR_DC", "DC", ISS_OFF);
    IUFillSwitch(&MotorTypeS[MOTOR_STEPDIR], "MOTOR_STEPDIR", "Step-Dir", ISS_OFF);
    IUFillSwitchVector(&MotorTypeSP, MotorTypeS, 4, getDeviceName(), "ROTATOR_MOTOR_TYPE", "Motor Type", SETTINGS_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // Firmware Version
    IUFillText(&FirmwareVersionT[0], "VERSION", "Version", "");
    IUFillTextVector(&FirmwareVersionTP, FirmwareVersionT, 1, getDeviceName(), "ROTATOR_FIRMWARE", "Firmware", MAIN_CONTROL_TAB,
                     IP_RO, 0, IPS_IDLE);

    // Rotator Ticks
    IUFillNumber(&RotatorAbsPosN[0], "ROTATOR_ABSOLUTE_POSITION", "Value", "%.f", 0., 1000000., 0., 0.);
    IUFillNumberVector(&RotatorAbsPosNP, RotatorAbsPosN, 1, getDeviceName(), "ABS_ROTATOR_POSITION", "Steps", MAIN_CONTROL_TAB,
                       IP_RW, 0, IPS_IDLE );

    addDebugControl();

    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    return true;
}

void SeletekRotator::ISGetProperties(const char *dev)
{
    INDI::Rotator::ISGetProperties(dev);

    defineProperty(&PerPortSP);
    loadConfig(true, PerPortSP.name);
}

bool SeletekRotator::updateProperties()
{
    INDI::Rotator::updateProperties();

    if (isConnected())
    {
        getParam("getpos", m_ZeroPosition);

        defineProperty(&FirmwareVersionTP);
        defineProperty(&RotatorAbsPosNP);
        defineProperty(&SettingNP);
        defineProperty(&MotorTypeSP);
        defineProperty(&HalfStepSP);
        defineProperty(&WiringSP);
    }
    else
    {
        deleteProperty(FirmwareVersionTP.name);
        deleteProperty(RotatorAbsPosNP.name);
        deleteProperty(SettingNP.name);
        deleteProperty(MotorTypeSP.name);
        deleteProperty(HalfStepSP.name);
        deleteProperty(WiringSP.name);
    }

    return true;
}

bool SeletekRotator::Handshake()
{
    return echo();
}

const char *SeletekRotator::getDefaultName()
{
    return  "Seletek Rotator";
}

bool SeletekRotator::echo()
{
    int32_t res = 0;

    char cmd[DRIVER_LEN] = {0};

    snprintf( cmd, DRIVER_LEN, "!seletek version#" );

    if ( sendCommand(cmd, res) )
    {
        const char *operative[ DRIVER_OPERATIVES + 1 ] = { "", "Bootloader", "Error" };
        const char *models[ DRIVER_MODELS + 1 ] = { "Error", "Seletek", "Armadillo", "Platypus", "Dragonfly" };
        int fwmaj, fwmin, model, oper;
        char txt[ DRIVER_LEN ] = {0};

        oper = res / 10000;		// 0 normal, 1 bootloader
        model = ( res / 1000 ) % 10;	// 1 seletek, etc.
        fwmaj = ( res / 100 ) % 10;
        fwmin = ( res % 100 );
        if ( oper >= DRIVER_OPERATIVES )
            oper = DRIVER_OPERATIVES;
        if ( model > DRIVER_MODELS )
            model = 0;
        sprintf( txt, "%s %s fwv %d.%d", operative[ oper ], models[ model ], fwmaj, fwmin );
        IUSaveText( &FirmwareVersionT[0], txt );
        LOGF_INFO("Setting version to [%s]", txt );

        return true;
    }
    return false;
}

bool SeletekRotator::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /////////////////////////////////////////////
        // Peripheral Port
        /////////////////////////////////////////////
        if (!strcmp(name, PerPortSP.name))
        {
            IUUpdateSwitch(&PerPortSP, states, names, n);
            if (isConnected())
                syncSettings();
            PerPortSP.s = IPS_OK;
            IDSetSwitch(&PerPortSP, nullptr);
            saveConfig(true, PerPortSP.name);
            return true;
        }
        /////////////////////////////////////////////
        // Halfstep
        /////////////////////////////////////////////
        else if (!strcmp(name, HalfStepSP.name))
        {
            if (setParam("halfstep", !strcmp(IUFindOnSwitchName(states, names, n), HalfStepS[INDI_ENABLED].name) ? 1 : 0))
            {
                IUUpdateSwitch(&HalfStepSP, states, names, n);
                HalfStepSP.s = IPS_OK;
            }
            else
                HalfStepSP.s = IPS_ALERT;

            IDSetSwitch(&HalfStepSP, nullptr);
            return true;
        }
        /////////////////////////////////////////////
        // Wiring
        /////////////////////////////////////////////
        else if (!strcmp(name, WiringSP.name))
        {
            uint32_t prevWireMode = IUFindOnSwitchIndex(&WiringSP);
            IUUpdateSwitch(&WiringSP, states, names, n);
            if (setParam("wiremode", IUFindOnSwitchIndex(&WiringSP)))
            {
                WiringSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&WiringSP);
                WiringS[prevWireMode].s = ISS_ON;
                WiringSP.s = IPS_ALERT;
            }

            IDSetSwitch(&WiringSP, nullptr);
            return true;
        }
        /////////////////////////////////////////////
        // Motor Type
        /////////////////////////////////////////////
        else if (!strcmp(name, MotorTypeSP.name))
        {
            uint32_t prevModel = IUFindOnSwitchIndex(&MotorTypeSP);
            IUUpdateSwitch(&MotorTypeSP, states, names, n);
            if (setParam("model", IUFindOnSwitchIndex(&MotorTypeSP)))
            {
                MotorTypeSP.s = IPS_OK;
            }
            else
            {
                IUResetSwitch(&MotorTypeSP);
                MotorTypeS[prevModel].s = ISS_ON;
                MotorTypeSP.s = IPS_ALERT;
            }

            IDSetSwitch(&MotorTypeSP, nullptr);
            return true;
        }
    }

    return INDI::Rotator::ISNewSwitch(dev, name, states, names, n);
}

bool SeletekRotator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /////////////////////////////////////////////
        // Settings
        /////////////////////////////////////////////
        if (!strcmp(name, SettingNP.name))
        {
            bool rc = true;
            std::vector<double> prevValue(SettingNP.nnp);
            for (uint8_t i = 0; i < SettingNP.nnp; i++)
                prevValue[i] = SettingN[i].value;
            IUUpdateNumber(&SettingNP, values, names, n);

            if (SettingN[PARAM_MIN_SPEED].value != prevValue[PARAM_MIN_SPEED] ||
                    SettingN[PARAM_MAX_SPEED].value != prevValue[PARAM_MAX_SPEED])
            {
                rc = setSpeedRange(SettingN[PARAM_MIN_SPEED].value, SettingN[PARAM_MAX_SPEED].value);
            }

            if (SettingN[PARAM_STEPS_DEGREE].value != prevValue[PARAM_STEPS_DEGREE])
            {
                GotoRotatorN[0].value = calculateAngle(RotatorAbsPosN[0].value);
                IDSetNumber(&GotoRotatorNP, nullptr);
            }

            if (!rc)
            {
                for (uint8_t i = 0; i < SettingNP.nnp; i++)
                    SettingN[i].value = prevValue[i];
            }

            SettingNP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetNumber(&SettingNP, nullptr);
            return true;
        }
        /////////////////////////////////////////////
        // Steps
        /////////////////////////////////////////////
        else if (!strcmp(name, RotatorAbsPosNP.name))
        {
            int target = values[0];
            if (gotoTarget(target))
            {
                RotatorAbsPosNP.s = IPS_BUSY;
                GotoRotatorNP.s = IPS_BUSY;
                LOGF_INFO("Moving to %d steps.", target);
                IDSetNumber(&GotoRotatorNP, nullptr);
            }
            else
            {
                RotatorAbsPosNP.s = IPS_ALERT;
            }

            IDSetNumber(&RotatorAbsPosNP, nullptr);
            return true;
        }
    }

    return INDI::Rotator::ISNewNumber(dev, name, values, names, n);
}

IPState SeletekRotator::MoveRotator(double angle)
{
    //    // Clamp to range
    //    double min = range360(SettingN[PARAM_MIN_LIMIT].value);
    //    double max = range360(SettingN[PARAM_MAX_LIMIT].value);
    //    // Clamp to range
    //    angle = std::max(min, std::min(max, angle));

    // Find closest distance
    double r = (angle > 180) ? 360 - angle : angle;
    int sign = (angle >= 0 && angle <= 180) ? 1 : -1;

    r *= sign;
    r *= IUFindOnSwitchIndex(&ReverseRotatorSP) == INDI_ENABLED ? -1 : 1;

    double newTarget = r * SettingN[PARAM_STEPS_DEGREE].value + m_ZeroPosition;

    return gotoTarget(newTarget) ? IPS_BUSY : IPS_ALERT;
}

///////////////////////////////////////////////////////////////////////////
/// Goto target
///////////////////////////////////////////////////////////////////////////
bool SeletekRotator::SyncRotator(double angle)
{
    //    double min = range360(SettingN[PARAM_MIN_LIMIT].value);
    //    double max = range360(SettingN[PARAM_MAX_LIMIT].value);
    //    // Clamp to range
    //    angle = std::max(min, std::min(max, angle));

    // Find closest distance
    double r = (angle > 180) ? 360 - angle : angle;
    int sign = (angle >= 0 && angle <= 180) ? 1 : -1;

    r *= sign;
    r *= IUFindOnSwitchIndex(&ReverseRotatorSP) == INDI_ENABLED ? -1 : 1;
    double newTarget = r * SettingN[PARAM_STEPS_DEGREE].value + m_ZeroPosition;

    return setParam("setpos", newTarget);
}

///////////////////////////////////////////////////////////////////////////
/// Goto target
///////////////////////////////////////////////////////////////////////////
bool SeletekRotator::gotoTarget(uint32_t position)
{
    char cmd[DRIVER_LEN] = {0};
    int32_t res = 0;
    uint32_t backlash = (IUFindOnSwitchIndex(&RotatorBacklashSP) == INDI_ENABLED) ? static_cast<uint32_t>
                        (RotatorBacklashN[0].value) : 0;
    snprintf(cmd, DRIVER_LEN, "!step goto %d %u %u#", IUFindOnSwitchIndex(&PerPortSP), position, backlash);
    if (sendCommand(cmd, res))
        m_IsMoving = (res == 0);
    else
        m_IsMoving = false;

    return m_IsMoving;
}

///////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////
bool SeletekRotator::setParam(const std::string &param, uint32_t value)
{
    char cmd[DRIVER_LEN] = {0};
    int32_t res = 0;
    snprintf(cmd, DRIVER_LEN, "!step %s %d %u#", param.c_str(), IUFindOnSwitchIndex(&PerPortSP), value);
    if (sendCommand(cmd, res))
        return res == 0;

    return false;
}

///////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////
bool SeletekRotator::getParam(const std::string &param, uint32_t &value)
{
    char cmd[DRIVER_LEN] = {0};
    int32_t res = 0;
    snprintf(cmd, DRIVER_LEN, "!step %s %d#", param.c_str(), IUFindOnSwitchIndex(&PerPortSP));
    if (sendCommand(cmd, res))
    {
        value = res;
        return true;
    }

    return false;
}

///////////////////////////////////////////////////////////////////////////
/// Set speed range in usecs
///////////////////////////////////////////////////////////////////////////
bool SeletekRotator::setSpeedRange(uint32_t min, uint32_t max)
{
    char cmd[DRIVER_LEN] = {0};
    int32_t res = 0;

    int min_usec = min > 0 ? (500000 - (min - 1) * 50) : 0;
    int max_usec = max > 0 ? (500000 - (max - 1) * 50) : 0;
    snprintf(cmd, DRIVER_LEN, "!step speedrangeus %d %d %d#", IUFindOnSwitchIndex(&PerPortSP), min_usec, max_usec);
    if (sendCommand(cmd, res))
        return (res == 0);

    return false;
}

///////////////////////////////////////////////////////////////////////////
/// Sync all settings in case of port change
///////////////////////////////////////////////////////////////////////////

bool SeletekRotator::syncSettings()
{
    setParam("halfstep", IUFindOnSwitchIndex(&HalfStepSP) == INDI_ENABLED ? 1 : 0);
    setParam("wiremode", IUFindOnSwitchIndex(&WiringSP));
    setParam("model", IUFindOnSwitchIndex(&MotorTypeSP));
    setSpeedRange(SettingN[PARAM_MIN_SPEED].value, SettingN[PARAM_MAX_SPEED].value);
    return true;
}

///////////////////////////////////////////////////////////////////////////
/// Set Backlash
///////////////////////////////////////////////////////////////////////////
bool SeletekRotator::SetRotatorBacklash(int32_t steps)
{
    INDI_UNUSED(steps);
    return true;
}

///////////////////////////////////////////////////////////////////////////
/// Enable/Disable backlash
///////////////////////////////////////////////////////////////////////////

bool SeletekRotator::SetRotatorBacklashEnabled(bool enabled)
{
    INDI_UNUSED(enabled);
    return true;
}

///////////////////////////////////////////////////////////////////////////
/// Read position and update accordingly
///////////////////////////////////////////////////////////////////////////
void SeletekRotator::TimerHit()
{
    if (!isConnected())
        return;

    uint32_t res {0};

    if (getParam("getpos", res))
    {
        if (fabs(res - RotatorAbsPosN[0].value) > 0)
        {
            RotatorAbsPosN[0].value = res;
            IDSetNumber(&RotatorAbsPosNP, nullptr);
        }
        else
            m_IsMoving = false;

        double newPosition = calculateAngle(res);

        if (fabs(GotoRotatorN[0].value - newPosition) > 0)
        {
            GotoRotatorN[0].value = newPosition;
            IDSetNumber(&GotoRotatorNP, nullptr);
        }
    }

    if (m_IsMoving == false && (GotoRotatorNP.s == IPS_BUSY || RotatorAbsPosNP.s == IPS_BUSY))
    {
        if (GotoRotatorNP.s == IPS_BUSY)
        {
            GotoRotatorNP.s = IPS_OK;
            IDSetNumber(&GotoRotatorNP, nullptr);
        }

        if (RotatorAbsPosNP.s == IPS_BUSY)
        {
            RotatorAbsPosNP.s = IPS_OK;
            IDSetNumber(&RotatorAbsPosNP, nullptr);
        }
    }

    SetTimer(getCurrentPollingPeriod());
}

/////////////////////////////////////////////////////////////////////////////
/// Stop motion
/////////////////////////////////////////////////////////////////////////////
bool SeletekRotator::AbortRotator()
{
    char cmd[DRIVER_LEN] = {0};
    bool rc = false;
    int32_t res = 0;
    snprintf(cmd, DRIVER_LEN, "!step stop %d#", IUFindOnSwitchIndex(&PerPortSP));
    if (sendCommand(cmd, res))
        rc = (res == 0);

    if (rc && RotatorAbsPosNP.s == IPS_BUSY)
    {
        RotatorAbsPosNP.s = IPS_IDLE;
        IDSetNumber(&RotatorAbsPosNP, nullptr);
    }

    return rc;
}

double SeletekRotator::calculateAngle(uint32_t steps)
{
    int diff = (static_cast<int32_t>(steps) - m_ZeroPosition) *
               (IUFindOnSwitchIndex(&ReverseRotatorSP) == INDI_ENABLED ? -1 : 1);
    return range360(diff / SettingN[PARAM_STEPS_DEGREE].value);
}

/////////////////////////////////////////////////////////////////////////////
/// Save in configuration file
/////////////////////////////////////////////////////////////////////////////
bool SeletekRotator::saveConfigItems(FILE *fp)
{
    INDI::Rotator::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &PerPortSP);
    IUSaveConfigSwitch(fp, &MotorTypeSP);
    IUSaveConfigSwitch(fp, &WiringSP);
    IUSaveConfigNumber(fp, &SettingNP);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Reverse
/////////////////////////////////////////////////////////////////////////////
bool SeletekRotator::ReverseRotator(bool enabled)
{
    INDI_UNUSED(enabled);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool SeletekRotator::sendCommand(const char * cmd, int32_t &res)
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
            res = std::stoi(match.str(1));
            return true;
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


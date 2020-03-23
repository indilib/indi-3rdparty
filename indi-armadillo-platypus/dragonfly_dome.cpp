/*
    DragonFly Dome

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

#include "dragonfly_dome.h"

#include "indicom.h"
#include "connectionplugins/connectionserial.h"

#include <cmath>
#include <cstring>
#include <memory>
#include <regex>

#include <termios.h>
#include <unistd.h>
#include "config.h"

static std::unique_ptr<DragonFlyDome> dome(new DragonFlyDome());

void ISGetProperties(const char *dev)
{
    dome->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    dome->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    dome->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    dome->ISNewNumber(dev, name, values, names, n);
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
    dome->ISSnoopDevice(root);
}

Relay::Relay(uint8_t id, const std::string &device, const std::string &group)
{
    m_ID = id;
    char name[MAXINDINAME] = {0}, label[MAXINDILABEL] = {0};

    snprintf(name, MAXINDINAME, "RELAY_%d", id);
    m_Name = name;
    snprintf(label, MAXINDILABEL, "Relay #%d", id);

    IUFillSwitch(&RelayS[DD::INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&RelayS[DD::INDI_DISABLED], "INDI_DISABLED", "Off", ISS_ON);
    IUFillSwitchVector(&RelaySP, RelayS, 2, device.c_str(), name, label, group.c_str(), IP_RW, ISR_1OFMANY, 60, IPS_IDLE);
}

void Relay::define(DD *parent)
{
    parent->defineSwitch(&RelaySP);
}

void Relay::remove(DD *parent)
{
    parent->deleteProperty(RelaySP.name);
}

bool Relay::update(ISState *states, char *names[], int n)
{
    return IUUpdateSwitch(&RelaySP, states, names, n) == 0;
}

bool Relay::isEnabled() const
{
    return IUFindOnSwitchIndex(&RelaySP) == DD::INDI_ENABLED;
}

void Relay::sync(IPState state)
{
    RelaySP.s = state;
    IDSetSwitch(&RelaySP, nullptr);
}

const std::string &Relay::name() const
{
    return m_Name;
}

DragonFlyDome::DragonFlyDome()
{
    setVersion(LUNATICO_VERSION_MAJOR, LUNATICO_VERSION_MINOR);
    SetDomeCapability(DOME_CAN_REL_MOVE | DOME_CAN_ABORT |  DOME_CAN_PARK);
}

bool DragonFlyDome::initProperties()
{
    INDI::Dome::initProperties();

    // Peripheral Port
    IUFillSwitch(&PerPortS[PORT_MAIN], "PORT_MAIN", "Main", ISS_ON );
    IUFillSwitch(&PerPortS[PORT_EXP], "PORT_EXP", "Exp", ISS_OFF );
    IUFillSwitch(&PerPortS[PORT_THIRD], "PORT_THIRD", "Third", ISS_OFF );
    IUFillSwitchVector(&PerPortSP, PerPortS, 3, getDeviceName(), "SELETEK_PORT", "Port", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // HalfStep
    IUFillSwitch(&HalfStepS[INDI_ENABLED], "INDI_ENABLED", "On", ISS_OFF);
    IUFillSwitch(&HalfStepS[INDI_DISABLED], "INDI_DISABLED", "Off", ISS_OFF);
    IUFillSwitchVector(&HalfStepSP, HalfStepS, 2, getDeviceName(), "ROTATOR_HALF_STEP", "Half Step", SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Wiring
    IUFillSwitch(&WiringS[WIRING_LUNATICO_NORMAL], "WIRING_LUNATICO_NORMAL", "Lunatico Normal", ISS_ON);
    IUFillSwitch(&WiringS[WIRING_LUNATICO_REVERSED], "WIRING_LUNATICO_REVERSED", "Lunatico Reverse", ISS_OFF);
    IUFillSwitch(&WiringS[WIRING_RFMOONLITE_NORMAL], "WIRING_RFMOONLITE_NORMAL", "RF/Moonlite Normal", ISS_OFF);
    IUFillSwitch(&WiringS[WIRING_RFMOONLITE_REVERSED], "WIRING_RFMOONLITE_REVERSED", "RF/Moonlite Reverse", ISS_OFF);
    IUFillSwitchVector(&WiringSP, WiringS, 4, getDeviceName(), "ROTATOR_WIRING", "Wiring", SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Max Speed
    // our internal speed is in usec/step, with a reasonable range from 500.000.usec for dc motors simulating steps to
    // 50 usec optimistic speed for very small steppers
    // So our range is 10.000.-
    // and the conversion is usec/step = 500000 - ((INDISpeed - 1) * 50)
    // with our default and standard 10.000usec being 9800 (9801 actually)
    IUFillNumber(&SettingN[PARAM_MIN_SPEED], "PARAM_MIN_SPEED", "Min Speed", "%.f", 1., 10000., 100., 9800.);
    IUFillNumber(&SettingN[PARAM_MAX_SPEED], "PARAM_MAX_SPEED", "Max Speed", "%.f", 1., 10000., 100., 9800.);
    IUFillNumber(&SettingN[PARAM_MIN_LIMIT], "PARAM_MIN_LIMIT", "Min Limit", "%.f", 0., 100000., 100., 0.);
    IUFillNumber(&SettingN[PARAM_MAX_LIMIT], "PARAM_MAX_LIMIT", "Max Limit", "%.f", 100., 100000., 100., 100000.);
    IUFillNumber(&SettingN[PARAM_STEPS_DEGREE], "PARAM_STEPS_DEGREE", "Steps/Degree", "%.f", 100., 100000., 100., 1000.);
    IUFillNumberVector(&SettingNP, SettingN, 5, getDeviceName(), "ROTATOR_SETTINGS", "Parameters", SETTINGS_TAB, IP_RW, 0, IPS_OK);

    // Motor Types
    IUFillSwitch(&MotorTypeS[MOTOR_UNIPOLAR], "MOTOR_UNIPOLAR", "Unipolar", ISS_ON);
    IUFillSwitch(&MotorTypeS[MOTOR_BIPOLAR], "MOTOR_BIPOLAR", "Bipolar", ISS_OFF);
    IUFillSwitch(&MotorTypeS[MOTOR_DC], "MOTOR_DC", "DC", ISS_OFF);
    IUFillSwitch(&MotorTypeS[MOTOR_STEPDIR], "MOTOR_STEPDIR", "Step-Dir", ISS_OFF);
    IUFillSwitchVector(&MotorTypeSP, MotorTypeS, 4, getDeviceName(), "ROTATOR_MOTOR_TYPE", "Motor Type", SETTINGS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Firmware Version
    IUFillText(&FirmwareVersionT[0], "VERSION", "Version", "");
    IUFillTextVector(&FirmwareVersionTP, FirmwareVersionT, 1, getDeviceName(), "ROTATOR_FIRMWARE", "Firmware", MAIN_CONTROL_TAB, IP_RO, 0, IPS_IDLE);

    // Relays
    for (uint8_t i = 0; i < 8; i++)
    {
        std::unique_ptr<Relay> oneRelay;
        oneRelay.reset(new Relay(i + i, getDeviceName(), RELAYS_TAB));
        Relays.push_back(std::move(oneRelay));
    }

    addDebugControl();

    serialConnection->setDefaultBaudRate(Connection::Serial::B_115200);

    return true;
}

void DragonFlyDome::ISGetProperties(const char *dev)
{
    INDI::Dome::ISGetProperties(dev);

    defineSwitch(&PerPortSP);
    loadConfig(true, PerPortSP.name);
}

bool DragonFlyDome::updateProperties()
{
    INDI::Dome::updateProperties();

    if (isConnected())
    {
        defineText(&FirmwareVersionTP);
        defineNumber(&SettingNP);
        defineSwitch(&MotorTypeSP);
        defineSwitch(&HalfStepSP);
        defineSwitch(&WiringSP);
        for (auto &oneRelay : Relays)
            oneRelay->define(this);
    }
    else
    {
        deleteProperty(FirmwareVersionTP.name);
        deleteProperty(SettingNP.name);
        deleteProperty(MotorTypeSP.name);
        deleteProperty(HalfStepSP.name);
        deleteProperty(WiringSP.name);
        for (auto &oneRelay : Relays)
            oneRelay->remove(this);
    }

    return true;
}

bool DragonFlyDome::Handshake()
{
    LOG_INFO("Error communicating with the DragonFly Dome. Please ensure it is powered and the port is correct.");
    return false;
}

const char *DragonFlyDome::getDefaultName()
{
    return  "DragonFly Dome";
}

bool DragonFlyDome::echo()
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
        if ( model >= DRIVER_MODELS )
            model = 0;
        sprintf( txt, "%s %s fwv %d.%d", operative[ oper ], models[ model ], fwmaj, fwmin );
        if ( strcmp( models[ model ], "Dragonfly" ) )
            LOGF_WARN("Detected model is %s while Dragonfly model is expected. This may lead to limited operability", models[model] );

        IUSaveText( &FirmwareVersionT[0], txt );
        LOGF_INFO("Setting version to [%s]", txt );

        return true;
    }
    return false;
}

bool DragonFlyDome::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /////////////////////////////////////////////
        // Peripheral Port
        /////////////////////////////////////////////
        if (!strcmp(name, PerPortSP.name))
        {
            IUUpdateSwitch(&PerPortSP, states, names, n);
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
        /////////////////////////////////////////////
        // Motor Type
        /////////////////////////////////////////////
        for (uint8_t i = 0; i < 8; i++)
        {
            if (!strcmp(name, Relays[i]->name().c_str()))
            {
                bool enabled = !strcmp(IUFindOnSwitchName(states, names, n), "INDI_ENABLED");
                if (setRelayEnabled(i, enabled))
                {
                    Relays[i]->update(states, names, n);
                    Relays[i]->sync(IPS_OK);
                }
                else
                {
                    Relays[i]->sync(IPS_ALERT);
                }

                return true;
            }
        }
    }

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

bool DragonFlyDome::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /////////////////////////////////////////////
        // Settings
        /////////////////////////////////////////////
        if (strcmp(name, SettingNP.name) == 0)
        {
            bool rc = false;
            std::vector<double> prevValue(SettingNP.nnp);
            for (int i = 0; i < SettingNP.nnp; i++)
                prevValue[i] = SettingN[i].value;
            IUUpdateNumber(&SettingNP, values, names, n);

            if (std::fabs(SettingN[PARAM_MIN_SPEED].value - prevValue[PARAM_MIN_SPEED]) > 0 ||
                    std::fabs(SettingN[PARAM_MAX_SPEED].value - prevValue[PARAM_MAX_SPEED]) > 0)
            {
                rc = setSpeedRange(SettingN[PARAM_MIN_SPEED].value, SettingN[PARAM_MAX_SPEED].value);
            }

            SettingNP.s = rc ? IPS_OK : IPS_ALERT;
            IDSetNumber(&SettingNP, nullptr);
            return true;
        }
    }

    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

///////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::setParam(const std::string &param, uint32_t value)
{
    char cmd[DRIVER_LEN] = {0};
    int32_t res = 0;
    snprintf(cmd, DRIVER_LEN, "!step %s %d %ud", param.c_str(), IUFindOnSwitchIndex(&PerPortSP), value);
    if (sendCommand(cmd, res))
        return res == 0;

    return false;
}

///////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::getParam(const std::string &param, uint32_t &value)
{
    char cmd[DRIVER_LEN] = {0};
    int32_t res = 0;
    snprintf(cmd, DRIVER_LEN, "!step %s %d", param.c_str(), IUFindOnSwitchIndex(&PerPortSP));
    if (sendCommand(cmd, res))
    {
        value = res;
        return true;
    }

    return false;
}

///////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::setRelayEnabled(uint8_t id, bool enabled)
{
    char cmd[DRIVER_LEN] = {0};
    int32_t res = 0;
    snprintf(cmd, DRIVER_LEN, "!relio rlset 0 %d %d", id, enabled ? 1 : 0);
    if (sendCommand(cmd, res))
        return res == 0;

    return false;
}

///////////////////////////////////////////////////////////////////////////
/// Set speed range in usecs
///////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::setSpeedRange(uint32_t min, uint32_t max)
{
    char cmd[DRIVER_LEN] = {0};
    int32_t res = 0;

    int min_usec = min > 0 ? (50000 - (min - 1) * 50) : 0;
    int max_usec = max > 0 ? (50000 - (max - 1) * 50) : 0;
    snprintf(cmd, DRIVER_LEN, "!step speedrangeus %d %d %d", IUFindOnSwitchIndex(&PerPortSP), min_usec, max_usec);
    if (sendCommand(cmd, res))
        return (res == 0);

    return false;
}

///////////////////////////////////////////////////////////////////////////
/// Sync all settings in case of port change
///////////////////////////////////////////////////////////////////////////

bool DragonFlyDome::syncSettings()
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
bool DragonFlyDome::SetBacklash(int32_t steps)
{
    INDI_UNUSED(steps);
    return true;
}

///////////////////////////////////////////////////////////////////////////
/// Enable/Disable backlash
///////////////////////////////////////////////////////////////////////////

bool DragonFlyDome::SetBacklashEnabled(bool enabled)
{
    INDI_UNUSED(enabled);
    return true;
}

///////////////////////////////////////////////////////////////////////////
/// Read position and update accordingly
///////////////////////////////////////////////////////////////////////////
void DragonFlyDome::TimerHit()
{
    //uint32_t res {0};

    //    if (getParam("getpos", res))
    //    {
    //        if (res != m_LastSteps)
    //        {
    //            m_LastSteps = res;
    //            double newPosition = range360(res / SettingN[PARAM_STEPS_DEGREE].value);
    //            GotoRotatorN[0].value = newPosition;
    //            IDSetNumber(&GotoRotatorNP, nullptr);
    //        }
    //        else
    //            m_IsMoving = false;

    //        if (GotoRotatorNP.s == IPS_BUSY)
    //        {
    //            if (m_IsMoving == false)
    //            {
    //                GotoRotatorNP.s = IPS_OK;
    //                IDSetNumber(&GotoRotatorNP, nullptr);
    //            }
    //        }
    //    }

    SetTimer(POLLMS);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::Abort()
{
    char cmd[DRIVER_LEN] = {0};
    int32_t res = 0;
    snprintf(cmd, DRIVER_LEN, "!step stop %d", IUFindOnSwitchIndex(&PerPortSP));
    if (sendCommand(cmd, res))
        return res == 0;

    return false;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState DragonFlyDome::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        double nextTarget = range360(DomeAbsPosN[0].value + (dir == DOME_CW ? 10 : -10));
        LOGF_INFO("Moving %s by 10 degrees...", (dir == DOME_CW ? "CW" : "CCW"));
        return MoveAbs(nextTarget);
    }
    else
    {
        return (Abort() ? IPS_OK : IPS_ALERT);
    }
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState DragonFlyDome::Park()
{
    MoveAbs(GetAxis1Park());

    LOGF_INFO("Parking to %.2f azimuth...", GetAxis1Park());

    //    if (HasShutter() && IUFindOnSwitchIndex(&CloseShutterOnParkSP) == ND::ENABLED)
    //    {
    //        LOG_INFO("Closing shutter on parking...");
    //        ControlShutter(ShutterOperation::SHUTTER_CLOSE);
    //        DomeShutterS[SHUTTER_OPEN].s = ISS_OFF;
    //        DomeShutterS[SHUTTER_CLOSE].s = ISS_ON;
    //        setShutterState(SHUTTER_MOVING);
    //    }

    return IPS_BUSY;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState DragonFlyDome::UnPark()
{
    SetParked(false);
    return IPS_OK;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::saveConfigItems(FILE *fp)
{
    INDI::Dome::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &PerPortSP);
    IUSaveConfigSwitch(fp, &MotorTypeSP);
    IUSaveConfigNumber(fp, &SettingNP);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::sendCommand(const char * cmd, int32_t &res)
{
    int nbytes_written = 0, nbytes_read = 0;
    char response[DRIVER_LEN] = {0};

    tcflush(PortFD, TCIOFLUSH);

    LOGF_DEBUG("CMD <%s>", cmd);

    char formatted_command[DRIVER_LEN] = {0};
    snprintf(formatted_command, DRIVER_LEN, "%s\r", cmd);
    int rc = tty_write_string(PortFD, formatted_command, &nbytes_written);


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
        char errstr[MAXRBUF] = {0};
        tty_error_msg(rc, errstr, MAXRBUF);
        LOGF_ERROR("Serial read error: %s.", errstr);
        return false;
    }

    // Remove extra #
    response[nbytes_read - 1] = 0;
    LOGF_DEBUG("RES <%s>", response);


    tcflush(PortFD, TCIOFLUSH);

    std::regex rgx(R"(.*:(\d+))");
    std::smatch match;
    std::string input(response);

    if (std::regex_search(input, match, rgx))
    {
        res = std::stoi(match.str(1));
        return true;
    }

    return false;
}


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
#include "connectionplugins/connectiontcp.h"

#include <cmath>
#include <cstring>
#include <cassert>
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

    snprintf(name, MAXINDINAME, "RELAY_%d", id + 1);
    m_Name = name;
    snprintf(label, MAXINDILABEL, "Relay #%d", id + 1);

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

void Relay::setEnabled(bool enabled)
{
    RelayS[DD::INDI_ENABLED].s  = enabled ? ISS_ON : ISS_OFF;
    RelayS[DD::INDI_DISABLED].s = enabled ? ISS_OFF : ISS_ON;
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
    SetDomeCapability(DOME_CAN_ABORT |  DOME_CAN_PARK);
    setDomeConnection(CONNECTION_TCP);
}

bool DragonFlyDome::initProperties()
{
    INDI::Dome::initProperties();

    SetParkDataType(PARK_NONE);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // #1 Relays
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Dome Relays
    IUFillNumber(&DomeControlRelayN[RELAY_OPEN], "RELAY_OPEN", "Open Relay", "%.f", 1., 8., 1., 1.);
    IUFillNumber(&DomeControlRelayN[RELAY_CLOSE], "RELAY_CLOSE", "Close Relay", "%.f", 1., 8., 1., 1.);
    IUFillNumberVector(&DomeControlRelayNP, DomeControlRelayN, 2, getDeviceName(), "DOME_CONTROL_RELAYS", "Relay Control",
                       RELAYS_TAB, IP_RW, 0, IPS_OK);

    // All Relays
    for (uint8_t i = 0; i < 8; i++)
    {
        std::unique_ptr<Relay> oneRelay;
        oneRelay.reset(new Relay(i, getDeviceName(), RELAYS_TAB));
        Relays.push_back(std::move(oneRelay));
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // #2 Sensors
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // Dome Control Sensors
    IUFillNumber(&DomeControlSensorN[SENSOR_UNPARKED], "SENSOR_UNPARKED", "Unparked", "%.f", 1., 8., 1., 1.);
    IUFillNumber(&DomeControlSensorN[SENSOR_PARKED], "SENSOR_PARKED", "Parked", "%.f", 1., 8., 1., 1.);
    IUFillNumberVector(&DomeControlSensorNP, DomeControlSensorN, 2, getDeviceName(), "DOME_CONTROL_SENSORS", "Sensors",
                       SENSORS_TAB, IP_RW, 0, IPS_OK);

    // ALL Sensors
    char sensorName[MAXINDINAME] = {0}, sensorLabel[MAXINDILABEL] = {0};
    for (uint8_t i = 0; i < 8; i++)
    {
        snprintf(sensorName, MAXINDINAME, "SENSOR_%d", i + 1);
        snprintf(sensorLabel, MAXINDILABEL, "Sensor #%d", i + 1);
        IUFillNumber(&SensorN[i], sensorName, sensorLabel, "%.f", 0, 1024, 1, 0);
    }
    IUFillNumberVector(&SensorNP, SensorN, 8, getDeviceName(), "DOME_SENSORS", "Sensors", SENSORS_TAB, IP_RO, 60, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // #3 Communication & Firmware
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // Peripheral Port
    IUFillSwitch(&PerPortS[PORT_MAIN], "PORT_MAIN", "Main", ISS_ON );
    IUFillSwitch(&PerPortS[PORT_EXP], "PORT_EXP", "Exp", ISS_OFF );
    IUFillSwitch(&PerPortS[PORT_THIRD], "PORT_THIRD", "Third", ISS_OFF );
    IUFillSwitchVector(&PerPortSP, PerPortS, 3, getDeviceName(), "DRAGONFLY_PORT", "Port", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY,
                       0, IPS_IDLE);

    // Firmware Version
    IUFillText(&FirmwareVersionT[0], "VERSION", "Version", "");
    IUFillTextVector(&FirmwareVersionTP, FirmwareVersionT, 1, getDeviceName(), "DOME_FIRMWARE", "Firmware", MAIN_CONTROL_TAB,
                     IP_RO, 0, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // #5 Misc.
    ///////////////////////////////////////////////////////////////////////////////////////////////
    tcpConnection->setDefaultHost("192.168.1.1");
    tcpConnection->setDefaultPort(10000);
    tcpConnection->setConnectionType(Connection::TCP::TYPE_UDP);
    tty_set_generic_udp_format(1);
    addDebugControl();
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
        InitPark();

        updateRelays();
        updateSensors();

        double parkedSensor = -1, unparkedSensor = -1;
        IUGetConfigNumber(getDeviceName(), DomeControlSensorNP.name, DomeControlSensorN[SENSOR_UNPARKED].name, &unparkedSensor);
        IUGetConfigNumber(getDeviceName(), DomeControlSensorNP.name, DomeControlSensorN[SENSOR_PARKED].name, &parkedSensor);
        if (parkedSensor > 0 && unparkedSensor > 0)
        {
            if (isSensorOn(unparkedSensor) == isSensorOn(parkedSensor))
            {
                setDomeState(DOME_UNKNOWN);
                LOG_WARN("Parking status is not known.");
            }
            else if (isSensorOn(parkedSensor) != isParked())
                SetParked(isSensorOn(parkedSensor));
        }

        defineText(&FirmwareVersionTP);

        // Relays
        defineNumber(&DomeControlRelayNP);
        for (auto &oneRelay : Relays)
            oneRelay->define(this);

        // Sensors
        defineNumber(&DomeControlSensorNP);
        defineNumber(&SensorNP);

    }
    else
    {
        deleteProperty(FirmwareVersionTP.name);

        deleteProperty(DomeControlRelayNP.name);
        for (auto &oneRelay : Relays)
            oneRelay->remove(this);

        deleteProperty(DomeControlSensorNP.name);
        deleteProperty(SensorNP.name);
    }

    return true;
}

bool DragonFlyDome::Handshake()
{
    return echo();
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
        if ( model > DRIVER_MODELS )
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
            PerPortSP.s = IPS_OK;
            IDSetSwitch(&PerPortSP, nullptr);
            saveConfig(true, PerPortSP.name);
            return true;
        }
        /////////////////////////////////////////////
        // Relays
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
        // Relay Control
        /////////////////////////////////////////////
        if (!strcmp(name, DomeControlRelayNP.name))
        {
            IUUpdateNumber(&DomeControlRelayNP, values, names, n);
            DomeControlRelayNP.s = IPS_OK;
            IDSetNumber(&DomeControlRelayNP, nullptr);
            return true;
        }

        /////////////////////////////////////////////
        // Sensor Control
        /////////////////////////////////////////////
        if (!strcmp(name, DomeControlSensorNP.name))
        {
            IUUpdateNumber(&DomeControlSensorNP, values, names, n);
            DomeControlSensorNP.s = IPS_OK;
            IDSetNumber(&DomeControlSensorNP, nullptr);
            saveConfig(true, DomeControlSensorNP.name);
            return true;
        }
    }

    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
}

///////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::setRelayEnabled(uint8_t id, bool enabled)
{
    char cmd[DRIVER_LEN] = {0};
    int32_t res = 0;
    snprintf(cmd, DRIVER_LEN, "!relio rlset 0 %d %d#", id, enabled ? 1 : 0);
    if (sendCommand(cmd, res))
        return res == (enabled ? 1 : 0);

    return false;
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
    if (!isConnected())
        return;

    // Update all sensors
    m_UpdateSensorCounter++;
    if (m_UpdateSensorCounter >= SENSOR_UPDATE_THRESHOLD)
    {
        m_UpdateSensorCounter = 0;
        if (updateSensors())
            IDSetNumber(&SensorNP, nullptr);
    }

    // Update all relays every RELAY_UPDATE_THRESHOLD timer hit
    m_UpdateRelayCounter++;
    if (m_UpdateRelayCounter >= RELAY_UPDATE_THRESHOLD)
    {
        m_UpdateRelayCounter = 0;
        if (updateRelays())
        {
            for (const auto &oneRelay : Relays)
                oneRelay->sync(oneRelay->isEnabled() ? IPS_OK : IPS_IDLE);
        }
    }

    // If we are in motion
    if (getDomeState() == DOME_MOVING || getDomeState() == DOME_PARKING || getDomeState() == DOME_UNPARKING)
    {
        // Roll off is opening
        if (DomeMotionS[DOME_CW].s == ISS_ON)
        {
            if (isSensorOn(DomeControlSensorN[SENSOR_UNPARKED].value))
            {
                setRoofOpen(false);
                SetParked(false);
            }
        }
        // Roll Off is closing
        else if (DomeMotionS[DOME_CCW].s == ISS_ON)
        {
            if (isSensorOn(DomeControlSensorN[SENSOR_PARKED].value))
            {
                setRoofClose(false);
                SetParked(true);
            }
        }
    }

    SetTimer(POLLMS);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::Abort()
{
    if (getDomeState() == DOME_MOVING)
    {
        return (setRoofOpen(false) && setRoofClose(false));
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::setRoofOpen(bool enabled)
{
    int id = DomeControlRelayN[RELAY_OPEN].value - 1;
    if (id < 0)
        return false;

    int closeRoofRelay = DomeControlRelayN[RELAY_CLOSE].value - 1;
    if (closeRoofRelay < 0)
        return false;
    // Only proceed is RELAY_CLOSE is OFF
    if (enabled && Relays[closeRoofRelay]->isEnabled())
    {
        LOG_DEBUG("Turning off Close Roof Relay in order to turn on Open Roof relay...");
        setRelayEnabled(closeRoofRelay, false);
        Relays[closeRoofRelay]->setEnabled(false);
        Relays[closeRoofRelay]->sync(IPS_IDLE);
    }

    if (setRelayEnabled(id, enabled))
    {
        Relays[id]->setEnabled(enabled);
        Relays[id]->sync(enabled ? IPS_OK : IPS_IDLE);
        return true;
    }
    else
    {
        Relays[id]->setEnabled(!enabled);
        Relays[id]->sync(IPS_ALERT);
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::setRoofClose(bool enabled)
{
    int id = DomeControlRelayN[RELAY_CLOSE].value - 1;
    if (id < 0)
        return false;

    int openRoofRelay = DomeControlRelayN[RELAY_OPEN].value - 1;
    if (openRoofRelay < 0)
        return false;
    // Only proceed is RELAY_OPEN is OFF
    if (enabled && Relays[openRoofRelay]->isEnabled())
    {
        LOG_DEBUG("Turning off Open Roof relay in order to turn on Close Roof relay...");
        setRelayEnabled(openRoofRelay, false);
        Relays[openRoofRelay]->setEnabled(false);
        Relays[openRoofRelay]->sync(IPS_IDLE);
    }

    if (setRelayEnabled(id, enabled))
    {
        Relays[id]->setEnabled(enabled);
        Relays[id]->sync(enabled ? IPS_OK : IPS_IDLE);
        return true;
    }
    else
    {
        Relays[id]->setEnabled(!enabled);
        Relays[id]->sync(IPS_ALERT);
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState DragonFlyDome::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        // DOME_CW --> OPEN. If can we are ask to "open" while we are fully opened as the limit switch indicates, then we simply return false.
        if (dir == DOME_CW && isSensorOn(DomeControlSensorN[SENSOR_UNPARKED].value))
        {
            LOG_WARN("Roof is already fully opened.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CW && getWeatherState() == IPS_ALERT)
        {
            LOG_WARN("Weather conditions are in the danger zone. Cannot open roof.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CCW && isSensorOn(DomeControlSensorN[SENSOR_PARKED].value))
        {
            LOG_WARN("Roof is already fully closed.");
            return IPS_ALERT;
        }
        else if (dir == DOME_CCW && INDI::Dome::isLocked())
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Cannot close roof when mount is locking. See: Telescope parking policy in options tab.");
            return IPS_ALERT;
        }

        if (dir == DOME_CW)
            setRoofOpen(true);
        else
            setRoofClose(true);

        return IPS_BUSY;
    }

    return (Dome::Abort() ? IPS_OK : IPS_ALERT);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState DragonFlyDome::Park()
{
    IPState rc = INDI::Dome::Move(DOME_CCW, MOTION_START);
    if (rc == IPS_BUSY)
    {
        LOG_INFO("Roll off is parking...");
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState DragonFlyDome::UnPark()
{
    IPState rc = INDI::Dome::Move(DOME_CW, MOTION_START);
    if (rc == IPS_BUSY)
    {
        LOG_INFO("Roll off is unparking...");
        return IPS_BUSY;
    }
    else
        return IPS_ALERT;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::saveConfigItems(FILE *fp)
{
    INDI::Dome::saveConfigItems(fp);

    IUSaveConfigSwitch(fp, &PerPortSP);
    IUSaveConfigNumber(fp, &DomeControlRelayNP);
    IUSaveConfigNumber(fp, &DomeControlSensorNP);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Sensor ON?
/////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::isSensorOn(uint8_t id)
{
    assert(id > 0 && id <= SensorNP.nnp);
    return SensorN[id - 1].value > SENSOR_THRESHOLD;
}


/////////////////////////////////////////////////////////////////////////////
/// Update All Sensors
/////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::updateSensors()
{
    for (uint8_t i = 0; i < 8; i++)
    {
        char cmd[DRIVER_LEN] = {0};
        int32_t res = 0;
        snprintf(cmd, DRIVER_LEN, "!relio snanrd 0 %d#", i);
        if (!sendCommand(cmd, res))
            return false;
        SensorN[i].value = res;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Update All Relays
/////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::updateRelays()
{
    for (uint8_t i = 0; i < 8; i++)
    {
        char cmd[DRIVER_LEN] = {0};
        int32_t res = 0;
        snprintf(cmd, DRIVER_LEN, "!relio rldgrd 0 %d#", i);
        if (!sendCommand(cmd, res))
            return false;
        Relays[i]->setEnabled(res == 1);
    }

    return true;

}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::sendCommand(const char * cmd, int32_t &res)
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

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

// Relay class methods are removed as the class itself is removed from .h

DragonFlyDome::DragonFlyDome() : INDI::OutputInterface(this)
{
    setVersion(LUNATICO_VERSION_MAJOR, LUNATICO_VERSION_MINOR);
    SetDomeCapability(DOME_CAN_ABORT |  DOME_CAN_PARK);
    setDomeConnection(CONNECTION_TCP);
}

bool DragonFlyDome::initProperties()
{
    INDI::Dome::initProperties();
    INDI::OutputInterface::initProperties("Outputs", 8, "Relay");

    SetParkDataType(PARK_NONE);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // #1 Relays (Dome Specific Control - these map to OutputInterface relays)
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Dome Relays
    DomeControlRelayNP[RELAY_OPEN].fill("RELAY_OPEN", "Open Relay", "%.f", 1., 8., 1., 1.);
    DomeControlRelayNP[RELAY_CLOSE].fill("RELAY_CLOSE", "Close Relay", "%.f", 1., 8., 1., 1.);
    DomeControlRelayNP.fill(getDeviceName(), "DOME_CONTROL_RELAYS", "Relay Control",
                            RELAYS_TAB, IP_RW, 0, IPS_OK);


    ///////////////////////////////////////////////////////////////////////////////////////////////
    // #2 Sensors
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // Dome Control Sensors
    DomeControlSensorNP[SENSOR_UNPARKED].fill("SENSOR_UNPARKED", "Unparked", "%.f", 1., 8., 1., 1.);
    DomeControlSensorNP[SENSOR_PARKED].fill("SENSOR_PARKED", "Parked", "%.f", 1., 8., 1., 1.);
    DomeControlSensorNP.fill(getDeviceName(), "DOME_CONTROL_SENSORS", "Sensors",
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
    PerPortSP[PORT_MAIN].fill("PORT_MAIN", "Main", ISS_ON );
    PerPortSP[PORT_EXP].fill("PORT_EXP", "Exp", ISS_OFF );
    PerPortSP[PORT_THIRD].fill("PORT_THIRD", "Third", ISS_OFF );
    PerPortSP.fill(getDeviceName(), "DRAGONFLY_PORT", "Port", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY,
                   0, IPS_IDLE);

    // Firmware Version
    FirmwareVersionTP[0].fill("VERSION", "Version", "");
    FirmwareVersionTP.fill(getDeviceName(), "DOME_FIRMWARE", "Firmware", MAIN_CONTROL_TAB,
                           IP_RO, 0, IPS_IDLE);

    // Load Configuration
    PerPortSP.load();

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

    defineProperty(PerPortSP);
}

bool DragonFlyDome::updateProperties()
{
    INDI::Dome::updateProperties();
    INDI::OutputInterface::updateProperties();

    if (isConnected())
    {
        InitPark();

        updateSensors();

        double parkedSensor = -1, unparkedSensor = -1;
        IUGetConfigNumber(getDeviceName(), DomeControlSensorNP.getName(), DomeControlSensorNP[SENSOR_UNPARKED].getName(),
                          &unparkedSensor);
        IUGetConfigNumber(getDeviceName(), DomeControlSensorNP.getName(), DomeControlSensorNP[SENSOR_PARKED].getName(),
                          &parkedSensor);
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

        defineProperty(FirmwareVersionTP);

        // Relays
        defineProperty(DomeControlRelayNP);

        // Sensors
        defineProperty(DomeControlSensorNP);
        defineProperty(&SensorNP);

    }
    else
    {
        deleteProperty(FirmwareVersionTP);

        deleteProperty(DomeControlRelayNP);

        deleteProperty(DomeControlSensorNP);
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

        FirmwareVersionTP[0].setText(txt);
        LOGF_INFO("Setting version to [%s]", txt );

        return true;
    }
    return false;
}

bool DragonFlyDome::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    // Process OutputInterface switches first
    if (INDI::OutputInterface::processSwitch(dev, name, states, names, n))
        return true;

    // Then process device-specific switches
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /////////////////////////////////////////////
        // Peripheral Port
        /////////////////////////////////////////////
        if (PerPortSP.isNameMatch(name))
        {
            PerPortSP.update(states, names, n);
            PerPortSP.setState(IPS_OK);
            PerPortSP.apply();
            saveConfig(PerPortSP);
            return true;
        }
    }

    return INDI::Dome::ISNewSwitch(dev, name, states, names, n);
}

bool DragonFlyDome::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (dev && strcmp(dev, getDeviceName()) == 0)
    {
        if (INDI::OutputInterface::processText(dev, name, texts, names, n))
            return true;
    }
    return INDI::Dome::ISNewText(dev, name, texts, names, n);
}

bool DragonFlyDome::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // Process OutputInterface numbers first (e.g., PulseDurationNP)
    if (INDI::OutputInterface::processNumber(dev, name, values, names, n))
        return true;

    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /////////////////////////////////////////////
        // Relay Control
        /////////////////////////////////////////////
        if (DomeControlRelayNP.isNameMatch(name))
        {
            DomeControlRelayNP.update(values, names, n);
            DomeControlRelayNP.setState(IPS_OK);
            DomeControlRelayNP.apply();
            return true;
        }

        /////////////////////////////////////////////
        // Sensor Control
        /////////////////////////////////////////////
        if (DomeControlSensorNP.isNameMatch(name))
        {
            DomeControlSensorNP.update(values, names, n);
            DomeControlSensorNP.setState(IPS_OK);
            DomeControlSensorNP.apply();
            saveConfig(DomeControlSensorNP);
            return true;
        }
    }

    return INDI::Dome::ISNewNumber(dev, name, values, names, n);
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
    // This is now handled by OutputInterface and UpdateDigitalOutputs
    m_UpdateRelayCounter++;
    if (m_UpdateRelayCounter >= RELAY_UPDATE_THRESHOLD) // This counter can be repurposed or removed later
    {
        m_UpdateRelayCounter = 0;
        UpdateDigitalOutputs();
    }

    // If we are in motion
    if (getDomeState() == DOME_MOVING || getDomeState() == DOME_PARKING || getDomeState() == DOME_UNPARKING)
    {
        // Roll off is opening
        if (DomeMotionSP[DOME_CW].getState() == ISS_ON)
        {
            if (isSensorOn(DomeControlSensorNP[SENSOR_UNPARKED].getValue()))
            {
                setRoofOpen(false);
                SetParked(false);
            }
        }
        // Roll Off is closing
        else if (DomeMotionSP[DOME_CCW].getState() == ISS_ON)
        {
            if (isSensorOn(DomeControlSensorNP[SENSOR_PARKED].getValue()))
            {
                setRoofClose(false);
                SetParked(true);
            }
        }
    }

    SetTimer(getCurrentPollingPeriod());
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
    int open_relay_idx = static_cast<int>(DomeControlRelayNP[RELAY_OPEN].getValue()) - 1;
    if (open_relay_idx < 0 || open_relay_idx >= static_cast<int>(DigitalOutputsSP.size()))
    {
        LOGF_ERROR("Open relay index %d is out of bounds.", open_relay_idx);
        return false;
    }

    int close_relay_idx = static_cast<int>(DomeControlRelayNP[RELAY_CLOSE].getValue()) - 1;
    if (close_relay_idx < 0 || close_relay_idx >= static_cast<int>(DigitalOutputsSP.size()))
    {
        LOGF_ERROR("Close relay index %d is out of bounds for setRoofOpen.", close_relay_idx);
        return false;
    }

    // Only proceed if RELAY_CLOSE is OFF (or we are turning RELAY_OPEN off)
    if (enabled && DigitalOutputsSP[close_relay_idx][INDI::OutputInterface::On].s == ISS_ON)
    {
        LOG_DEBUG("Turning off Close Roof Relay in order to turn on Open Roof relay...");
        if (!CommandOutput(close_relay_idx, INDI::OutputInterface::Off))
        {
            LOG_ERROR("Failed to turn off close relay before opening.");
            // Decide if we should still try to open or return false.
            // For safety, perhaps return false.
            return false;
        }
    }

    return CommandOutput(open_relay_idx, enabled ? INDI::OutputInterface::On : INDI::OutputInterface::Off);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::setRoofClose(bool enabled)
{
    int close_relay_idx = static_cast<int>(DomeControlRelayNP[RELAY_CLOSE].getValue()) - 1;
    if (close_relay_idx < 0 || close_relay_idx >= static_cast<int>(DigitalOutputsSP.size()))
    {
        LOGF_ERROR("Close relay index %d is out of bounds.", close_relay_idx);
        return false;
    }

    int open_relay_idx = static_cast<int>(DomeControlRelayNP[RELAY_OPEN].getValue()) - 1;
    if (open_relay_idx < 0 || open_relay_idx >= static_cast<int>(DigitalOutputsSP.size()))
    {
        LOGF_ERROR("Open relay index %d is out of bounds for setRoofClose.", open_relay_idx);
        return false;
    }

    // Only proceed if RELAY_OPEN is OFF (or we are turning RELAY_CLOSE off)
    if (enabled && DigitalOutputsSP[open_relay_idx][INDI::OutputInterface::On].s == ISS_ON)
    {
        LOG_DEBUG("Turning off Open Roof relay in order to turn on Close Roof relay...");
        if (!CommandOutput(open_relay_idx, INDI::OutputInterface::Off))
        {
            LOG_ERROR("Failed to turn off open relay before closing.");
            return false; // Safety: don't proceed if we can't turn off the opposing relay
        }
    }

    return CommandOutput(close_relay_idx, enabled ? INDI::OutputInterface::On : INDI::OutputInterface::Off);
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
IPState DragonFlyDome::Move(DomeDirection dir, DomeMotionCommand operation)
{
    if (operation == MOTION_START)
    {
        // DOME_CW --> OPEN. If can we are ask to "open" while we are fully opened as the limit switch indicates, then we simply return false.
        if (dir == DOME_CW && isSensorOn(DomeControlSensorNP[SENSOR_UNPARKED].getValue()))
        {
            LOG_WARN("Roof is already fully opened.");
            return IPS_ALERT;
        }
        //         else if (dir == DOME_CW && getWeatherState() == IPS_ALERT)
        //         {
        //             LOG_WARN("Weather conditions are in the danger zone. Cannot open roof.");
        //             return IPS_ALERT;
        //         }
        else if (dir == DOME_CCW && isSensorOn(DomeControlSensorNP[SENSOR_PARKED].getValue()))
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
    INDI::OutputInterface::saveConfigItems(fp);

    PerPortSP.save(fp);
    DomeControlRelayNP.save(fp);
    DomeControlSensorNP.save(fp);

    return true;
}

///////////////////////////////////////////////////////////////////////////////
/// INDI::OutputInterface Implementation
///////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::CommandOutput(uint32_t index, INDI::OutputInterface::OutputState command)
{
    if (index >= 8) // Assuming 8 outputs, adjust as necessary
    {
        LOGF_ERROR("Invalid output index %u. Valid range from 0 to 7.", index);
        return false;
    }

    char cmd[DRIVER_LEN] = {0};
    int32_t res = 0;
    int enabled = (command == INDI::OutputInterface::On) ? 1 : 0;

    // Determine which port to use based on PerPortSP (Main, Exp, Third)
    // For now, assuming port 0 (Main). This might need adjustment if Dragonfly
    // supports multiple relay boards on different selected ports.
    // The original sendCommand in DragonFlyDome doesn't show port selection for !relio commands.
    // It seems to assume the active PortFD is for the correct device.
    // Let's assume '0' is the board ID for !relio commands on the selected port.
    uint8_t board_id = 0;
    // Example if PerPortSP selected EXP or THIRD and they map to different board IDs:
    // if (PerPortSP[PORT_EXP].s == ISS_ON) board_id = 1;
    // else if (PerPortSP[PORT_THIRD].s == ISS_ON) board_id = 2;

    snprintf(cmd, DRIVER_LEN, "!relio rlset %u %u %d#", board_id, index, enabled);
    if (sendCommand(cmd, res))
    {
        // OutputInterface::processSwitch should handle updating DigitalOutputsSP states
        // when it's called from ISNewSwitch.
        // However, direct calls to CommandOutput (e.g. from dome logic)
        // should also reflect the state.
        DigitalOutputsSP[index][INDI::OutputInterface::Off].s = (enabled == 0) ? ISS_ON : ISS_OFF; // ISwitch state
        DigitalOutputsSP[index][INDI::OutputInterface::On].s  = (enabled == 1) ? ISS_ON : ISS_OFF; // ISwitch state
        DigitalOutputsSP[index].setState(IPS_OK);
        DigitalOutputsSP[index].apply();
        return res == enabled;
    }

    DigitalOutputsSP[index].setState(IPS_ALERT);
    DigitalOutputsSP[index].apply();
    LOGF_ERROR("Failed to set relay %u to %d", index, enabled);
    return false;
}

bool DragonFlyDome::UpdateDigitalOutputs()
{
    bool overall_success = true;
    uint8_t board_id = 0; // Assuming board 0 for now, see CommandOutput comments

    for (uint8_t i = 0; i < DigitalOutputsSP.size(); i++)
    {
        char cmd[DRIVER_LEN] = {0};
        int32_t res = 0;
        snprintf(cmd, DRIVER_LEN, "!relio rldgrd %u %u#", board_id, i);
        if (sendCommand(cmd, res))
        {
            bool current_state_on = (res == 1);
            IPState current_property_state = DigitalOutputsSP[i].getState();
            ISState current_on_switch_state = DigitalOutputsSP[i][INDI::OutputInterface::On].s; // ISwitch state

            if (current_property_state == IPS_BUSY) // If busy, don't override, device is pulsing
            {
                // If it was pulsing and now the physical state matches the target of the pulse,
                // we might need to clear the busy state. This is complex with OutputInterface's
                // internal pulse timer. For now, let OutputInterface handle pulse completion.
            }
            // Update if the actual switch state differs from the property's switch element state,
            // or if the property was in alert.
            else if (current_on_switch_state != (current_state_on ? ISS_ON : ISS_OFF) || current_property_state == IPS_ALERT)
            {
                DigitalOutputsSP[i][INDI::OutputInterface::Off].s = current_state_on ? ISS_OFF : ISS_ON; // ISwitch state
                DigitalOutputsSP[i][INDI::OutputInterface::On].s  = current_state_on ? ISS_ON : ISS_OFF; // ISwitch state
                DigitalOutputsSP[i].setState(IPS_OK); // PropertySwitch state
                DigitalOutputsSP[i].apply();          // Send update
            }
            // Sync our custom Relay object if it's still in use (will be removed later)
            // if (i < Relays.size() && Relays[i]) // Check Relays[i] is not null // Removed
            //     Relays[i]->setEnabled(current_state_on);
        }
        else
        {
            if (DigitalOutputsSP[i].getState() != IPS_BUSY)
            {
                DigitalOutputsSP[i].setState(IPS_ALERT);
                DigitalOutputsSP[i].apply();
            }
            overall_success = false;
        }
    }
    return overall_success;
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

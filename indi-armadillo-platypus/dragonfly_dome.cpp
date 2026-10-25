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

    // Close retries: when the roof leaves the closed limit right after a park (or comes back
    // to the open limit while closing), e.g. because the roof controller reversed on an
    // obstruction, send a new close command once the roof is resting at the open limit.
    CloseRetryNP[0].fill("RETRIES", "Retries", "%.f", 0., 3., 1., 0.);
    CloseRetryNP.fill(getDeviceName(), "DOME_CLOSE_RETRY", "Close Retry", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Travel timeout: a roof that reaches no limit within this time stopped mid-way.
    // Must be well above the slowest full travel. 0 disables it.
    TravelTimeoutNP[0].fill("TIMEOUT", "Seconds", "%.f", 0., 600., 10., 0.);
    TravelTimeoutNP.fill(getDeviceName(), "DOME_TRAVEL_TIMEOUT", "Travel Timeout", OPTIONS_TAB, IP_RW, 0, IPS_IDLE);

    // Load Configuration
    PerPortSP.load();
    CloseRetryNP.load();
    TravelTimeoutNP.load();

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
            {
                // InitPark() just set the park status from the persisted config (the last
                // confirmed physical state). A single sensor read right after Connect() can
                // be a transient glitch (the relio board's I/O lines are not guaranteed to be
                // settled yet), so before overriding that persisted state, confirm with a
                // second independent read taken a moment later. Only trust the change if both
                // reads agree; otherwise keep the persisted state and warn instead of silently
                // reporting a wrong park status.
                bool firstReading = isSensorOn(parkedSensor);
                usleep(200000);
                updateSensors();
                bool secondReading = isSensorOn(parkedSensor);
                if (firstReading == secondReading)
                    SetParked(secondReading);
                else
                    LOG_WARN("Parked sensor reading is unstable right after connection, keeping last known park status.");
            }
        }

        defineProperty(FirmwareVersionTP);

        // Relays
        defineProperty(DomeControlRelayNP);

        // Sensors
        defineProperty(DomeControlSensorNP);
        defineProperty(&SensorNP);

        defineProperty(CloseRetryNP);
        defineProperty(TravelTimeoutNP);
    }
    else
    {
        deleteProperty(FirmwareVersionTP);

        deleteProperty(DomeControlRelayNP);

        deleteProperty(DomeControlSensorNP);
        deleteProperty(SensorNP.name);

        deleteProperty(CloseRetryNP);
        deleteProperty(TravelTimeoutNP);
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

        /////////////////////////////////////////////
        /// Close Retry
        /////////////////////////////////////////////
        if (CloseRetryNP.isNameMatch(name))
        {
            CloseRetryNP.update(values, names, n);
            CloseRetryNP.setState(IPS_OK);
            CloseRetryNP.apply();
            saveConfig(CloseRetryNP);
            return true;
        }

        /////////////////////////////////////////////
        /// Travel Timeout
        /////////////////////////////////////////////
        if (TravelTimeoutNP.isNameMatch(name))
        {
            TravelTimeoutNP.update(values, names, n);
            TravelTimeoutNP.setState(IPS_OK);
            TravelTimeoutNP.apply();
            saveConfig(TravelTimeoutNP);
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
    bool sensorsUpdated = false;
    m_UpdateSensorCounter++;
    if (m_UpdateSensorCounter >= SENSOR_UPDATE_THRESHOLD)
    {
        m_UpdateSensorCounter = 0;
        if (updateSensors())
        {
            IDSetNumber(&SensorNP, nullptr);
            sensorsUpdated = true;
        }
    }

    // Update all relays every RELAY_UPDATE_THRESHOLD timer hit
    // This is now handled by OutputInterface and UpdateDigitalOutputs
    m_UpdateRelayCounter++;
    if (m_UpdateRelayCounter >= RELAY_UPDATE_THRESHOLD) // This counter can be repurposed or removed later
    {
        m_UpdateRelayCounter = 0;
        UpdateDigitalOutputs();
    }

    // Sensor values only change on a fresh read, so the roof state is only evaluated then.
    // This also makes the confirmation counters below count real, independent reads.
    if (sensorsUpdated)
        checkRoofState();

    SetTimer(getCurrentPollingPeriod());
}

//////////////////////////////////////////////////////////////////////////////
/// Supervise roof motion and park state from the limit switches.
///
/// Roof controllers with obstacle detection (typically a single toggle relay driving a
/// garage-door style controller) can touch the closed limit for an instant and then reverse
/// back to fully open. Trusting a single parked sensor read reported the roof as parked while
/// it was reopening, and nothing re-checked the sensor afterwards, so a weather emergency
/// park could be reported successful with the roof open.
//////////////////////////////////////////////////////////////////////////////
void DragonFlyDome::checkRoofState()
{
    const bool openOn = isSensorOn(DomeControlSensorNP[SENSOR_UNPARKED].getValue());
    const bool closedOn = isSensorOn(DomeControlSensorNP[SENSOR_PARKED].getValue());
    const auto now = std::chrono::steady_clock::now();

    // A close failed: wait for the roof to rest at the open limit, then close again. With a
    // toggle relay, a new pulse while the roof is still moving would stop it mid-way instead.
    if (m_RecloseWaiting)
    {
        m_OpenConfirmCount = (openOn && !closedOn) ? m_OpenConfirmCount + 1 : 0;
        if (m_OpenConfirmCount >= SENSOR_CONFIRM_READS)
            retryClose();
        else if (now > m_RecloseDeadline)
        {
            m_RecloseWaiting = false;
            LOG_ERROR("Roof did not come back to the open limit after a failed close, its position is unknown. Check the roof!");
            SetParked(false);
            setDomeState(DOME_ERROR);
        }
        return;
    }

    switch (getDomeState())
    {
        case DOME_MOVING:
        case DOME_PARKING:
        case DOME_UNPARKING:
            // Roll off is opening
            if (DomeMotionSP[DOME_CW].getState() == ISS_ON)
            {
                // The relay was released on the previous read: close it again so the
                // controller sees a new pulse.
                if (m_NudgePending)
                {
                    m_NudgePending = false;
                    setRoofOpen(true);
                    m_MotionStart = now;
                    break;
                }

                if (openOn)
                {
                    setRoofOpen(false);
                    SetParked(false);
                }
                // After a new pulse, the roof went back to the closed limit: it is safe, but the
                // unpark failed.
                else if (m_OpenNudgeDone && closedOn)
                {
                    if (++m_ParkedConfirmCount >= SENSOR_CONFIRM_READS)
                    {
                        setRoofOpen(false);
                        LOG_ERROR("Roof went back to the closed limit, unpark failed.");
                        SetParked(true);
                        setDomeState(DOME_ERROR);
                    }
                }
                else if (travelTimedOut(now))
                {
                    // No limit reached in time: the roof stopped mid-way, which leaves it open to
                    // the weather. A new pulse sends it to one of the limits, once per opening.
                    setRoofOpen(false);
                    if (!m_OpenNudgeDone)
                    {
                        m_OpenNudgeDone = true;
                        m_NudgePending = true;
                        m_ParkedConfirmCount = 0;
                        LOGF_WARN("Roof did not reach the open limit within %.f seconds, it probably stopped mid-way. Sending a new pulse.",
                                  TravelTimeoutNP[0].getValue());
                    }
                    else
                    {
                        LOGF_ERROR("Roof did not reach a limit within %.f seconds, it probably stopped mid-way. Check the roof!",
                                   TravelTimeoutNP[0].getValue());
                        setDomeState(DOME_IDLE);
                        setDomeState(DOME_ERROR);
                    }
                }
                else
                    m_ParkedConfirmCount = 0;
            }
            // Roll Off is closing
            else if (DomeMotionSP[DOME_CCW].getState() == ISS_ON)
            {
                if (!openOn)
                    m_LeftOpenLimit = true;

                // The relay was released on the previous read: close it again so the
                // controller sees a new pulse.
                if (m_NudgePending)
                {
                    m_NudgePending = false;
                    setRoofClose(true);
                    m_MotionStart = now;
                    break;
                }

                if (closedOn)
                {
                    // Only trust the closed limit once it is stable over consecutive reads.
                    if (++m_ParkedConfirmCount >= SENSOR_CONFIRM_READS)
                    {
                        setRoofClose(false);
                        SetParked(true);
                        m_ParkedTime = now;
                        m_ParkLostCount = 0;
                    }
                    else
                        LOG_DEBUG("Parked sensor is on, waiting for confirmation...");
                }
                else
                {
                    if (m_ParkedConfirmCount > 0)
                        LOG_WARN("Parked sensor dropped right after reaching the closed limit, the roof may have reversed.");
                    m_ParkedConfirmCount = 0;

                    if (openOn && m_LeftOpenLimit)
                        handleCloseFailure("Roof came back to the open limit while closing (obstruction?)");
                    else if (travelTimedOut(now))
                    {
                        // No limit reached in time: the roof stopped mid-way. A new pulse sends it
                        // to one of the limits; the closed limit completes the park and the open
                        // limit is handled as a failed close above. Uses the close retry budget.
                        setRoofClose(false);
                        if (m_CloseRetriesLeft > 0)
                        {
                            m_CloseRetriesLeft--;
                            m_NudgePending = true;
                            LOGF_WARN("Roof did not reach a limit within %.f seconds, it probably stopped mid-way. Sending a new pulse (%d retry left).",
                                      TravelTimeoutNP[0].getValue(), m_CloseRetriesLeft);
                        }
                        else
                        {
                            LOGF_ERROR("Roof did not reach a limit within %.f seconds, it probably stopped mid-way. Roof is NOT closed!",
                                       TravelTimeoutNP[0].getValue());
                            SetParked(false);
                            setDomeState(DOME_ERROR);
                        }
                    }
                }
            }
            break;

        case DOME_PARKED:
            // The parked sensor must stay on for as long as the dome is reported parked.
            m_ParkLostCount = closedOn ? 0 : m_ParkLostCount + 1;
            if (m_ParkLostCount >= SENSOR_CONFIRM_READS)
            {
                m_ParkLostCount = 0;
                if (now - m_ParkedTime < std::chrono::seconds(RECLOSE_WINDOW_S))
                    handleCloseFailure("Roof left the closed limit right after parking (obstruction?)");
                else
                {
                    LOG_WARN("Roof left the closed limit while parked, dome is no longer parked.");
                    SetParked(false);
                }
            }
            break;

        default:
            break;
    }
}

//////////////////////////////////////////////////////////////////////////////
/// Without a motion sensor, a roof stopped mid-way looks like a moving roof: only the
/// travel time tells them apart.
//////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::travelTimedOut(std::chrono::steady_clock::time_point now) const
{
    const double timeout = TravelTimeoutNP[0].getValue();
    return timeout > 0 && now - m_MotionStart > std::chrono::duration<double>(timeout);
}

//////////////////////////////////////////////////////////////////////////////
/// The roof did not stay closed: release the relay, then either wait for the roof to come
/// back to the open limit and close it again, or report the park as failed.
//////////////////////////////////////////////////////////////////////////////
void DragonFlyDome::handleCloseFailure(const char *reason)
{
    setRoofClose(false);
    m_ParkedConfirmCount = 0;

    if (m_CloseRetriesLeft > 0)
    {
        m_CloseRetriesLeft--;
        m_RecloseWaiting = true;
        m_OpenConfirmCount = 0;
        m_RecloseDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(RECLOSE_WAIT_S);
        LOGF_WARN("%s. Will close again once the roof is back at the open limit.", reason);
        // Stop the motion and record the roof as open, but keep reporting a park in progress
        // to clients.
        SetParked(false);
        setDomeState(DOME_PARKING);
    }
    else
    {
        LOGF_ERROR("%s. Roof is NOT closed!", reason);
        SetParked(false);
        setDomeState(DOME_ERROR);
    }
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
void DragonFlyDome::retryClose()
{
    LOGF_WARN("Roof is back at the open limit, closing again (%d retry left).", m_CloseRetriesLeft);
    // INDI::Dome::Move() refuses new motion while parking.
    setDomeState(DOME_IDLE);
    // m_RecloseWaiting is still set here so that Move() keeps the remaining retry count.
    if (INDI::Dome::Park() == IPS_ALERT)
    {
        LOG_ERROR("Failed to start closing the roof again. Roof is NOT closed!");
        setDomeState(DOME_ERROR);
    }
    m_RecloseWaiting = false;
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool DragonFlyDome::Abort()
{
    m_RecloseWaiting = false;
    m_NudgePending = false;

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
        // CommandOutput() talks to the hardware and updates DigitalOutputsSP directly, without
        // going through ISNewProperty()/ISNewSwitch()/processSwitch(). That matters here because
        // setRoofOpen()/setRoofClose() can be invoked from TimerHit() itself once a motion
        // completes (see TimerHit() below): re-entering the property dispatch pipeline from
        // inside TimerHit() corrupts driver state and crashes the process (SIGABRT).
        if (!CommandOutput(close_relay_idx, INDI::OutputInterface::Off))
        {
            LOG_ERROR("Failed to send command to turn off close relay before opening.");
            if (DigitalOutputsSP[close_relay_idx][INDI::OutputInterface::On].s == ISS_ON)
            {
                LOG_ERROR("Close relay is still ON after attempting to turn it off.");
                return false; // Safety: don't proceed if we can't turn off the opposing relay
            }
        }
    }

    // See the comment above: CommandOutput() is the safe, non-reentrant way to drive a relay.
    if (!CommandOutput(open_relay_idx, enabled ? INDI::OutputInterface::On : INDI::OutputInterface::Off))
    {
        LOGF_ERROR("Failed to send command to %s open relay.", enabled ? "turn on" : "turn off");
        return false;
    }
    return true;
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
        // See setRoofOpen(): CommandOutput() is the safe, non-reentrant way to drive a relay.
        if (!CommandOutput(open_relay_idx, INDI::OutputInterface::Off))
        {
            LOG_ERROR("Failed to send command to turn off open relay before closing.");
            if (DigitalOutputsSP[open_relay_idx][INDI::OutputInterface::On].s == ISS_ON)
            {
                LOG_ERROR("Open relay is still ON after attempting to turn it off.");
                return false; // Safety: don't proceed if we can't turn off the opposing relay
            }
        }
    }

    if (!CommandOutput(close_relay_idx, enabled ? INDI::OutputInterface::On : INDI::OutputInterface::Off))
    {
        LOGF_ERROR("Failed to send command to %s close relay.", enabled ? "turn on" : "turn off");
        return false;
    }
    return true;
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
            // Already in the requested state: this is success, not a failure. Returning
            // IPS_ALERT here breaks idempotent unpark requests (e.g. Scheduler startup after
            // the roof was opened manually) and leaves m_DomeState out of sync with reality.
            LOG_INFO("Roof is already fully opened.");
            SetParked(false);
            return IPS_OK;
        }
        //         else if (dir == DOME_CW && getWeatherState() == IPS_ALERT)
        //         {
        //             LOG_WARN("Weather conditions are in the danger zone. Cannot open roof.");
        //             return IPS_ALERT;
        //         }
        else if (dir == DOME_CCW && isSensorOn(DomeControlSensorNP[SENSOR_PARKED].getValue()))
        {
            LOG_INFO("Roof is already fully closed.");
            SetParked(true);
            return IPS_OK;
        }
        else if (dir == DOME_CCW && INDI::Dome::isLocked())
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Cannot close roof when mount is locking. See: Telescope parking policy in options tab.");
            return IPS_ALERT;
        }

        m_MotionStart = std::chrono::steady_clock::now();
        m_NudgePending = false;

        if (dir == DOME_CW)
        {
            // An explicit open cancels any pending automatic close retry.
            m_RecloseWaiting = false;
            m_OpenNudgeDone = false;
            m_ParkedConfirmCount = 0;
            setRoofOpen(true);
        }
        else
        {
            // Clients may close the roof through DOME_MOTION as well as DOME_PARK, so the
            // retry budget is set here. A new close gets the full budget, an automatic retry
            // keeps what is left.
            if (!m_RecloseWaiting)
                m_CloseRetriesLeft = static_cast<uint8_t>(CloseRetryNP[0].getValue());
            m_ParkedConfirmCount = 0;
            m_LeftOpenLimit = false;
            setRoofClose(true);
        }

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
    CloseRetryNP.save(fp);
    TravelTimeoutNP.save(fp);

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

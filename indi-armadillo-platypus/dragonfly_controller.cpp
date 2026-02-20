/*
    DragonFly Controller

    Copyright (C) 2024 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#include "dragonfly_controller.h"

#include "indicom.h"

#include <cmath>
#include <cstring>
#include <cassert>
#include <memory>
#include <regex>

#include <termios.h>
#include <unistd.h>
#include "config.h"

static class Loader
{
    public:
        std::unique_ptr<DragonFly> device;
    public:
        Loader()
        {
            device.reset(new DragonFly());
        }
} loader;

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
DragonFly::DragonFly(): INDI::InputInterface(this), INDI::OutputInterface(this)
{
    setVersion(LUNATICO_VERSION_MAJOR, LUNATICO_VERSION_MINOR);
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool DragonFly::initProperties()
{
    INDI::DefaultDevice::initProperties();
    INDI::InputInterface::initProperties("Inputs", 8, 0, "Sensor");
    INDI::OutputInterface::initProperties("Outputs", 8, "Relay");

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Communication & Firmware
    ///////////////////////////////////////////////////////////////////////////////////////////////

    // Peripheral Port
    PerPortSP[PORT_MAIN].fill("PORT_MAIN", "Main", ISS_ON );
    PerPortSP[PORT_EXP].fill("PORT_EXP", "Exp", ISS_OFF );
    PerPortSP[PORT_THIRD].fill("PORT_THIRD", "Third", ISS_OFF );
    PerPortSP.fill(getDeviceName(), "DRAGONFLY_PORT", "Port", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY,
                   0, IPS_IDLE);
    // Load Configuration
    PerPortSP.load();

    // Firmware Version
    FirmwareVersionTP[0].fill("VERSION", "Version", "");
    FirmwareVersionTP.fill(getDeviceName(), "DOME_FIRMWARE", "Firmware", MAIN_CONTROL_TAB,
                           IP_RO, 0, IPS_IDLE);

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Misc.
    ///////////////////////////////////////////////////////////////////////////////////////////////
    tcpConnection = new Connection::TCP(this);
    tcpConnection->setDefaultHost("192.168.1.1");
    tcpConnection->setDefaultPort(10000);
    tcpConnection->setConnectionType(Connection::TCP::TYPE_UDP);
    tty_set_generic_udp_format(1);
    tcpConnection->registerHandshake([&]()
    {
        PortFD = tcpConnection->getPortFD();
        return echo();
    });

    registerConnection(tcpConnection);

    addDebugControl();
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
void DragonFly::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);

    defineProperty(PerPortSP);
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool DragonFly::updateProperties()
{
    INDI::DefaultDevice::updateProperties();
    INDI::InputInterface::updateProperties();
    INDI::OutputInterface::updateProperties();

    if (isConnected())
    {
        defineProperty(FirmwareVersionTP);

        // If any relay has a non-zero saved pulse duration, rebuild the full Outputs
        // property stack so that switches, Labels, and Pulse numbers appear in the
        // correct order and [P] suffixes are applied.
        bool anyPulse = false;
        for (size_t i = 0; i < PulseDurationNP.size(); i++)
        {
            if (PulseDurationNP[i][0].getValue() > 0)
            {
                anyPulse = true;
                break;
            }
        }
        if (anyPulse)
            rebuildOutputProperties();

        SetTimer(getPollingPeriod());
    }
    else
    {
        deleteProperty(FirmwareVersionTP);
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
const char *DragonFly::getDefaultName()
{
    return  "DragonFly Controller";
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool DragonFly::echo()
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

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool DragonFly::ISNewText(const char * dev, const char * name, char * texts[], char * names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (INDI::InputInterface::processText(dev, name, texts, names, n))
        {
            // If input labels changed, redefine the switch properties with the new labels
            // so EKOS/clients immediately show the updated names.
            if (DigitalInputLabelsTP.isNameMatch(name))
            {
                for (size_t i = 0; i < DigitalInputsSP.size() && i < DigitalInputLabelsTP.count(); i++)
                {
                    deleteProperty(DigitalInputsSP[i]);
                    DigitalInputsSP[i].setLabel(DigitalInputLabelsTP[i].getText());
                    defineProperty(DigitalInputsSP[i]);
                }
                // Re-append the labels property so it stays below the switch table in the UI
                deleteProperty(DigitalInputLabelsTP);
                defineProperty(DigitalInputLabelsTP);
            }
            return true;
        }
        if (INDI::OutputInterface::processText(dev, name, texts, names, n))
        {
            // If output labels changed, rebuild the full Outputs property stack so that
            // the ordering is: relay switches → Labels text → Pulse numbers.
            if (DigitalOutputLabelsTP.isNameMatch(name))
                rebuildOutputProperties();
            return true;
        }
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool DragonFly::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev && !strcmp(dev, getDeviceName()))
    {
        if (INDI::OutputInterface::processNumber(dev, name, values, names, n))
        {
            // Rebuild the full Outputs property stack so that the switch label [P] indicator
            // and the ordering (switches → Labels → Pulse numbers) stay correct.
            rebuildOutputProperties();
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool DragonFly::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (INDI::OutputInterface::processSwitch(dev, name, states, names, n))
        return true;

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

    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

///////////////////////////////////////////////////////////////////////////
/// Read position and update accordingly
///////////////////////////////////////////////////////////////////////////
void DragonFly::TimerHit()
{
    if (!isConnected())
        return;

    // Throttle polling: sensors every SENSOR_UPDATE_THRESHOLD ticks, relays every RELAY_UPDATE_THRESHOLD ticks.
    // Both run on tick 0 (first call after connect), giving an immediate full update.
    if (m_UpdateSensorCounter++ % SENSOR_UPDATE_THRESHOLD == 0)
        UpdateDigitalInputs();

    if (m_UpdateRelayCounter++ % RELAY_UPDATE_THRESHOLD == 0)
        UpdateDigitalOutputs();

    SetTimer(getCurrentPollingPeriod());
}

//////////////////////////////////////////////////////////////////////////////
///
//////////////////////////////////////////////////////////////////////////////
bool DragonFly::saveConfigItems(FILE *fp)
{
    INDI::DefaultDevice::saveConfigItems(fp);
    INDI::InputInterface::saveConfigItems(fp);
    INDI::OutputInterface::saveConfigItems(fp);

    PerPortSP.save(fp);
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool DragonFly::UpdateDigitalInputs()
{
    int failCount = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        auto oldState = DigitalInputsSP[i].findOnSwitchIndex();
        char cmd[DRIVER_LEN] = {0};
        int32_t res = 0;
        snprintf(cmd, DRIVER_LEN, "!relio snanrd 0 %d#", i);
        if (!sendCommand(cmd, res))
        {
            // Log the individual failure but continue polling the remaining channels.
            // A single dropped UDP packet should not suppress all further updates.
            LOGF_DEBUG("Failed to read sensor %d, continuing.", i);
            failCount++;
            continue;
        }
        // Sensor returns analog value 0-1023; threshold to digital ON (1) or OFF (0)
        int newState = (res > SENSOR_THRESHOLD) ? 1 : 0;
        if (oldState != newState)
        {
            DigitalInputsSP[i].reset();
            DigitalInputsSP[i][newState].setState(ISS_ON);
            DigitalInputsSP[i].setState(IPS_OK);
            DigitalInputsSP[i].apply();
        }
    }

    return failCount == 0;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool DragonFly::UpdateAnalogInputs()
{
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool DragonFly::UpdateDigitalOutputs()
{
    int failCount = 0;
    for (uint8_t i = 0; i < 8; i++)
    {
        char cmd[DRIVER_LEN] = {0};
        int32_t res = 0;
        snprintf(cmd, DRIVER_LEN, "!relio rldgrd 0 %d#", i);
        // rldgrd returns 1 = relay ON, 0 = relay OFF
        if (!sendCommand(cmd, res))
        {
            // Log the individual failure but continue polling the remaining channels.
            // A single dropped UDP packet should not suppress all further updates.
            LOGF_DEBUG("Failed to read relay %d, continuing.", i);
            failCount++;
            continue;
        }
        DigitalOutputsSP[i][INDI::OutputInterface::Off].setState(res == 1 ? ISS_OFF : ISS_ON);
        DigitalOutputsSP[i][INDI::OutputInterface::On].setState(res == 1 ? ISS_ON : ISS_OFF);
        DigitalOutputsSP[i].setState(IPS_OK);
        DigitalOutputsSP[i].apply();
    }

    return failCount == 0;
}

////////////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////////////
bool DragonFly::CommandOutput(uint32_t index, OutputState command)
{
    if (index >= 8)
    {
        LOGF_ERROR("Invalid output index %d. Valid range from 0 to 7.", index);
        return false;
    }

    char cmd[DRIVER_LEN] = {0};
    int32_t res = 0;
    int enabled = command == OutputInterface::On ? 1 : 0;
    snprintf(cmd, DRIVER_LEN, "!relio rlset 0 %d %d#", index, enabled);
    if (sendCommand(cmd, res))
        return res == enabled;

    return false;
}

/////////////////////////////////////////////////////////////////////////////
/// Rebuild Outputs Tab
/////////////////////////////////////////////////////////////////////////////
void DragonFly::rebuildOutputProperties()
{
    // Tear down the full Outputs property stack in reverse order
    for (size_t i = 0; i < PulseDurationNP.size(); i++)
        deleteProperty(PulseDurationNP[i]);
    deleteProperty(DigitalOutputLabelsTP);
    for (size_t i = 0; i < DigitalOutputsSP.size(); i++)
        deleteProperty(DigitalOutputsSP[i]);

    // Rebuild switches with [P] suffix where pulse duration > 0
    for (size_t i = 0; i < DigitalOutputsSP.size() && i < DigitalOutputLabelsTP.count(); i++)
    {
        std::string label = DigitalOutputLabelsTP[i].getText();
        if (i < PulseDurationNP.size() && PulseDurationNP[i][0].getValue() > 0)
            label += " [P]";
        DigitalOutputsSP[i].setLabel(label.c_str());
        defineProperty(DigitalOutputsSP[i]);
    }

    // Labels text below the switch table
    defineProperty(DigitalOutputLabelsTP);

    // Pulse number controls at the bottom
    for (size_t i = 0; i < PulseDurationNP.size(); i++)
        defineProperty(PulseDurationNP[i]);
}

/////////////////////////////////////////////////////////////////////////////
/// Send Command
/////////////////////////////////////////////////////////////////////////////
bool DragonFly::sendCommand(const char * cmd, int32_t &res)
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

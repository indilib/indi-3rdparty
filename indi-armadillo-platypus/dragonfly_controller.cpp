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
            return true;
        if (INDI::OutputInterface::processText(dev, name, texts, names, n))
            return true;
    }

    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
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

    UpdateDigitalInputs();
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
    for (uint8_t i = 0; i < 8; i++)
    {
        auto oldState = DigitalInputsSP[i].findOnSwitchIndex();
        char cmd[DRIVER_LEN] = {0};
        int32_t res = 0;
        snprintf(cmd, DRIVER_LEN, "!relio snanrd 0 %d#", i);
        if (!sendCommand(cmd, res))
            return false;
        if (oldState != res)
        {
            DigitalInputsSP[i].reset();
            DigitalInputsSP[i][res].setState(ISS_ON);
            DigitalInputsSP[i].setState(IPS_OK);
            DigitalInputsSP[i].apply();
        }
    }

    return true;
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
    for (uint8_t i = 0; i < 8; i++)
    {
        char cmd[DRIVER_LEN] = {0};
        int32_t res = 0;
        snprintf(cmd, DRIVER_LEN, "!relio rldgrd 0 %d#", i);
        if (!sendCommand(cmd, res))
            return false;
        DigitalOutputsSP[i][INDI::OutputInterface::Off].setState(res == 1 ? ISS_OFF : ISS_ON);
        DigitalOutputsSP[i][INDI::OutputInterface::On].setState(res == 1 ? ISS_ON : ISS_OFF);
        DigitalOutputsSP[i].setState(IPS_OK);
        DigitalOutputsSP[i].apply();
    }

    return true;
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

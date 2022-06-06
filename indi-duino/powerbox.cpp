/*
    Power Box - an Arduino based power box controlling two power switches
    and two PWM controlled dimmers.

    Copyright (C) 2022 Wolfgang Reissenberger <sterne-jaegeropenfuture.de>

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

#include "powerbox.h"

#include "indistandardproperty.h"
#include "indilogger.h"
#include "connectionplugins/connectionserial.h"
#include "indicom.h"

#include "config.h"

/* Our power box auto pointer */
static std::unique_ptr<PowerBox> powerbox_ptr(new PowerBox());

#define MAX_POWERBOXBUFFER 512

PowerBox::PowerBox() : LightBoxInterface(this, true)
{
    setVersion(DUINOPOWERBOX_VERSION_MAJOR, DUINOPOWERBOX_VERSION_MINOR);

    // define power box commands
    commands[CMD_CONFIG]         = "c";
    commands[CMD_STATUS]         = "i";
    commands[CMD_PWM_FREQUENCY]  = "f";
    commands[CMD_PWM_DUTY_CYCLE] = "d";
    commands[CMD_PWM_POWER]      = "p";
    commands[CMD_SWITCH_POWER]   = "s";

}

void PowerBox::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);
}

bool PowerBox::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool PowerBox::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool PowerBox::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool PowerBox::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    return INDI::DefaultDevice::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool PowerBox::Handshake()
{
    // Load the configuration
    loadConfig();

    // retrieve basic data to initialize the weather station
    IPState result = getBasicData();
    return result == IPS_OK;
}

const char *PowerBox::getDefaultName()
{
    return "Arduino Power Box";
}

bool PowerBox::initProperties()
{
    INDI::DefaultDevice::initProperties();
    INDI::LightBoxInterface::initLightBoxProperties(getDeviceName(), MAIN_CONTROL_TAB);

    addConfigurationControl();

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE);

    addAuxControls();

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]()
    {
        return Handshake();
    });
    registerConnection(serialConnection);

    return true;
}

bool PowerBox::updateProperties()
{
    bool result = true;
    if (isConnected())
    {
        defineProperty(&LightSP);
        defineProperty(&LightIntensityNP);

        result = updateLightBoxProperties();
    }
    else
    {
        deleteProperty(LightSP.name);
        deleteProperty(LightIntensityNP.name);

        result = updateLightBoxProperties();
    }

    return result;
}

IPState PowerBox::getBasicData()
{
    IPState result = IPS_OK;
    return result;
}

/**************************************************************************************
** Helper functions for serial communication
***************************************************************************************/
bool PowerBox::receiveSerial(char *buffer, int *bytes, char end, int wait)
{
    int timeout = wait;
    int returnCode = TTY_PORT_BUSY;
    int retry = 0;

    while (returnCode != TTY_OK && retry < 3)
    {
        returnCode = tty_read_section(PortFD, buffer, end, timeout, bytes);
        if (returnCode != TTY_OK)
        {
            char errorString[MAXRBUF];
            tty_error_msg(returnCode, errorString, MAXRBUF);
            if(returnCode == TTY_TIME_OUT && wait <= 0) return false;
            if (retry++ < 3)
                LOGF_INFO("Failed to receive full response: %s. (Return code: %d). Retrying...", errorString, returnCode);
            else
            {
                LOGF_WARN("Failed to receive full response: %s. (Return code: %d). Giving up", errorString, returnCode);
                return false;
            }
        }
    }
    return true;
}

bool PowerBox::transmitSerial(std::string buffer)
{
    int bytesWritten = 0;
    int returnCode = tty_write_string(PortFD, buffer.c_str(), &bytesWritten);

    if (returnCode != TTY_OK)
    {
        char errorString[MAXRBUF];
        tty_error_msg(returnCode, errorString, MAXRBUF);
        LOGF_WARN("Failed to transmit %s. Wrote %d bytes and got error %s.",
                  buffer.c_str(), bytesWritten, errorString);
        return false;
    }
    return true;
}

bool PowerBox::executeCommand(PowerBox::pb_command cmd, std::string args)
{
    std::string cmdstring= commands[cmd];
    // append arguments if present
    if (args != "")
        cmdstring += "?" + args;

    char response[MAX_POWERBOXBUFFER] = {0};
    int length = 0;
    // communication through a serial (USB) interface
    if (getActiveConnection()->type() == Connection::Interface::CONNECTION_SERIAL)
    {
        // first clear the read buffer for unexpected data
        while (receiveSerial(response, &length, '\n', 0))
        {
            if (response[0] == '\0') // nothing received
                break;
            response[0] = '\0';
        }
        // send query
        LOGF_DEBUG("Sending query: %s", cmdstring.c_str());

        if(!transmitSerial(cmdstring + "\n"))
        {
            LOGF_ERROR("Command <%s> failed.", cmdstring.c_str());
            return false;
        }
        // read the response lines
        bool result = receiveSerial(response, &length, '\n', getTTYTimeout());
        if (result)
            handleResponse(cmd, response, length);
        else
        {
            LOG_ERROR("Receiving response failed.");
            return false;
        }
        // read subsequent lines
        while (receiveSerial(response, &length, '\n', 0))
        {
            if (response[0] == '\0') // nothing received
                break;
            handleResponse(cmd, response, length);
        }
        return true;
    }
    // other connection types not implemented
    else
    {
        LOG_ERROR("Unexpected connection type.");
        return false;
    }
}

void PowerBox::handleResponse(PowerBox::pb_command cmd, const char *response, u_long length)
{
    // ignore empty response and non JSON
    if (length == 0 || strcmp(response, "\r\n") == 0 || (response[0] != '[' && response[0] != '{'))
        return;

    char *source{new char[length + 1] {0}};
    // duplicate the buffer since the parser will modify it
    strncpy(source, response, static_cast<size_t>(length));

    // parse JSON string
    char *endptr;
    JsonValue value;
    JsonAllocator allocator;
    int status = jsonParse(source, &endptr, &value, allocator);
    if (status != JSON_OK)
    {
        LOGF_ERROR("Parsing error %s at %zd", jsonStrError(status), endptr - source);
        return;
    }

    JsonIterator typeIter;
    for (typeIter = begin(value); typeIter != end(value); ++typeIter)
    {
        LOGF_WARN("Unknown response type: %s", typeIter->key);
    }
}

void PowerBox::handleMessage(JsonValue value)
{
    JsonIterator typeIter;
    char* text = nullptr;
    char* messagetype = nullptr;

    for (typeIter = begin(value); typeIter != end(value); ++typeIter)
    {
        if (strcmp(typeIter->key, "text") == 0)
            text = strdup(typeIter->value.toString());
        else if (strcmp(typeIter->key, "type") == 0)
            messagetype = strdup(typeIter->value.toString());
    }
    // log the message
    if (text != nullptr)
    {
        if (messagetype == nullptr || strcmp(messagetype, "debug") == 0)
            LOG_DEBUG(text);
        else if (strcmp(messagetype, "alert") == 0)
            LOG_ERROR(text);
        else if (strcmp(messagetype, "warning") == 0)
            LOG_WARN(text);
        else
            LOG_INFO(text);
    }
    // clean up
    if (text != nullptr) free(text);
    if (messagetype != nullptr) free(messagetype);
}

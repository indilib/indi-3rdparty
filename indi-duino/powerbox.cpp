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
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        if (strcmp(name, PWMFrequencyNP.name) == 0)
        {
            // set Arduino PWM frequency
            IUUpdateNumber(&PWMFrequencyNP, values, names, n);
            std::string args = "value=" + std::to_string(static_cast<int>(values[0]));

            bool result = executeCommand(CMD_PWM_FREQUENCY, args);
            PWMFrequencyNP.s = result ? IPS_OK : IPS_ALERT;
            IDSetNumber(&PWMFrequencyNP, nullptr);
            return result;
        }
        else if (strcmp(name, PWMDutyCycle_1_NP.name) == 0)
        {
            // set duty cycle of PWM port 2
            IUUpdateNumber(&PWMDutyCycle_1_NP, values, names, n);
            bool result = setPWMDutyCycle(1, static_cast<int>(values[0]));
            return result;
        }
        else if (strcmp(name, PWMDutyCycle_2_NP.name) == 0)
        {
            // set duty cycle of PWM port 2
            IUUpdateNumber(&PWMDutyCycle_2_NP, values, names, n);
            bool result = setPWMDutyCycle(2, static_cast<int>(values[0]));
            return result;
        }
        else if (strcmp(name, LightIntensityNP.name) == 0)
        {
            // forward the light box intensity to the selected PWM port
            IUUpdateNumber(&LightIntensityNP, values, names, n);
            double intensity = LightIntensityN[0].value;
            int pwmPort = 1 + IUFindOnSwitchIndex(&LightBoxPWMPortSP);
            setPWMDutyCycle(pwmPort, static_cast<int>(round(intensity)));
        }
    }
    // in all other cases let the default device handle the switch
    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool PowerBox::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool PowerBox::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {

        if (strcmp(name, PowerPortStatus_1_SP.name) == 0)
        {
            // set power port 1
            IUUpdateSwitch(&PowerPortStatus_1_SP, states, names, n);
            bool result = setPowerPortStatus(1);
            return result;
        }
        else if (strcmp(name, PowerPortStatus_2_SP.name) == 0)
        {
            // set power port 2
            IUUpdateSwitch(&PowerPortStatus_2_SP, states, names, n);
            bool result = setPowerPortStatus(2);
            return result;
        }
        else if (strcmp(name, PWMPortStatus_1_SP.name) == 0)
        {
            // set PWM port 1
            IUUpdateSwitch(&PWMPortStatus_1_SP, states, names, n);
            bool result = setPWMPortStatus(1, IUFindOnSwitchIndex(&PWMPortStatus_1_SP) == POWER_ON);
            return result;
        }
        else if (strcmp(name, PWMPortStatus_2_SP.name) == 0)
        {
            // set PWM port 2
            IUUpdateSwitch(&PWMPortStatus_2_SP, states, names, n);
            bool result = setPWMPortStatus(2,  IUFindOnSwitchIndex(&PWMPortStatus_2_SP) == POWER_ON);
            return result;
        }
        else if (strcmp(name, LightBoxPWMPortSP.name) == 0)
        {
            // select light box PWM port
            IUUpdateSwitch(&LightBoxPWMPortSP, states, names, n);
            LightBoxPWMPortSP.s = IPS_OK;
            IDSetSwitch(&LightBoxPWMPortSP, nullptr);
            updateLightBoxSettings();
        }
        else if (strcmp(name, LightSP.name) == 0)
        {
            IUUpdateSwitch(&LightSP, states, names, n);
            bool powerOn = (IUFindOnSwitchIndex(&LightSP) == FLAT_LIGHT_ON);
            int pwmPort = 1 + IUFindOnSwitchIndex(&LightBoxPWMPortSP);
            setPWMPortStatus(pwmPort, powerOn);
        }
    }
    // in all other cases let the default device handle the switch
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool PowerBox::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    return INDI::DefaultDevice::ISNewBLOB(dev, name, sizes, blobsizes, blobs, formats, names, n);
}

bool PowerBox::Handshake()
{
    PortFD = serialConnection->getPortFD();

    // Load the configuration
    loadConfig();

    // retrieve basic data to initialize the weather station
    bool result = getBasicData();
    return result;
}

const char *PowerBox::getDefaultName()
{
    return "Arduino Power Box";
}

bool PowerBox::initProperties()
{
    INDI::DefaultDevice::initProperties();

    // Power Port 1
    IUFillSwitch(&PowerPortStatus_1_S[POWER_ON], "ON", "On", ISS_OFF);
    IUFillSwitch(&PowerPortStatus_1_S[POWER_OFF], "OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&PowerPortStatus_1_SP, PowerPortStatus_1_S, 2, getDeviceName(), "POWER_PORT_1", "Power Port 1",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    // Power Port 2
    IUFillSwitch(&PowerPortStatus_2_S[POWER_ON], "ON", "On", ISS_OFF);
    IUFillSwitch(&PowerPortStatus_2_S[POWER_OFF], "OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&PowerPortStatus_2_SP, PowerPortStatus_2_S, 2, getDeviceName(), "POWER_PORT_2", "Power Port 2",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    // PWM frequency
    IUFillNumber(&PWMFrequencyN[0], "PWM_FREQUENCY", "PWM Frequency (Hz)", "%.f", 0, 50000, 100, getTTYTimeout());
    IUFillNumberVector(&PWMFrequencyNP, PWMFrequencyN, 1, getDeviceName(), "PWM_SETUP", "PWM Setup", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // PWM Port 1
    IUFillSwitch(&PWMPortStatus_1_S[POWER_ON], "ON", "On", ISS_OFF);
    IUFillSwitch(&PWMPortStatus_1_S[POWER_OFF], "OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&PWMPortStatus_1_SP, PWMPortStatus_1_S, 2, getDeviceName(), "PWM_PORT_1", "PWM Port 1",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    IUFillNumber(&PWMDutyCycle_1_N[0], "PWM_DUTY_CYCLE", "PWM Duty Cycle", "%.f", 0, 255, 1, getTTYTimeout());
    IUFillNumberVector(&PWMDutyCycle_1_NP, PWMDutyCycle_1_N, 1, getDeviceName(), "PWM_PORT_1_DC", " ", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    // PWM Port 2
    IUFillSwitch(&PWMPortStatus_2_S[POWER_ON], "ON", "On", ISS_OFF);
    IUFillSwitch(&PWMPortStatus_2_S[POWER_OFF], "OFF", "Off", ISS_OFF);
    IUFillSwitchVector(&PWMPortStatus_2_SP, PWMPortStatus_2_S, 2, getDeviceName(), "PWM_PORT_2", "PWM Port 2",
                       MAIN_CONTROL_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);
    IUFillNumber(&PWMDutyCycle_2_N[0], "PWM_DUTY_CYCLE", "PWM Duty Cycle", "%.f", 0, 255, 1, getTTYTimeout());
    IUFillNumberVector(&PWMDutyCycle_2_NP, PWMDutyCycle_2_N, 1, getDeviceName(), "PWM_PORT_2_DC", " ", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);
    initLightBoxProperties(getDeviceName(), LIGHT_BOX_TAB);

    addConfigurationControl();

    setDriverInterface(AUX_INTERFACE | LIGHTBOX_INTERFACE);
    // Light box control
    IUFillSwitch(&LightBoxPWMPortS[0], "PWM_1", "PWM 1", ISS_ON);
    IUFillSwitch(&LightBoxPWMPortS[1], "PWM_2", "PWM 2", ISS_OFF);
    IUFillSwitchVector(&LightBoxPWMPortSP, LightBoxPWMPortS, 2, getDeviceName(), "LIGHT_BOX_PWM_PORT", "Light Box PWM Port",
                       LIGHT_BOX_TAB, IP_RW, ISR_1OFMANY, 60, IPS_IDLE);

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
        defineProperty(&PowerPortStatus_1_SP);
        defineProperty(&PowerPortStatus_2_SP);
        defineProperty(&PWMFrequencyNP);
        defineProperty(&PWMPortStatus_1_SP);
        defineProperty(&PWMDutyCycle_1_NP);
        defineProperty(&PWMPortStatus_2_SP);
        defineProperty(&PWMDutyCycle_2_NP);
        defineProperty(&LightSP);
        defineProperty(&LightIntensityNP);
        defineProperty(&LightBoxPWMPortSP);

        result = updateLightBoxProperties();
    }
    else
    {
        deleteProperty(LightBoxPWMPortSP.name);
        deleteProperty(LightSP.name);
        deleteProperty(LightIntensityNP.name);
        deleteProperty(PWMPortStatus_2_SP.name);
        deleteProperty(PWMDutyCycle_2_NP.name);
        deleteProperty(PWMPortStatus_1_SP.name);
        deleteProperty(PWMDutyCycle_1_NP.name);
        deleteProperty(PWMFrequencyNP.name);
        deleteProperty(PowerPortStatus_2_SP.name);
        deleteProperty(PowerPortStatus_1_SP.name);

        result = updateLightBoxProperties();
    }

    return result;
}

bool PowerBox::getBasicData()
{
    // read device status
    bool result = getStatus();
    return result;
}

bool PowerBox::getStatus()
{
    bool result = executeCommand(CMD_STATUS);
    return result;
}

bool PowerBox::SetLightBoxBrightness(uint16_t value)
{
    LOG_WARN("PowerBox::SetLightBoxBrightness not implemented");
    INDI_UNUSED(value);
    return true;
}

bool PowerBox::EnableLightBox(bool enable)
{
    LOG_WARN("PowerBox::EnableLightBox not implemented");
    INDI_UNUSED(enable);
    return true;
}

bool PowerBox::saveConfigItems(FILE *fp)
{
    bool result = saveLightBoxConfigItems(fp);
    result &= INDI::DefaultDevice::saveConfigItems(fp);
    // remember the PWM port selection for the light port
    IUSaveConfigSwitch(fp, &LightBoxPWMPortSP);
    return result;
}

void PowerBox::TimerHit()
{
    if (!isConnected())
        return;

    SetTimer(getCurrentPollingPeriod());
}

IPState PowerBox::handleConfig(JsonValue jvalue)
{
    INDI_UNUSED(jvalue);
    LOG_WARN("PowerBox::handleConfig not implemented");
    return IPS_OK;
}

IPState PowerBox::handleStatus(JsonValue jvalue)
{
    JsonIterator statusIter;
    for (statusIter = begin(jvalue); statusIter != end(jvalue); ++statusIter)
    {
        if (strcmp(statusIter->key, "PWM frequency") == 0)
        {
            if (statusIter->value.getTag() == JSON_NUMBER)
            {
                PWMFrequencyN[0].value = statusIter->value.toNumber();
                PWMFrequencyNP.s = IPS_OK;
            }
            else
            {
                PWMFrequencyNP.s = IPS_ALERT;
                LOGF_WARN("Unknown PWM frequency %s", statusIter->value.toString());
            }
            IDSetNumber(&PWMFrequencyNP, nullptr);
        }
        else if (strncmp(statusIter->key, "PWM", 3) == 0 && strlen(statusIter->key) > 4)
        {
            int switch_nr = static_cast<int>(statusIter->key[4] - '0');
            PwmStatus status = readPWMPortStatus(statusIter->value);
            switch (switch_nr)
            {
            case 1:
                // set PWM 1 status
                PWMPortStatus_1_S[POWER_ON].s = status.power ? ISS_ON : ISS_OFF;
                PWMPortStatus_1_S[POWER_OFF].s = status.power ? ISS_OFF : ISS_ON;
                PWMPortStatus_1_SP.s = IPS_OK;
                IDSetSwitch(&PWMPortStatus_1_SP, nullptr);
                // set PWM 1 duty cycle
                PWMDutyCycle_1_N[0].value = status.duty_cycle;
                PWMDutyCycle_1_NP.s = IPS_OK;
                IDSetNumber(&PWMDutyCycle_1_NP, nullptr);
                break;
            case 2:
                // set PWM 2 status
                PWMPortStatus_2_S[POWER_ON].s = status.power ? ISS_ON : ISS_OFF;
                PWMPortStatus_2_S[POWER_OFF].s = status.power ? ISS_OFF : ISS_ON;
                PWMPortStatus_2_SP.s = IPS_OK;
                IDSetSwitch(&PWMPortStatus_2_SP, nullptr);
                // set PWM 2 duty cycle
                PWMDutyCycle_2_N[0].value = status.duty_cycle;
                PWMDutyCycle_2_NP.s = IPS_OK;
                IDSetNumber(&PWMDutyCycle_2_NP, nullptr);
                break;
            default:
                LOGF_WARN("Unknown PWM control %d %s, duty cycle %f", switch_nr, status.power ? "on" : "off", status.duty_cycle);
                break;
            }
        }
        else if (strncmp(statusIter->key, "Switch", 6) == 0 && strlen(statusIter->key) > 7)
        {
            int switch_nr = static_cast<int>(statusIter->key[7] - '0');
            bool status = readPowerPortStatus(statusIter->value);
            switch (switch_nr)
            {
            case 1:
                // set power switch 1 status
                PowerPortStatus_1_S[POWER_ON].s = status ? ISS_ON : ISS_OFF;
                PowerPortStatus_1_S[POWER_OFF].s = status ? ISS_OFF : ISS_ON;
                PowerPortStatus_1_SP.s = IPS_OK;
                IDSetSwitch(&PowerPortStatus_1_SP, nullptr);
                break;
            case 2:
                // set power switch 2 status
                PowerPortStatus_2_S[POWER_ON].s = status ? ISS_ON : ISS_OFF;
                PowerPortStatus_2_S[POWER_OFF].s = status ? ISS_OFF : ISS_ON;
                PowerPortStatus_2_SP.s = IPS_OK;
                IDSetSwitch(&PowerPortStatus_2_SP, nullptr);
                break;
            default:
                LOGF_WARN("Unknown power switch %d %s", switch_nr, status ? "on" : "off");
                break;
            }
        }
    }
    // forward new settings to light box
    updateLightBoxSettings();

    return IPS_OK;
}

bool PowerBox::readPowerPortStatus(JsonValue jvalue)
{
    JsonIterator statusIter;
    for (statusIter = begin(jvalue); statusIter != end(jvalue); ++statusIter)
    {
        if (strcmp(statusIter->key, "power") == 0)
            return (strcmp(statusIter->value.toString(), "on") == 0);
    }
    LOG_WARN("Power status missing");
    return false;
}

bool PowerBox::setPowerPortStatus(int port_number)
{
    ISwitchVectorProperty *svp = port_number == 1 ? &PowerPortStatus_1_SP : &PowerPortStatus_2_SP;
    int pressed = IUFindOnSwitchIndex(svp);
    std::string args = "id=" + std::to_string(port_number) + "&power=" + (pressed == POWER_ON ? "on" : "off");

    bool result = executeCommand(CMD_SWITCH_POWER, args);
    svp->s = result ? IPS_OK : IPS_ALERT;
    IDSetSwitch(svp, nullptr);
    return result;
}

PowerBox::PwmStatus PowerBox::readPWMPortStatus(JsonValue jvalue)
{
    PwmStatus status = {false, -1.0};
    JsonIterator statusIter;
    for (statusIter = begin(jvalue); statusIter != end(jvalue); ++statusIter)
    {
        if (strcmp(statusIter->key, "power") == 0)
            status.power = (strcmp(statusIter->value.toString(), "on") == 0);
        else if (strcmp(statusIter->key, "duty cycle") == 0 && statusIter->value.getTag() == JSON_NUMBER)
            status.duty_cycle = statusIter->value.toNumber();
    }
    return status;
}

bool PowerBox::setPWMPortStatus(int port_number, bool powerOn)
{
    std::string args = "id=" + std::to_string(port_number) + "&power=" + (powerOn ? "on" : "off");

    bool result = executeCommand(CMD_PWM_POWER, args);
    // select the right switch
    ISwitchVectorProperty *svp = port_number == 1 ? &PWMPortStatus_1_SP : &PWMPortStatus_2_SP;
    svp->s = result ? IPS_OK : IPS_ALERT;
    IDSetSwitch(svp, nullptr);
    return result;
}

bool PowerBox::setPWMDutyCycle(int port_number, int value)
{
    INumberVectorProperty *nvp = port_number == 1 ? &PWMDutyCycle_1_NP : &PWMDutyCycle_2_NP;
    std::string args = "id=" + std::to_string(port_number) + "&value=" + std::to_string(value);

    bool result = executeCommand(CMD_PWM_DUTY_CYCLE, args);
    nvp->s = result ? IPS_OK : IPS_ALERT;
    IDSetNumber(nvp, nullptr);
    return result;
}

void PowerBox::updateLightBoxSettings()
{
    int portSelected = IUFindOnSwitchIndex(&LightBoxPWMPortSP);
    ISwitchVectorProperty *pwmPort = (portSelected == 0) ? &PWMPortStatus_1_SP : &PWMPortStatus_2_SP;
    INumber *dutyCycle = (portSelected == 0) ? &PWMDutyCycle_1_N[0] : &PWMDutyCycle_2_N[0];

    // read settings
    bool pwmPortOn = IUFindOnSwitchIndex(pwmPort) == POWER_ON;
    double value   = dutyCycle->value;

    //update light settings
    LightS[POWER_ON].s = pwmPortOn ? ISS_ON : ISS_OFF;
    LightS[POWER_OFF].s = pwmPortOn ? ISS_OFF : ISS_ON;
    LightSP.s = IPS_OK;
    // update intensity
    LightIntensityN[0].value = value;
    LightIntensityNP.s = IPS_OK;
    IDSetSwitch(&LightSP, nullptr);
    IDSetNumber(&LightIntensityNP, nullptr);
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
    std::string cmdstring = commands[cmd];
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
    INDI_UNUSED(cmd);
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
        if (strcmp(typeIter->key, "config") == 0)
            handleConfig(typeIter->value);
        else if (strcmp(typeIter->key, "status") == 0)
            handleStatus(typeIter->value);
        else
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

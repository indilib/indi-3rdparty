#if 0
    Induino general propose driver. Allow using arduino boards
    as general I/O 	
    Copyright 2012 (c) Nacho Mas (mas.ignacio at gmail.com)


    Base on Tutorial Four
    Demonstration of libindi v0.7 capabilities.

    Copyright (C) 2010 Jasem Mutlaq (mutlaqja@ikarustech.com)

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

#endif

#include "indiduino.h"
#include "connectionplugins/connectionserial.h"
#include "connectionplugins/connectiontcp.h"

#include "config.h"
#include "firmata.h"

#include <indicontroller.h>

#include <memory>
#include <sys/stat.h>

/* Our indiduino auto pointer */
std::unique_ptr<indiduino> indiduino_prt(new indiduino());

const char *indiduino_id = "indiduino";

/**************************************************************************************
** Initialize firmata debug
***************************************************************************************/

void firmata_debug(const char *file, int line, const char *message, ...)
{
    va_list ap;
    char msg[257];
    msg[256] = '\0';
    va_start(ap, message);
    vsnprintf(msg, 257, message, ap);
    va_end(ap);

    INDI::Logger::getInstance().print(indiduino_prt->getDeviceName(), INDI::Logger::DBG_DEBUG, file, line, msg);
}


/**************************************************************************************
**
***************************************************************************************/
indiduino::indiduino()
{
    LOG_DEBUG("Indiduino driver start...");
    firmata_debug_cb = firmata_debug;
    setVersion(DUINO_VERSION_MAJOR, DUINO_VERSION_MINOR);
    controller = new INDI::Controller(this);
    controller->setJoystickCallback(joystickHelper);
    controller->setButtonCallback(buttonHelper);
    controller->setAxisCallback(axisHelper);
}

/**************************************************************************************
**
***************************************************************************************/
indiduino::~indiduino()
{
    delete (controller);
}

bool indiduino::ISSnoopDevice(XMLEle *root)
{
    controller->ISSnoopDevice(root);
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

void indiduino::TimerHit()
{
    if (isConnected() == false)
        return;

    sf->OnIdle();

    for (const auto &it: *getProperties())
    {
        const char *name = it.getName();
        INDI_PROPERTY_TYPE type = it.getType();

        //DIGITAL INPUT
        if (type == INDI_LIGHT)
        {
            bool changed = false;
            auto lvp = getLight(name);
            if (lvp.getLight()->getAux() != (void *)indiduino_id)
                continue;

            for (auto &lqp: lvp)
            {
                IO *pin_config = (IO *)lqp.getAux();
                if (pin_config == nullptr)
                    continue;
                if (pin_config->IOType == DI)
                {
                    int pin = pin_config->pin;
                    if (sf->pin_info[pin].mode == FIRMATA_MODE_INPUT)
                    {
                        if ((sf->pin_info[pin].value == 1) && (lqp.getState() != IPS_OK))
                        {
                            //LOGF_DEBUG("%s.%s on pin %u change to  ON",lvp->name,lqp->name,pin);
                            //IDSetLight (lvp, "%s.%s change to ON\n",lvp->name,lqp->name);
                            lqp.setState(IPS_OK);
                            changed = true;

                        }
                        else if ((sf->pin_info[pin].value == 0) && (lqp.getState() != IPS_IDLE))
                        {
                            //LOGF_DEBUG("%s.%s on pin %u change to  OFF",lvp->name,lqp->name,pin);
                            //IDSetLight (lvp, "%s.%s change to OFF\n",lvp->name,lqp->name);
                            lqp.setState(IPS_IDLE);
                            changed = true;
                        }
                    }
                }
            }
            if (changed) lvp.apply();
        }

        //read back DIGITAL OUTPUT values as reported by the board (FIRMATA_PIN_STATE_RESPONSE)
        if (type == INDI_SWITCH)
        {
            bool changed = false;
            int n_on = 0;
            auto svp = getSwitch(name);

            if (svp.getSwitch()->getAux() != (void *)indiduino_id)
                continue;

            for (auto &sqp: svp)
            {
                IO *pin_config = (IO *)sqp.getAux();
                if (pin_config == nullptr)
                    continue;
                if ((pin_config->IOType == DO) || (pin_config->IOType == DI))
                {
                    int pin = pin_config->pin;
                    if ((sf->pin_info[pin].mode == FIRMATA_MODE_OUTPUT) || (sf->pin_info[pin].mode == FIRMATA_MODE_INPUT))
                    {
                        if (sf->pin_info[pin].value == 1)
                        {
                            changed = changed || (sqp.getState() != ISS_ON);
                            sqp.setState(ISS_ON);
                            n_on++;
                        }
                        else
                        {
                            changed = changed || (sqp.getState() != ISS_OFF);
                            sqp.setState(ISS_OFF);
                        }
                    }
                }
            }
            if (changed)
            {
                if (svp.getRule() == ISR_1OFMANY) // make sure that 1 switch is on
                {
                    for (auto &sqp: svp)
                    {

                        if ((IO *)sqp.getAux() != nullptr)
                            continue;

                        if (n_on > 0)
                        {
                            sqp.setState(ISS_OFF);
                        }
                        else
                        {
                            sqp.setState(ISS_ON);
                            n_on++;
                        }
                    }
                }
                svp.apply();
            }
        }

        //ANALOG
        if (type == INDI_NUMBER)
        {
            bool changed = false;
            auto nvp = getNumber(name);

            if (nvp.getNumber()->getAux() != (void *)indiduino_id)
                continue;

            for (auto &eqp: nvp)
            {
                IO *pin_config = (IO *)eqp.getAux();
                if (pin_config == nullptr)
                    continue;

                if (pin_config->IOType == AI)
                {
                    int pin = pin_config->pin;
                    if (sf->pin_info[pin].mode == FIRMATA_MODE_ANALOG)
                    {
                        double new_value = pin_config->MulScale * (double)(sf->pin_info[pin].value) + pin_config->AddScale;
                        changed = changed || (eqp.getValue() != new_value);
                        eqp.setValue(new_value);
                        //LOGF_DEBUG("%f",eqp->value);
                    }
                }
                if (pin_config->IOType == AO) // read back ANALOG OUTPUT values as reported by the board (FIRMATA_PIN_STATE_RESPONSE)
                {
                    int pin = pin_config->pin;
                    if (sf->pin_info[pin].mode == FIRMATA_MODE_PWM)
                    {
                        double new_value = ((double)(sf->pin_info[pin].value) - pin_config->AddScale) / pin_config->MulScale;
                        changed = changed || (eqp.getValue() != new_value);
                        eqp.setValue(new_value);
                        //LOGF_DEBUG("%f",eqp->value);
                    }
                }
            }
            if (changed) nvp.apply();
        }

        //TEXT
        if (type == INDI_TEXT)
        {
            auto tvp = getText(name);
            if (tvp.getText()->getAux() != (void *)indiduino_id)
                continue;

            for (auto &eqp: tvp)
            {
                if (eqp.getAux() == nullptr) continue;
                if (strcmp(eqp.getText(), (const char*)eqp.getAux()) != 0)
                {
                    eqp.setText((const char*)eqp.getAux());
                    //LOGF_DEBUG("%s.%s TEXT: %s ",tvp->name,eqp->name,eqp->text);
                    tvp.apply();
                }
            }
        }
    }
    // START: Switch of for debugging!
    time_t sec_since_reply = sf->secondsSinceVersionReply();
    time_t max_delay = static_cast<time_t>(5*getCurrentPollingPeriod() < 30000 ? 30 : 5*getCurrentPollingPeriod()/1000);
    if (sec_since_reply > max_delay)
    {
        LOGF_ERROR("No reply from the device for %d secs, disconnecting", max_delay);
        setConnected(false, IPS_OK);
        delete sf;
        sf = NULL;
        Disconnect();

        if (getActiveConnection() == tcpConnection)
        {
            // handle reset of the device
            // serial connection survives but tcp must be reconnected
            bool rc = Connect();
            if (rc)
            {
                // Connection is successful, set it to OK and updateProperties.
                setConnected(true, IPS_OK);
                updateProperties();
            }
            else {
                setConnected(false, IPS_ALERT);
            }
            return;
        }
        setConnected(false, IPS_ALERT);
        return;
    }
    if (sec_since_reply > 10)
    {
        LOG_DEBUG("Sending keepalive message");
        sf->askFirmwareVersion();
    }
    // END: Switch of for debugging!
    SetTimer(getCurrentPollingPeriod());
}

/**************************************************************************************
** Initialize all properties & set default values.
**************************************************************************************/
bool indiduino::initProperties()
{
    // This is the default driver skeleton file location
    // Convention is: drivername_sk_xml
    // Default location is /usr/share/indi

    struct stat st;

    strcpy(skelFileName, DEFAULT_SKELETON_FILE);

    char *skel = getenv("INDISKEL");
    if (skel)
    {
        LOGF_INFO("Building from %s skeleton", skel);
        strcpy(skelFileName, skel);
        buildSkeleton(skel);
    }
    else if (stat(skelFileName, &st) == 0)
    {
        LOGF_INFO("Building from %s skeleton", skelFileName);
        buildSkeleton(skelFileName);
    }
    else
    {
        LOG_WARN(
            "No skeleton file was specified. Set environment variable INDISKEL to the skeleton path and try again.");
    }
    // Optional: Add aux controls for configuration, debug & simulation that get added in the Options tab
    //           of the driver.
    //addAuxControls();

    /* Switch only for testing
    IUFillSwitch(&TestStateS[0], "On", "", ISS_OFF);
    IUFillSwitch(&TestStateS[1], "Off", "", ISS_OFF);
    IUFillSwitchVector(&TestStateSP, TestStateS, 2, getDeviceName(), "TEST", "Test", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE)
    */

    controller->initProperties();

    DefaultDevice::initProperties();

    serialConnection = new Connection::Serial(this);
    serialConnection->registerHandshake([&]() { return Handshake(); });
    serialConnection->setDefaultBaudRate(Connection::Serial::B_57600);
    // Arduino default port
    serialConnection->setDefaultPort("/dev/ttyACM0");
    registerConnection(serialConnection);

    tcpConnection = new Connection::TCP(this);
    tcpConnection->registerHandshake([&]() { return Handshake(); });
    registerConnection(tcpConnection);

    addDebugControl();
    addPollPeriodControl();
    return true;
}

bool indiduino::Handshake()
{
    if (isSimulation())
    {
        LOGF_INFO("Connected successfuly to simulated %s.", getDeviceName());
        return true;
    }

    int PortFD;
    if (getActiveConnection() == serialConnection)
        PortFD = serialConnection->getPortFD();
    else if (getActiveConnection() == tcpConnection)
        PortFD = tcpConnection->getPortFD();
    else
        return false;

    sf = new Firmata(PortFD);
    if (!sf->portOpen) {
        delete sf;
        sf = NULL;
        return false;
    }

    return true;
}

bool indiduino::updateProperties()
{
    if (isConnected())
    {
        if (!sf)
        {
            LOG_ERROR("Firmata not connected in updateProperties().");
            return false;
        }

        if (sf->initState() != 0)
        {
            LOG_ERROR("Failed to get Arduino state");
            getSwitch("CONNECTION").apply("Fail to get Arduino state");
            delete sf;
            this->serialConnection->Disconnect();
            return false;
        }
        LOG_INFO("Arduino board connected.");
        LOGF_INFO("FIRMATA version:%s", sf->firmata_name);
        getSwitch("CONNECTION").apply("CONNECTED. FIRMATA version:%s", sf->firmata_name);
        if (!setPinModesFromSKEL())
        {
            LOG_ERROR("Failed to map Arduino pins, check skeleton file syntax.");
            getSwitch("CONNECTION").apply("Failed to map Arduino pins, check skeleton file syntax.");
            delete sf;
            this->serialConnection->Disconnect();
            return false;
        }


        // Mapping the controller according to the properties previously read from the XML file
        // We only map controls for pin of type AO and SERVO
        for (int numiopin = 0; numiopin < MAX_IO_PIN; numiopin++)
        {
            if (iopin[numiopin].IOType == SERVO)
            {
                if (iopin[numiopin].SwitchButton)
                {
                    char buffer[33];
                    sprintf(buffer, "%d", numiopin);
                    controller->mapController(buffer, iopin[numiopin].defVectorName,
                                              INDI::Controller::CONTROLLER_BUTTON, iopin[numiopin].SwitchButton);
                }
            }
            else if (iopin[numiopin].IOType == AO)
            {
                if (iopin[numiopin].UpButton && iopin[numiopin].DownButton)
                {
                    char upBuffer[33];
                    sprintf(upBuffer, "%d", numiopin);
                    // To distinguish the up button from the down button, we add MAX_IO_PIN to the message
                    char downBuffer[33];
                    sprintf(downBuffer, "%d", numiopin + MAX_IO_PIN);
                    controller->mapController(upBuffer, iopin[numiopin].defVectorName,
                                              INDI::Controller::CONTROLLER_BUTTON, iopin[numiopin].UpButton);
                    controller->mapController(downBuffer, iopin[numiopin].defVectorName,
                                              INDI::Controller::CONTROLLER_BUTTON, iopin[numiopin].DownButton);
                }
            }
        }
        // defineProperty(&TestStateSP); Switch only for testing
    }
    else
    {
        delete sf;
        sf = NULL;
        LOG_INFO("Arduino board disconnected.");
    }
    controller->updateProperties();
    return true;
}

/**************************************************************************************
** Define Basic properties to clients.
***************************************************************************************/
void indiduino::ISGetProperties(const char *dev)
{
    static int configLoaded = 0;

    // Ask the default driver first to send properties.
    INDI::DefaultDevice::ISGetProperties(dev);

    configLoaded = 1;
    INDI_UNUSED(configLoaded);

    controller->ISGetProperties(dev);
}

/**************************************************************************************
** Process Text properties
***************************************************************************************/
bool indiduino::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    // Ignore if not ours
    if (strcmp(dev, getDeviceName()))
        return false;

    controller->ISNewText(dev, name, texts, names, n);

    return DefaultDevice::ISNewText(dev, name, texts, names, n);

}

/**************************************************************************************
**
***************************************************************************************/
bool indiduino::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    // Ignore if not ours
    if (strcmp(dev, getDeviceName()))
        return false;

    auto nvp = getNumber(name);
    if (!nvp)
        return false;

    if (isConnected() == false)
    {
        nvp.setState(IPS_ALERT);
        nvp.apply("Cannot change property while device is disconnected.");
        return false;
    }

    bool change = false;
    for (int i = 0; i < n; i++)
    {
        auto eqp = nvp.findWidgetByName(names[i]);

        if (!eqp)
            return false;

        IO *pin_config = (IO *)eqp->aux0;
        if (pin_config == nullptr)
            continue;
        if ((pin_config->IOType == AO) || (pin_config->IOType == SERVO))
        {
            int pin = pin_config->pin;
            nvp.update(values, names, n);
            LOGF_DEBUG("Setting output %s.%s on pin %u to %f", nvp.getName(), eqp->getName(), pin, eqp->getValue());
            sf->setPwmPin(pin, (int)(pin_config->MulScale * eqp->getValue() + pin_config->AddScale));
            nvp.apply("%s.%s change to %f", nvp.getName(), eqp->getName(), eqp->getValue());
            nvp.setState(IPS_IDLE);
            change = true;
        }
        if (pin_config->IOType == AI)
        {
            nvp.update(values, names, n);
            nvp.setState(IPS_IDLE);
            change = true;
        }
    }

    if (change)
    {
        nvp.apply();
        return true;
    }
    else
    {
        //  Nothing changed, so pass it to the parent
        return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
    }
}

/**************************************************************************************
**
***************************************************************************************/
bool indiduino::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    /*for (int i = 0; i < n; i++)
    {
        if (states[i] == ISS_ON)
            LOGF_DEBUG("State %d is on", i);
        else if (states[i] == ISS_OFF)
            LOGF_DEBUG("State %d is off", i);
    }*/
    // ignore if not ours //
    if (strcmp(dev, getDeviceName()))
        return false;

    if (INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n) == true)
        return true;

    auto svp = getSwitch(name);

    if (isConnected() == false)
    {
        svp.setState(IPS_ALERT);
        svp.apply("Cannot change property while device is disconnected.");
        return false;
    }

    if (!svp)
        return false;

    //for (int i = 0; i < svp->nsp; i++)
    for (auto &sqp: svp)
    {
        //ISwitch *sqp   = &svp->sp[i];
        IO *pin_config = (IO *)sqp.getAux();
        if (pin_config == nullptr)
            continue;
        if (pin_config->IOType == DO)
        {
            int pin = pin_config->pin;
            svp.update(states, names, n);
            svp.setState(IPS_ALERT);
            if (sqp.getState() == ISS_ON)
            {
                LOGF_DEBUG("Switching ON %s.%s on pin %u", svp.getName(), sqp.getName(), pin);
                if (sf->writeDigitalPin(pin, ARDUINO_HIGH) == 0)
                {
                    //IDSetSwitch(svp, "%s.%s ON", svp->name, sqp->name); Seems not to work anymore!
                    sf->pin_info[pin].value = 1; // Set Standard Firmata record, so time loop can set correct switch state!
                    svp.setState(IPS_OK);
                }
            }
            else
            {
                LOGF_DEBUG("Switching OFF %s.%s on pin %u", svp.getName(), sqp.getName(), pin);
                if (sf->writeDigitalPin(pin, ARDUINO_LOW) ==0)
                {
                    //IDSetSwitch(svp, "%s.%s OFF", svp->name, sqp->name); Seems not to work anymore!
                    sf->pin_info[pin].value = 0; // Set Standard Firmata record, so time loop can set correct switch state!
                    svp.setState(IPS_OK);
                }
            }
            svp.apply();
        }
        else if (pin_config->IOType == SERVO)
        {
            int pin = pin_config->pin;
            svp.update(states, names, n);
            if (sqp.getState() == ISS_ON)
            {
                LOGF_DEBUG("Switching ON %s.%s on pin %u", svp.getName(), sqp.getName(), pin);
                sf->setPwmPin(pin, (int)pin_config->OnAngle);
                svp.apply("%s.%s ON", svp.getName(), sqp.getName());
            }
            else
            {
                LOGF_DEBUG("Switching OFF %s.%s on pin %u", svp.getName(), sqp.getName(), pin);
                sf->setPwmPin(pin, (int)pin_config->OffAngle);
                svp.apply("%s.%s OFF", svp.getName(), sqp.getName());
            }
        }
    }
    /* Switch only for testing
    if (!strcmp(name, TestStateSP.name))
    {
        if (IUUpdateSwitch(&TestStateSP, states, names, n) < 0)
        {
            TestStateSP.s = IPS_ALERT;
            return false;
        }
        // IUFindOnSwitchIndex(&TestStateSP);
        else
            TestStateSP.s = IPS_OK;

        IDSetSwitch(&TestStateSP, nullptr);
        return true;
    }
    */
    controller->ISNewSwitch(dev, name, states, names, n);

    svp.update(states, names, n);
    return true;
}

bool indiduino::ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[],
                          char *formats[], char *names[], int n)
{
    if (strcmp(dev, getDeviceName()))
        return false;

    auto bvp = getBLOB(name);

    if (!bvp)
        return false;

    if (isConnected() == false)
    {
        bvp.setState(IPS_ALERT);
        bvp.apply("Cannot change property while device is disconnected.");
        return false;
    }

    if (bvp.isNameMatch("BLOB Test"))
    {
        bvp.update(sizes, blobsizes, blobs, formats, names, n);

        auto bp = bvp.findWidgetByName(names[0]);

        if (!bp)
            return false;

        LOGF_DEBUG("Recieved BLOB with name %s, format %s, and size %d, and bloblen %d",
              bp->getName(), bp->getFormat(), bp->getSize(), bp->getBlobLen());

        char *blobBuffer = new char[bp->bloblen + 1];
        strncpy(blobBuffer, ((char *)bp->blob), bp->bloblen);
        blobBuffer[bp->bloblen] = '\0';

        LOGF_DEBUG("BLOB Content:\n##################################\n%s\n##################################",
              blobBuffer);

        delete[] blobBuffer;
    }

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
const char *indiduino::getDefaultName()
{
    return "Arduino";
}

bool indiduino::setPinModesFromSKEL()
{
    char errmsg[MAXRBUF];
    FILE *fp       = nullptr;
    LilXML *lp     = newLilXML();
    XMLEle *fproot = nullptr;
    XMLEle *ep = nullptr, *ioep = nullptr, *xmlp = nullptr;
    int numiopin = 0;

    fp = fopen(skelFileName, "r");

    if (fp == nullptr)
    {
        LOGF_ERROR("Unable to build skeleton. Error loading file %s: %s", skelFileName, strerror(errno));
        return false;
    }

    fproot = readXMLFile(fp, lp, errmsg);

    if (fproot == nullptr)
    {
        LOGF_ERROR("Unable to parse skeleton XML: %s", errmsg);
        return false;
    }

    LOG_INFO("Setting pins behaviour from <indiduino> tags");

    for (const auto &it: *getProperties())
    {
        const char *name = it.getName();
        INDI_PROPERTY_TYPE type = it.getType();

        if (ep == nullptr)
        {
            ep = nextXMLEle(fproot, 1);
        }
        else
        {
            ep = nextXMLEle(fproot, 0);
        }
        if (ep == nullptr)
        {
            break;
        }
        ioep = nullptr;
        if (type == INDI_SWITCH)
        {
            auto svp = getSwitch(name);
            ioep = nullptr;
            for (auto &sqp: svp)
            {
                if (ioep == nullptr)
                {
                    ioep = nextXMLEle(ep, 1);
                }
                else
                {
                    ioep = nextXMLEle(ep, 0);
                }
                xmlp = findXMLEle(ioep, "indiduino");
                if (xmlp != nullptr)
                {
                    if (!readInduinoXml(xmlp, numiopin))
                    {
                        LOG_ERROR("Malforme <indiduino> XML");
                        return false;
                    }
                    svp.getSwitch()->setAux((void *)indiduino_id);
                    sqp.setAux((void *)&iopin[numiopin]);
                    iopin[numiopin].defVectorName = svp.getName();
                    iopin[numiopin].defName       = sqp.getName();
                    int pin                       = iopin[numiopin].pin;
                    if (iopin[numiopin].IOType == DO)
                    {
                        LOGF_DEBUG("%s.%s  pin %u set as DIGITAL OUTPUT", svp.getName(), sqp.getName(), pin);
                        sf->setPinMode(pin, FIRMATA_MODE_OUTPUT);
                    }
                    else if (iopin[numiopin].IOType == DI)
                    {
                        LOGF_DEBUG("%s.%s  pin %u set as DIGITAL INPUT", svp.getName(), sqp.getName(), pin);
                        sf->setPinMode(pin, FIRMATA_MODE_INPUT);
                    }
                    else if (iopin[numiopin].IOType == SERVO)
                    {
                        LOGF_DEBUG("%s.%s  pin %u set as SERVO", svp.getName(), sqp.getName(), pin);
                        sf->setPinMode(pin, FIRMATA_MODE_SERVO);
                        // Setting Servo pin to default startup angle
                        sf->setPwmPin(
                            pin, (int)(iopin[numiopin].MulScale * iopin[numiopin].OnAngle + iopin[numiopin].AddScale));
                    }
                    LOGF_DEBUG("numiopin:%u", numiopin);
                    numiopin++;
                }
            }
        }
        if (type == INDI_TEXT)
        {
            auto tvp = getText(name);

            ioep = nullptr;
            for (auto &tqp: tvp)
            {
                if (ioep == nullptr)
                {
                    ioep = nextXMLEle(ep, 1);
                }
                else
                {
                    ioep = nextXMLEle(ep, 0);
                }
                xmlp = findXMLEle(ioep, "indiduino");
                if (xmlp != nullptr)
                {
                    if (!readInduinoXml(xmlp, 0))
                    {
                        LOG_ERROR("Malforme <indiduino> XML");
                        return false;
                    }
                    tvp.getText()->setAux((void *)indiduino_id);
                    tqp.setAux((void *)&sf->string_buffer);
                    iopin[numiopin].defVectorName = tvp.getName();
                    iopin[numiopin].defName       = tqp.getName();
                    LOGF_DEBUG("%s.%s ARDUINO TEXT", tvp.getName(), tqp.getName());
                    LOGF_DEBUG("numiopin:%u", numiopin);
                }
            }
        }
        if (type == INDI_LIGHT)
        {
            auto lvp = getLight(name);

            ioep = nullptr;
            for (auto &lqp: lvp)
            {
                if (ioep == nullptr)
                {
                    ioep = nextXMLEle(ep, 1);
                }
                else
                {
                    ioep = nextXMLEle(ep, 0);
                }
                xmlp = findXMLEle(ioep, "indiduino");
                if (xmlp != nullptr)
                {
                    if (!readInduinoXml(xmlp, numiopin))
                    {
                        LOG_ERROR("Malforme <indiduino> XML");
                        return false;
                    }
                    lvp.getLight()->setAux((void *)indiduino_id);
                    lqp.setAux((void *)&iopin[numiopin]);
                    iopin[numiopin].defVectorName = lvp.getName();
                    iopin[numiopin].defName       = lqp.getName();
                    int pin                       = iopin[numiopin].pin;
                    LOGF_DEBUG("%s.%s  pin %u set as DIGITAL INPUT", lvp.getName(), lqp.getName(), pin);
                    sf->setPinMode(pin, FIRMATA_MODE_INPUT);
                    LOGF_DEBUG("numiopin:%u", numiopin);
                    numiopin++;
                }
            }
        }
        if (type == INDI_NUMBER)
        {
            auto nvp = getNumber(name);
            ioep = nullptr;
            //for (int i = 0; i < nvp->nnp; i++)
            for (auto &eqp: nvp)
            {
                if (ioep == nullptr)
                {
                    ioep = nextXMLEle(ep, 1);
                }
                else
                {
                    ioep = nextXMLEle(ep, 0);
                }
                xmlp = findXMLEle(ioep, "indiduino");
                if (xmlp != nullptr)
                {
                    if (!readInduinoXml(xmlp, numiopin))
                    {
                        LOG_ERROR("Malforme <indiduino> XML");
                        return false;
                    }
                    nvp.getNumber()->setAux((void *)indiduino_id); // #PS: workaround (getNumber)
                    eqp.setAux((void *)&iopin[numiopin]);

                    iopin[numiopin].defVectorName = nvp.getName();
                    iopin[numiopin].defName       = eqp.getName();
                    int pin                       = iopin[numiopin].pin;
                    if (iopin[numiopin].IOType == AO)
                    {
                        LOGF_DEBUG("%s.%s  pin %u set as ANALOG OUTPUT", nvp.getName(), eqp.getName(), pin);
                        sf->setPinMode(pin, FIRMATA_MODE_PWM);
                    }
                    else if (iopin[numiopin].IOType == AI)
                    {
                        LOGF_DEBUG("%s.%s  pin %u set as ANALOG INPUT", nvp.getName(), eqp.getName(), pin);
                        sf->setPinMode(pin, FIRMATA_MODE_ANALOG);
                    }
                    else if (iopin[numiopin].IOType == SERVO)
                    {
                        LOGF_DEBUG("%s.%s  pin %u set as SERVO", nvp.getName(), eqp.getName(), pin);
                        sf->setPinMode(pin, FIRMATA_MODE_SERVO);
                    }
                    LOGF_DEBUG("numiopin:%u", numiopin);
                    numiopin++;
                }
            }
        }
    }
    sf->setSamplingInterval(getCurrentPollingPeriod() / 2);
    sf->reportAnalogPorts(1);
    sf->reportDigitalPorts(1);
    return true;
}

bool indiduino::readInduinoXml(XMLEle *ioep, int npin)
{
    char *propertyTag;
    int pin;

    if (ioep == nullptr)
        return false;

    propertyTag = tagXMLEle(parentXMLEle(ioep));

    if (!strcmp(propertyTag, "defSwitch") || !strcmp(propertyTag, "defLight") || !strcmp(propertyTag, "defNumber"))
    {
        pin = atoi(findXMLAttValu(ioep, "pin"));

        if (pin >= 0 && pin < MAX_IO_PIN)
        { //19 hardware pins.
            iopin[npin].pin = pin;
        }
        else
        {
            LOG_ERROR("induino: pin number is required. Check pin attrib value (0-127)");
            return false;
        }

        if (!strcmp(propertyTag, "defSwitch"))
        {
            if (!strcmp(findXMLAttValu(ioep, "type"), "servo"))
            {
                iopin[npin].IOType = SERVO;
                if (strcmp(findXMLAttValu(ioep, "onangle"), ""))
                {
                    iopin[npin].OnAngle = atof(findXMLAttValu(ioep, "onangle"));
                }
                else
                {
                    iopin[npin].OnAngle = 150;
                }
                if (strcmp(findXMLAttValu(ioep, "offangle"), ""))
                {
                    iopin[npin].OffAngle = atof(findXMLAttValu(ioep, "offangle"));
                }
                else
                {
                    iopin[npin].OffAngle = 10;
                }
                if (strcmp(findXMLAttValu(ioep, "button"), ""))
                {
                    iopin[npin].SwitchButton = const_cast<char *>(findXMLAttValu(ioep, "button"));
                    LOGF_DEBUG("found button %s", iopin[npin].SwitchButton);
                }
            }
            else if (!strcmp(findXMLAttValu(ioep, "type"), "input"))
            {
                iopin[npin].IOType = DI;
            }
            else
            {
                iopin[npin].IOType = DO;
            }
        }

        if (!strcmp(propertyTag, "defLight"))
        {
            iopin[npin].IOType = DI;
        }

        if (!strcmp(propertyTag, "defNumber"))
        {
            if (strcmp(findXMLAttValu(ioep, "mul"), ""))
            {
                iopin[npin].MulScale = atof(findXMLAttValu(ioep, "mul"));
            }
            else
            {
                iopin[npin].MulScale = 1;
            }
            if (strcmp(findXMLAttValu(ioep, "add"), ""))
            {
                iopin[npin].AddScale = atof(findXMLAttValu(ioep, "add"));
            }
            else
            {
                iopin[npin].AddScale = 0;
            }
            if (!strcmp(findXMLAttValu(ioep, "type"), "output"))
            {
                iopin[npin].IOType = AO;
            }
            else if (!strcmp(findXMLAttValu(ioep, "type"), "input"))
            {
                iopin[npin].IOType = AI;
            }
            else if (!strcmp(findXMLAttValu(ioep, "type"), "servo"))
            {
                iopin[npin].IOType = SERVO;
            }
            else
            {
                LOG_ERROR("induino: Setting type (input or output) is required for analogs");
                return false;
            }
            if (strcmp(findXMLAttValu(ioep, "downbutton"), ""))
            {
                iopin[npin].DownButton = const_cast<char *>(findXMLAttValu(ioep, "downbutton"));
            }
            if (strcmp(findXMLAttValu(ioep, "upbutton"), ""))
            {
                iopin[npin].UpButton = const_cast<char *>(findXMLAttValu(ioep, "upbutton"));
            }
            if (strcmp(findXMLAttValu(ioep, "buttonincvalue"), ""))
            {
                iopin[npin].buttonIncValue = atof(findXMLAttValu(ioep, "buttonincvalue"));
            }
            else
            {
                iopin[npin].buttonIncValue = 50;
            }
        }

        if (false)
        {
            LOGF_DEBUG("arduino IO Match:Property:(%s)", findXMLAttValu(parentXMLEle(ioep), "name"));
            LOGF_DEBUG("arduino IO pin:(%u)", iopin[npin].pin);
            LOGF_DEBUG("arduino IO type:(%u)", iopin[npin].IOType);
            LOGF_DEBUG("arduino IO scale: mul:%g add:%g", iopin[npin].MulScale, iopin[npin].AddScale);
        }
    }
    return true;
}

void indiduino::joystickHelper(const char *joystick_n, double mag, double angle, void *context)
{
    static_cast<indiduino *>(context)->processJoystick(joystick_n, mag, angle);
}

void indiduino::buttonHelper(const char *button_n, ISState state, void *context)
{
    static_cast<indiduino *>(context)->processButton(button_n, state);
}

void indiduino::axisHelper(const char *axis_n, double value, void *context)
{
    static_cast<indiduino *>(context)->processAxis(axis_n, value);
}

void indiduino::processAxis(const char *axis_n, double value)
{
    INDI_UNUSED(axis_n);
    INDI_UNUSED(value);
    // TO BE IMPLEMENTED
}

void indiduino::processJoystick(const char *joystick_n, double mag, double angle)
{
    INDI_UNUSED(joystick_n);
    INDI_UNUSED(mag);
    INDI_UNUSED(angle);
    // TO BE IMPLEMENTED
}

void indiduino::processButton(const char *button_n, ISState state)
{
    //ignore OFF
    if (state == ISS_OFF)
        return;

    int numiopin;
    numiopin = atoi(button_n);

    bool isDownAO = false;
    // Recognizing a shifted numiopin means it is a button to decrease the value of a AO
    if (numiopin >= MAX_IO_PIN)
    {
        isDownAO = true;
        numiopin = numiopin - MAX_IO_PIN;
    }

    if (iopin[numiopin].IOType == AO)
    {
        const char *names[1]  = { iopin[numiopin].defName };
        const char *name      = iopin[numiopin].defVectorName;
        auto nvp        = getNumber(name);
        auto eqp        = nvp.findWidgetByName(names[0]);
        if (isDownAO)
        {
            double values[1] = { eqp->getValue() - iopin[numiopin].buttonIncValue };
            ISNewNumber(getDeviceName(), name, values, const_cast<char **>(names), 1);
        }
        else
        {
            double values[1] = { eqp->getValue() + iopin[numiopin].buttonIncValue };
            ISNewNumber(getDeviceName(), name, values, const_cast<char **>(names), 1);
        }
    }
    else if (iopin[numiopin].IOType == SERVO)
    {
        auto svp = getSwitch(iopin[numiopin].defVectorName);
        // Only considering the first switch, because the servo switches must be configured with only one switch
        const char *names[1] = { iopin[numiopin].defName };
        if (svp[0].getState() == ISS_ON)
        {
            ISState states[1] = { ISS_OFF };
            ISNewSwitch(getDeviceName(), iopin[numiopin].defVectorName, states, const_cast<char **>(names), 1);
        }
        else
        {
            ISState states[1] = { ISS_ON };
            ISNewSwitch(getDeviceName(), iopin[numiopin].defVectorName, states, const_cast<char **>(names), 1);
        }
    }
}

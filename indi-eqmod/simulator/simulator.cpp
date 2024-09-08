
#include "simulator.h"

#include <string.h>

#ifndef INDI_PROPERTY_HAS_COMPARISON
static bool operator!=(INDI::Property lhs, INDI::Property rhs)
{
    return lhs.getProperty() != rhs.getProperty();
}
#endif

EQModSimulator::EQModSimulator(INDI::Telescope *t)
{
    telescope = t;
}

void EQModSimulator::Connect()
{
    auto sw  = SimModeSP.findOnSwitch();
    sksim    = new SkywatcherSimulator();

    if (sw->isNameMatch("SIM_EQ6"))
    {
        sksim->setupVersion("020300");
        sksim->setupRA(180, 47, 12, 200, 64, 2);
        sksim->setupDE(180, 47, 12, 200, 64, 2);
        return;
    }

    if (sw->isNameMatch("SIM_HEQ5"))
    {
        sksim->setupVersion("020301");
        sksim->setupRA(135, 47, 9, 200, 64, 2);
        sksim->setupDE(135, 47, 9, 200, 64, 2);
        return;
    }

    if (sw->isNameMatch("SIM_NEQ5"))
    {
        sksim->setupVersion("020302");
        sksim->setupRA(144, 44, 9, 200, 32, 2);
        sksim->setupDE(144, 44, 9, 200, 32, 2);
        return;
    }

    if (sw->isNameMatch("SIM_NEQ3"))
    {
        sksim->setupVersion("020303");
        sksim->setupRA(130, 55, 10, 200, 32, 2);
        sksim->setupDE(130, 55, 10, 200, 32, 2);
        return;
    }

    if (sw->isNameMatch("SIM_GEEHALEL"))
    {
        sksim->setupVersion("0203F0");
        sksim->setupRA(144, 60, 15, 400, 8, 1);
        sksim->setupDE(144, 60, 10, 400, 8, 1);
        return;
    }

    if (sw->isNameMatch("SIM_CUSTOM"))
    {
        double teeth, num, den, steps, microsteps, highspeed;
        auto hssw = SimHighSpeedSP.findOnSwitch();

        sksim->setupVersion(SimMCVersionTP.findWidgetByName("SIM_MCPHRASE")->getText());
        teeth      = SimWormNP.findWidgetByName("RA_TEETH")->getValue();
        num        = SimRatioNP.findWidgetByName("RA_RATIO_NUM")->getValue();
        den        = SimRatioNP.findWidgetByName("RA_RATIO_DEN")->getValue();
        steps      = SimMotorNP.findWidgetByName("RA_MOTOR_STEPS")->getValue();
        microsteps = SimMotorNP.findWidgetByName("RA_MOTOR_USTEPS")->getValue();
        highspeed  = 1;

        if (hssw->isNameMatch("SIM_HALFSTEP"))
            highspeed = 2;

        sksim->setupRA(teeth, num, den, steps, microsteps, highspeed);

        teeth      = SimWormNP.findWidgetByName("DE_TEETH")->getValue();
        num        = SimRatioNP.findWidgetByName("DE_RATIO_NUM")->getValue();
        den        = SimRatioNP.findWidgetByName("DE_RATIO_DEN")->getValue();
        steps      = SimMotorNP.findWidgetByName("DE_MOTOR_STEPS")->getValue();
        microsteps = SimMotorNP.findWidgetByName("DE_MOTOR_USTEPS")->getValue();
        sksim->setupDE(teeth, num, den, steps, microsteps, highspeed);

        return;
    }
}

void EQModSimulator::receive_cmd(const char *cmd, int *received)
{
    // *received=0;
    if (sksim)
        sksim->process_command(cmd, received);
}

void EQModSimulator::send_reply(char *buf, int *sent)
{
    if (sksim)
        sksim->get_reply(buf, sent);
    //strncpy(buf,"=\r", 2);
    //*sent=2;
}

bool EQModSimulator::initProperties()
{
    telescope->buildSkeleton("indi_eqmod_simulator_sk.xml");

    SimWormNP      = telescope->getNumber("SIMULATORWORM");
    SimRatioNP     = telescope->getNumber("SIMULATORRATIO");
    SimMotorNP     = telescope->getNumber("SIMULATORMOTOR");
    SimModeSP      = telescope->getSwitch("SIMULATORMODE");
    SimHighSpeedSP = telescope->getSwitch("SIMULATORHIGHSPEED");
    SimMCVersionTP = telescope->getText("SIMULATORMCVERSION");

    return true;
}

bool EQModSimulator::updateProperties(bool enable)
{
    if (enable)
    {
        telescope->defineProperty(SimModeSP);
        telescope->defineProperty(SimWormNP);
        telescope->defineProperty(SimRatioNP);
        telescope->defineProperty(SimMotorNP);
        telescope->defineProperty(SimHighSpeedSP);
        telescope->defineProperty(SimMCVersionTP);

        defined = true;
        /*
        AlignDataFileTP=telescope->getText("ALIGNDATAFILE");
        AlignDataBP=telescope->getBLOB("ALIGNDATA");
        */
    }
    else if (defined)
    {
        telescope->deleteProperty(SimModeSP);
        telescope->deleteProperty(SimWormNP);
        telescope->deleteProperty(SimRatioNP);
        telescope->deleteProperty(SimMotorNP);
        telescope->deleteProperty(SimHighSpeedSP);
        telescope->deleteProperty(SimMCVersionTP);
    }

    return true;
}

bool EQModSimulator::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    //  first check if it's for our device

    if (strcmp(dev, telescope->getDeviceName()) == 0)
    {
        auto nvp = telescope->getNumber(name);
        if (nvp != SimWormNP && nvp != SimRatioNP && nvp != SimMotorNP)
            return false;
        if (telescope->isConnected())
        {
            DEBUGDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_WARNING,
                        "Can not change simulation settings when mount is already connected");
            return false;
        }

        nvp.setState(IPS_OK);
        nvp.update(values, names, n);
        nvp.apply();
        return true;
    }
    return false;
}

bool EQModSimulator::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    //  first check if it's for our device

    if (strcmp(dev, telescope->getDeviceName()) == 0)
    {
        auto svp = telescope->getSwitch(name);
        if ((svp != SimModeSP) && (svp != SimHighSpeedSP))
            return false;

        if (telescope->isConnected())
        {
            DEBUGDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_WARNING,
                        "Can not change simulation settings when mount is already connected");
            return false;
        }

        svp.setState(IPS_OK);
        svp.update(states, names, n);
        svp.apply();
        return true;
    }
    return false;
}

bool EQModSimulator::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    //  first check if it's for our device

    if (strcmp(dev, telescope->getDeviceName()) == 0)
    {
        auto tvp = telescope->getText(name);
        if (tvp)
        {
            if (tvp != SimMCVersionTP)
                return false;

            if (telescope->isConnected())
            {
                DEBUGDEVICE(telescope->getDeviceName(), INDI::Logger::DBG_WARNING,
                            "Can not change simulation settings when mount is already connected");
                return false;
            }

            tvp.setState(IPS_OK);
            tvp.update(texts, names, n);
            tvp.apply();
            return true;
        }
    }
    return false;
}

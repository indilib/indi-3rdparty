#if 0
FLI Filter Wheels
INDI Interface for Finger Lakes Instrument Filter Wheels
Copyright (C) 2003 - 2020 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This library is free software;
you can redistribute it and / or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation;
either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY;
without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library;
if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301  USA

#endif

#include <memory>
#include <time.h>
#include <math.h>
#include <sys/time.h>

#include "indidevapi.h"
#include "eventloop.h"

#include "config.h"
#include "fli_cfw.h"

static std::unique_ptr<FLICFW> fliCFW(new FLICFW());

const flidomain_t Domains[] = { FLIDOMAIN_USB, FLIDOMAIN_SERIAL, FLIDOMAIN_PARALLEL_PORT, FLIDOMAIN_INET };

void ISGetProperties(const char *dev)
{
    fliCFW->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    fliCFW->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    fliCFW->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    fliCFW->ISNewNumber(dev, name, values, names, num);
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
    fliCFW->ISSnoopDevice(root);
}

FLICFW::FLICFW()
{
    setVersion(FLI_CCD_VERSION_MAJOR, FLI_CCD_VERSION_MINOR);
}

const char *FLICFW::getDefaultName()
{
    return "FLI CFW";
}

bool FLICFW::initProperties()
{
    INDI::FilterWheel::initProperties();

    IUFillSwitch(&PortS[0], "USB", "USB", ISS_ON);
    IUFillSwitch(&PortS[1], "SERIAL", "Serial", ISS_OFF);
    IUFillSwitch(&PortS[2], "PARALLEL", "Parallel", ISS_OFF);
    IUFillSwitch(&PortS[3], "INET", "INet", ISS_OFF);
    IUFillSwitchVector(&PortSP, PortS, 4, getDeviceName(), "PORTS", "Port", MAIN_CONTROL_TAB, IP_WO, ISR_1OFMANY, 0,
                       IPS_IDLE);

    IUFillText(&FilterInfoT[0], "Model", "", "");
    IUFillText(&FilterInfoT[1], "HW Rev", "", "");
    IUFillText(&FilterInfoT[2], "FW Rev", "", "");
    IUFillTextVector(&FilterInfoTP, FilterInfoT, 3, getDeviceName(), "Model", "Model", "Filter Info", IP_RO, 60, IPS_IDLE);

    IUFillSwitch(&TurnWheelS[0], "FILTER_CW", "+", ISS_OFF);
    IUFillSwitch(&TurnWheelS[1], "FILTER_CCW", "-", ISS_OFF);
    IUFillSwitchVector(&TurnWheelSP, TurnWheelS, 2, getDeviceName(), "FILTER_WHEEL_MOTION", "Turn Wheel", FILTER_TAB, IP_RW,
                       ISR_1OFMANY, 60, IPS_IDLE);
    return true;
}

void FLICFW::ISGetProperties(const char *dev)
{
    INDI::FilterWheel::ISGetProperties(dev);

    defineSwitch(&PortSP);

    addAuxControls();
}

bool FLICFW::updateProperties()
{
    INDI::FilterWheel::updateProperties();

    if (isConnected())
    {
        defineSwitch(&TurnWheelSP);
        defineText(&FilterInfoTP);
    }
    else
    {
        deleteProperty(TurnWheelSP.name);
        deleteProperty(FilterInfoTP.name);
    }

    return true;
}

bool FLICFW::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (strcmp(dev, getDeviceName()) == 0)
    {
        // Ports
        if (!strcmp(name, PortSP.name))
        {
            if (IUUpdateSwitch(&PortSP, states, names, n) < 0)
                return false;

            PortSP.s = IPS_OK;
            IDSetSwitch(&PortSP, nullptr);
            return true;
        }

        // Turn Wheel
        if (!strcmp(name, TurnWheelSP.name))
        {
            if (IUUpdateSwitch(&TurnWheelSP, states, names, n) < 0)
                return false;
            turnWheel();
            return true;
        }
    }

    return INDI::FilterWheel::ISNewSwitch(dev, name, states, names, n);
}

bool FLICFW::Connect()
{
    LOG_INFO("Connecting to FLI CFW...");

    int portSwitchIndex = IUFindOnSwitchIndex(&PortSP);

    if (findFLICFW(Domains[portSwitchIndex]) == false)
    {
        LOG_ERROR("Error: no filter wheels were detected.");
        return false;
    }

    int errorCode = 0;

    if ((errorCode = FLIOpen(&fli_dev, FLIFilter.name, FLIDEVICE_FILTERWHEEL | FLIFilter.domain)))
    {
        LOGF_ERROR("Error: FLIOpen() failed: %s.", strerror(-errorCode));
        return false;
    }

    LOG_INFO("Filter wheel is online. Retrieving basic data.");

    setupParams();

    return true;
}

bool FLICFW::Disconnect()
{
    int errorCode = 0;

    if ((errorCode = FLIClose(fli_dev)))
    {
        LOGF_ERROR("Error: FLIClose() failed: %s.", strerror(-errorCode));
        return false;
    }

    return true;
}

bool FLICFW::setupParams()
{
    int errorCode = 0;

    char hw_rev[16] = {0}, fw_rev[16] = {0};

    ////////////////////////////
    // 1. Get Filter wheels Model
    ////////////////////////////
    if ((errorCode = FLIGetModel(fli_dev, FLIFilter.model, MAXINDINAME)))
    {
        LOGF_ERROR("FLIGetModel() failed: %s.", strerror(-errorCode));
        return false;
    }

    IUSaveText(&FilterInfoT[0], FLIFilter.model);

    ///////////////////////////
    // 2. Get Hardware revision
    ///////////////////////////
    if ((errorCode = FLIGetHWRevision(fli_dev, &FLIFilter.HWRevision)))
    {
        LOGF_ERROR("FLIGetHWRevision() failed: %s.", strerror(errorCode));
        return false;
    }

    snprintf(hw_rev, 16, "%ld", FLIFilter.HWRevision);
    IUSaveText(&FilterInfoT[1], hw_rev);

    ///////////////////////////
    // 3. Get Firmware revision
    ///////////////////////////
    if ((errorCode = FLIGetFWRevision(fli_dev, &FLIFilter.FWRevision)))
    {
        IDMessage(getDeviceName(), "FLIGetFWRevision() failed. %s.", strerror(-errorCode));
        return false;
    }

    snprintf(fw_rev, 16, "%ld", FLIFilter.FWRevision);
    IUSaveText(&FilterInfoT[2], fw_rev);

    IDSetText(&FilterInfoTP, nullptr);

    ///////////////////////////
    // 4. Filter position
    ///////////////////////////

    // on first contact fliter wheel reports position -1
    // to avoid wrong number presented in client dialog:
    //SelectFilter(FilterSlotN[0].min);

    if ((errorCode = FLIGetFilterPos(fli_dev, &FLIFilter.raw_pos)))
    {
        LOGF_DEBUG("FLIGetFilterPos() failed. %s.", strerror(-errorCode));
        return false;
    }

    ///////////////////////////
    // 5. filter max limit
    ///////////////////////////
    if ((errorCode = FLIGetFilterCount(fli_dev, &FLIFilter.count)))
    {
        LOGF_ERROR("FLIGetFilterCount() failed: %s.", strerror(-errorCode));
        return false;
    }

    FilterSlotN[0].min   = 1;
    FilterSlotN[0].max   = FLIFilter.count;
    FilterSlotN[0].value = FLIFilter.raw_pos + 1;

    return true;
}

bool FLICFW::SelectFilter(int targetFilter)
{
    int errorCode = 0;
    long filter = targetFilter - 1;

    LOGF_DEBUG("Requested position is %ld", targetFilter);

    if ((errorCode = FLISetFilterPos(fli_dev, filter)))
    {
        LOGF_ERROR("FLIGetFilterCount() failed: %s.", strerror(-errorCode));
        return false;
    }

    FLIFilter.raw_pos = filter;

    SelectFilterDone(targetFilter);

    return true;
}

int FLICFW::QueryFilter()
{
    int errorCode = 0;

    if ((errorCode = FLIGetFilterPos(fli_dev, &FLIFilter.raw_pos)))
    {
        LOGF_ERROR("FLIGetFilterPos() failed: %s.", strerror(-errorCode));
        return false;
    }

    LOGF_DEBUG("Current position: %ld", FLIFilter.raw_pos + 1);

    return FLIFilter.raw_pos + 1;
}

void FLICFW::turnWheel()
{
    long current_filter = FLIFilter.raw_pos + 1;
    if (current_filter > FLIFilter.count)
        current_filter = FLIFilter.count;

    long target_filter = current_filter;

    switch (TurnWheelS[0].s)
    {
        case ISS_ON:
            if (current_filter < FilterSlotN[0].max)
                target_filter = current_filter + 1;
            else
                target_filter = FilterSlotN[0].min;
            break;

        case ISS_OFF:
            if (current_filter > FilterSlotN[0].min)
                target_filter = current_filter - 1;
            else
                target_filter = FilterSlotN[0].max;
            break;
    }

    LOGF_DEBUG("Turning CFW %s from %ld to %ld", (TurnWheelS[0].s == ISS_ON) ? "CW" : "CCW", current_filter, target_filter);

    SelectFilter(target_filter);

    IUResetSwitch(&TurnWheelSP);
    TurnWheelSP.s = IPS_OK;
    IDSetSwitch(&TurnWheelSP, nullptr);
}

bool FLICFW::findFLICFW(flidomain_t domain)
{
    char **names;
    int errorCode = 0;

    if ((errorCode = FLIList(domain | FLIDEVICE_FILTERWHEEL, &names)))
    {
        LOGF_ERROR("FLIList() failed: %s", strerror(-errorCode));
        return false;
    }

    if (names != nullptr && names[0] != nullptr)
    {
        for (int i = 0; names[i] != nullptr; i++)
        {
            for (int j = 0; names[i][j] != '\0'; j++)
                if (names[i][j] == ';')
                {
                    names[i][j] = '\0';
                    break;
                }
        }

        FLIFilter.domain = domain;

        switch (domain)
        {
            case FLIDOMAIN_PARALLEL_PORT:
                FLIFilter.dname = strdup("parallel port");
                break;

            case FLIDOMAIN_USB:
                FLIFilter.dname = strdup("USB");
                break;

            case FLIDOMAIN_SERIAL:
                FLIFilter.dname = strdup("serial");
                break;

            case FLIDOMAIN_INET:
                FLIFilter.dname = strdup("inet");
                break;

            default:
                FLIFilter.dname = strdup("Unknown domain");
        }

        FLIFilter.name = strdup(names[0]);

        if ((errorCode = FLIFreeList(names)))
        {
            LOGF_ERROR("FLIFreeList() failed: %s.", strerror(-errorCode));
            return false;
        }

    }
    else
    {
        if ((errorCode = FLIFreeList(names)))
        {
            LOGF_ERROR("FLIFreeList() failed: %s.", strerror(-errorCode));
            return false;
        }

        return false;
    }

    return true;
}

void FLICFW::debugTriggered(bool enable)
{
    FLISetDebugLevel(nullptr, enable ? FLIDEBUG_INFO : FLIDEBUG_WARN);
}

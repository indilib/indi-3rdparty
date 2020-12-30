/*
    AOK Skywalker driver

    Copyright (C) 2019 T. Schriber

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

#include "lx200aok.h"

#include <cmath>
#include <memory>
#include <cstring>
#include <unistd.h>
#ifndef _WIN32
#include <termios.h>
#endif
#include <libnova/julian_day.h>
#include <libnova/sidereal_time.h>
#include <libnova/transform.h>

#include "config.h"

// Unique pointers
static std::unique_ptr<LX200Skywalker> telescope;

const char *INFO_TAB = "Info";

void ISInit()
{
    static int isInit = 0;

    if (isInit)
        return;

    isInit = 1;
    if (telescope.get() == nullptr)
    {
        LX200Skywalker* myScope = new LX200Skywalker();
        telescope.reset(myScope);
    }
}

void ISGetProperties(const char *dev)
{
    ISInit();
    telescope->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    ISInit();
    telescope->ISNewSwitch(dev, name, states, names, n);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    ISInit();
    telescope->ISNewText(dev, name, texts, names, n);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    ISInit();
    telescope->ISNewNumber(dev, name, values, names, n);
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
    telescope->ISSnoopDevice(root);
}

/**************************************************
*** LX200 Generic Implementation / Constructor
***************************************************/

LX200Skywalker::LX200Skywalker() : LX200Telescope()
{
    LOG_DEBUG(__FUNCTION__);
    setVersion(SKYWALKER_VERSION_MAJOR, SKYWALKER_VERSION_MINOR);

    DBG_SCOPE = INDI::Logger::DBG_DEBUG;

    /* TCS has no location/elevation and no time but we have to define these, so it is possible
     * to store the data and load it to TCS at startup.
     * This way the TCS is able to calculate it's internal "earth model"
     * TELESCOPE_HAS_TIME
     * TELESCOPE_HAS_LOCATION
     * (Better solution: new enum class?)
     */

    setLX200Capability(LX200_HAS_PULSE_GUIDING);

    SetTelescopeCapability(TELESCOPE_CAN_PARK | TELESCOPE_CAN_SYNC | TELESCOPE_CAN_GOTO | TELESCOPE_CAN_ABORT |
                           TELESCOPE_HAS_TRACK_MODE | TELESCOPE_CAN_CONTROL_TRACK | TELESCOPE_HAS_LOCATION |
                           TELESCOPE_HAS_TIME | TELESCOPE_HAS_PIER_SIDE, 4);

}

/**************************************************************************************
**
***************************************************************************************/
const char *LX200Skywalker::getDefaultName()
{
    return "AOK Skywalker";
}


/**************************************************************************************
**
***************************************************************************************/
bool LX200Skywalker::Handshake()
{
    char fwinfo[64] = {0}; // 64 for strcpy
    if (!getFirmwareInfo(fwinfo))
    {
        LOG_ERROR("Communication with telescope failed");
        return false;
    }
    else
    {
        char strinfo[1][64] = {""};
        sscanf(fwinfo, "%*[\"]%64[^\"]", strinfo[0]);
        strcpy(FirmwareVersionT[0].text, strinfo[0]);
        IDSetText(&FirmwareVersionTP, nullptr);
        LOGF_INFO("Handshake ok. Firmware version: %s", strinfo[0]);
        return true;
    }
}


/**************************************************************************************
**
***************************************************************************************/
bool LX200Skywalker::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // tracking state
        if (!strcmp(name, TrackStateSP.name))
        {
            if (IUUpdateSwitch(&TrackStateSP, states, names, n) < 0)
                return false;
            int trackState = IUFindOnSwitchIndex(&TrackStateSP);
            bool result = false;

            if (INDI::Telescope::TrackState != SCOPE_PARKED)
            {
                if ((trackState == TRACK_ON) && (SetTrackEnabled(true)))
                {
                    TrackState = SCOPE_TRACKING; // ALWAYS set status! [cf. ReadScopeStatus() -> Inditelescope::NewRaDec()]
                    result = true;
                }
                else if ((trackState == TRACK_OFF) && SetTrackEnabled(false))
                {
                    TrackState = SCOPE_IDLE; // ALWAYS set status! [cf. ReadScopeStatus() -> Inditelescope::NewRaDec()]
                    result = true;
                }
                else
                    LOG_ERROR("Trackstate undefined");
            }
            else
                LOG_WARN("Mount still parked");

            TrackStateSP.s = result ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&TrackStateSP, nullptr);
            return result;
        }
        // tracking mode
        else if (!strcmp(name, TrackModeSP.name))
        {
            if (IUUpdateSwitch(&TrackModeSP, states, names, n) < 0)
                return false;
            int trackMode = IUFindOnSwitchIndex(&TrackModeSP);
            bool result = false;

            switch (trackMode) // ToDo: Lunar and Custom tracking
            {
                case TRACK_SIDEREAL:
                    LOG_INFO("Sidereal tracking rate selected.");
                    result = SetTrackMode(trackMode);
                    break;
                case TRACK_SOLAR:
                    LOG_INFO("Solar tracking rate selected.");
                    result = SetTrackMode(trackMode);
                    break;
                case TRACK_LUNAR:
                    LOG_INFO("Lunar tracking not implemented.");
                    break;
                case TRACK_CUSTOM:
                    LOG_INFO("Custom tracking not yet implemented.");
                    break;
            }

            TrackModeSP.s = result ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&TrackModeSP, nullptr);
            return result;
        }
        // mount state
        if (!strcmp(name, MountStateSP.name))
        {
            if (IUUpdateSwitch(&MountStateSP, states, names, n) < 0)
                return false;
            int NewMountState = IUFindOnSwitchIndex(&MountStateSP);
            bool result = false;

            if (INDI::Telescope::TrackState != SCOPE_PARKED)
            {
                if ((NewMountState == MOUNT_LOCKED) && SetMountLock(true))
                {
                    CurrentMountState = MOUNT_LOCKED; // ALWAYS set status!
                    result = true;
                }
                else if ((NewMountState == MOUNT_UNLOCKED) && SetMountLock(false))
                {
                    CurrentMountState = MOUNT_UNLOCKED; // ALWAYS set status!
                    result = true;
                }
                else
                    LOG_ERROR("Mountlock undefined");
            }
            else
                LOG_WARN("Mount still parked.");

            MountStateSP.s = result ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&MountStateSP, nullptr);
            return result;
        }
        if (!strcmp(name, ParkSP.name))
        {
            if (LX200Telescope::ISNewSwitch(dev, name, states, names, n))
            {
                ParkSP.s = IPS_OK; //INDI::Telescope::SetParked(false) sets IPS_IDLE (!?)
                IDSetSwitch(&ParkSP, nullptr);
                return true;
            }
            else
                return false;
        }
        if (!strcmp(name, ParkOptionSP.name))
        {
            IUUpdateSwitch(&ParkOptionSP, states, names, n);
            int index = IUFindOnSwitchIndex(&ParkOptionSP);
            if (index == -1)
                return false;
            IUResetSwitch(&ParkOptionSP);
            if ((TrackState != SCOPE_IDLE && TrackState != SCOPE_TRACKING) || MovementNSSP.s == IPS_BUSY ||
                    MovementWESP.s == IPS_BUSY)
            {
                LOG_WARN("Mount slewing or already parked...");
                ParkOptionSP.s = IPS_ALERT;
                IDSetSwitch(&ParkOptionSP, nullptr);
                return false;
            }
            bool result = false;
            if (index == PARK_WRITE_DATA)
            {
                if (SavePark())
                {
                    SetParked(true);
                    if (Disconnect())
                    {
                        setConnected(false, IPS_IDLE);
                        updateProperties();
                    }
                    LOG_INFO("Controller is rebooting! Please reconnect.");
                    result = true;
                }
            }
            else
                result = INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
            return result;
        }

    }

    //  Nobody has claimed this until now, so pass it to the parent
    return LX200Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool LX200Skywalker::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        /* Unpark'ing automatically
        if (!strcmp(name, LocationNP.name))
            if (LX200Telescope::ISNewNumber(dev, name, values, names, n))
            {
                return Unpark();
            }
        */
        if (!strcmp(name, SystemSlewSpeedNP.name))
        {
            int slewSpeed  = round(values[0]);
            bool result  = setSystemSlewSpeed(slewSpeed);

            if(result)
            {
                SystemSlewSpeedP[0].value = static_cast<double>(slewSpeed);
                SystemSlewSpeedNP.s = IPS_OK;
            }
            else
            {
                SystemSlewSpeedNP.s = IPS_ALERT;
            }
            IDSetNumber(&SystemSlewSpeedNP, nullptr);
            return result;
        }
    }

    //  Nobody has claimed this, so pass it to the parent
    return LX200Telescope::ISNewNumber(dev, name, values, names, n);
}



/**************************************************************************************
**
***************************************************************************************/
bool LX200Skywalker::initProperties()
{
    /* Make sure to init parent properties first */
    if (!LX200Telescope::initProperties()) return false;

    // INDI::Telescope has to know which parktype to enable parking options UI
    SetParkDataType(PARK_AZ_ALT); //

    // System Slewspeed
    IUFillNumber(&SystemSlewSpeedP[0], "SLEW_SPEED", "Slewspeed", "%.2f", 0.0, 30.0, 1, 0);
    IUFillNumberVector(&SystemSlewSpeedNP, SystemSlewSpeedP, 1, getDeviceName(), "SLEW_SPEED", "Slewspeed", MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    // Motors Status
    IUFillSwitch(&MountStateS[0], "On", "", ISS_OFF);
    IUFillSwitch(&MountStateS[1], "Off", "", ISS_OFF);
    IUFillSwitchVector(&MountStateSP, MountStateS, 2, getDeviceName(), "Mountlock", "Mount lock", MAIN_CONTROL_TAB, IP_RW, ISR_1OFMANY, 0, IPS_IDLE);

    // Infotab
    IUFillText(&FirmwareVersionT[0], "Firmware", "Version", "123456");
    IUFillTextVector(&FirmwareVersionTP, FirmwareVersionT, 1, getDeviceName(), "Firmware", "Firmware", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // Setting the park position in the controller (with webinterface) evokes a restart of the very same!
    // 4th option "purge" of INDI::Telescope doesn't make any sense here, so it is not displayed
    IUFillSwitch(&ParkOptionS[PARK_CURRENT], "PARK_CURRENT", "Copy", ISS_OFF);
    IUFillSwitch(&ParkOptionS[PARK_DEFAULT], "PARK_DEFAULT", "Read",ISS_OFF);
    IUFillSwitch(&ParkOptionS[PARK_WRITE_DATA], "PARK_WRITE_DATA", "Write", ISS_OFF);
    IUFillSwitchVector(&ParkOptionSP, ParkOptionS, 3, getDeviceName(), "TELESCOPE_PARK_OPTION", "Park Options",
                       SITE_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200Skywalker::updateProperties()
{
    if (! LX200Telescope::updateProperties()) return false;
    if (isConnected())
    {
        // registering here results in display at bottom of tab
        defineSwitch(&MountStateSP);
        defineNumber(&SystemSlewSpeedNP);
        defineText(&FirmwareVersionTP);
    }
    else
    {
        deleteProperty(MountStateSP.name);
        deleteProperty(SystemSlewSpeedNP.name);
        deleteProperty(FirmwareVersionTP.name);
    }

    return true;
}

/**************************************************************************************
**
***************************************************************************************/
bool LX200Skywalker::Connect()
{
    if (! DefaultDevice::Connect())
        return false;
    return true;
}

bool LX200Skywalker::Disconnect()
{
    return DefaultDevice::Disconnect();
}

/**************************************************************************************
**
***************************************************************************************
bool LX200Skywalker::ReadScopeStatus()
{
    return (LX200Telescope::ReadScopeStatus());  // TCS does not :D#! -> ovverride isSlewComplete
}*/

bool LX200Skywalker::isSlewComplete()
{
    char response[TCS_RESPONSE_BUFFER_LENGTH];
    bool result = false;
    if (sendQuery("?#", response)) // Send query is sent only if not tracking (cf. lx200telescope)
    {
        // Slew complete?
        if (*response == '0') // Query response == '0', mount is not slewing (anymore)
        {
            if (TrackState == SCOPE_SLEWING)
            {
                notifyTrackState(SCOPE_TRACKING);
                if ((notifyPierSide()) && (MountLocked())) // Normally lock is set by TCS if slew ends
                {
                    notifyMountLock(true);
                    result = true;
                }
                else
                    LOG_ERROR("Mount could not be locked by TCS!");
            }
            else if (TrackState == SCOPE_PARKING)
            {
                notifyTrackState(SCOPE_PARKED);
                if (SetMountLock(false))
                {
                    notifyMountLock(false);
                    result = true;
                }
                else
                    LOG_ERROR("Mount could not be unlocked by TCS!");
            }
        }
    }
    return result;
}

/**************************************************************************************
**
***************************************************************************************/

void LX200Skywalker::getBasicData()
{
    LOG_DEBUG(__FUNCTION__);
    if (!isSimulation())
    {
        checkLX200EquatorialFormat();

        if (INDI::Telescope::capability & TELESCOPE_CAN_PARK)
        {
            ParkSP.s = IPS_OK;
            IDSetSwitch(&ParkSP, nullptr);
        }

        if (genericCapability & LX200_HAS_ALIGNMENT_TYPE)
            getAlignment();

        if (genericCapability & LX200_HAS_TRACKING_FREQ)
        {
            if (! getTrackFrequency(&TrackFreqN[0].value))
                LOG_ERROR("Failed to get tracking frequency from device.");
            else
                IDSetNumber(&TrackingFreqNP, nullptr);
        }

        int slewSpeed;
        if (getSystemSlewSpeed(&slewSpeed))
        {
            SystemSlewSpeedP[0].value = static_cast<double>(slewSpeed);
            SystemSlewSpeedNP.s = IPS_OK;
        }
        else
        {
            SystemSlewSpeedNP.s = IPS_ALERT;
        }
        IDSetNumber(&SystemSlewSpeedNP, nullptr);

        if (INDI::Telescope::capability & TELESCOPE_HAS_TRACK_MODE)
        {
            int trackMode = IUFindOnSwitchIndex(&TrackModeSP);
            //int modes = sizeof(TelescopeTrackMode); (enum Sidereal, Solar, Lunar, Custom)
            int modes = TRACK_SOLAR; // ToDo: Lunar and Custom tracking
            TrackModeSP.s = (trackMode <= modes) ? IPS_OK : IPS_ALERT;
            IDSetSwitch(&TrackModeSP, nullptr);
        }
        if (InitPark())
        {
            LOG_INFO("Parkdata loaded");
            if (!INDI::Telescope::isParked()) // Mount is unparked and working on connection of the driver!
            {
                    if ((MountLocked()) && (MountTracking())) // default state of working mount
                     {
                        notifyMountLock(true);
                        notifyTrackState(SCOPE_TRACKING);
                        ParkSP.s = IPS_OK;
                        IDSetSwitch(&ParkSP, nullptr);
                        //LOG_INFO("Mount is working");
                     }
                     else
                        LOG_WARN("Mount is unparked but not locked and/or not tracking!");
            }
            else // Mount is parked
            {
                notifyMountLock(MountLocked());
                notifyTrackState(SCOPE_PARKED);
            }
        }
        else
            LOG_INFO("Parkdata load failed");
    }

    //ToDo: collect other fixed data here like Manufacturer, version etc...
    if (genericCapability & LX200_HAS_PULSE_GUIDING)
    {
        UsePulseCmdS[0].s = ISS_ON;
        UsePulseCmdS[1].s = ISS_OFF;
        UsePulseCmdSP.s = IPS_OK;
        usePulseCommand = false; // ALWAYS set status! (cf. ISNewSwitch())
        IDSetSwitch(&UsePulseCmdSP, nullptr);
    }

}

/**************************************************************************************
**
***************************************************************************************/

bool LX200Skywalker::updateLocation(double latitude, double longitude, double elevation)
{
    LOGF_DEBUG("%s Lat:%.3lf Lon:%.3lf", __FUNCTION__, latitude, longitude);
    INDI_UNUSED(elevation); // Elevation only as info

    if (isSimulation())
        return true;

    //    LOGF_DEBUG("Setting site longitude '%lf'", longitude);
    if (!isSimulation() && ! setSiteLongitude(360.0 - longitude))  // Meade defines longitude as 0 to 360 WESTWARD
    {
        LOGF_ERROR("Error setting site longitude %lf", longitude);
        return false;
    }

    if (!isSimulation() && ! setSiteLatitude(latitude))
    {
        LOGF_ERROR("Error setting site latitude %lf", latitude);
        return false;
    }

    char l[32] = {0}, L[32] = {0};
    fs_sexa(l, latitude, 3, 3600);
    fs_sexa(L, longitude, 4, 3600);

    // LOGF_INFO("Site location updated to Lat %.32s - Long %.32s", l, L); Info provided by "inditelescope"

   return true;
}

/**************************************************************************************
**
***************************************************************************************/

bool LX200Skywalker::Park()
{
    if (INDI::Telescope::TrackState == SCOPE_PARKED)  // already parked
    {
        // Important: SetParked() invokes WriteParkData() which saves state and position
        // in ParkData.XML (this means parkstate will be overwritten with true)
        INDI::Telescope::SetParked(true);
        return true;
    }
    else
        return (LX200Telescope::Park());
}

bool LX200Skywalker::UnPark()
{
    char response[TCS_RESPONSE_BUFFER_LENGTH];
    if (sendQuery(":hW#", response, TCS_NOANSWER))
    {
        // Important: SetParked() invokes WriteParkData() which saves state and position
        // in ParkData.XML (this means parkstate is overwritten with false!)
        INDI::Telescope::SetParked(false);
        if ((MountLocked()) && (MountTracking())) // TCS sets Mountlock & Tracking
        {
            notifyMountLock(true);
            // INDI::Telescope::SetParked(false) sets TrackState = SCOPE_IDLE but TCS is tracking
            notifyTrackState(SCOPE_TRACKING);
            // INDI::Telescope::SetParked(false) sets ParkSP.S = IPS_IDLE but mount IS unparked!
            ParkSP.s = IPS_OK;
            IDSetSwitch(&ParkSP, nullptr);
            return SyncDefaultPark();
        }
        else
            return false;
    }
    else
        return false;
}

bool LX200Skywalker::SavePark()
{
    char response[TCS_RESPONSE_BUFFER_LENGTH];
    if (sendQuery(":SP#", response, TCS_NOANSWER))  // Controller sets parkposition and reboots
        return true;
    else
    {
        LOG_ERROR("Controller did not accept 'SetPark'.");
        return false;
    }
}

/********************************************************************************
 * Notifier-Section
 ********************************************************************************/
// Changed from original "set_" to "notify_" because of the logic behind: From
// the controller view we change the viewer (and a copy of the model), not the
// the model itself!
bool LX200Skywalker::notifyPierSide()
{
    char lstat[20] = {0};
    if (getJSONData_Y(5, lstat)) // this is the model!
    {
        int li = std::stoi(lstat);
        li = li & (1 << 7);
        if (li > 0)
            Telescope::setPierSide(INDI::Telescope::PIER_WEST);
        else
            Telescope::setPierSide(INDI::Telescope::PIER_EAST);
        LOGF_INFO("Telescope pointing %s", (li > 0) ? "east" : "west");
        return true;
    }
    else
    {
        Telescope::setPierSide(INDI::Telescope::PIER_UNKNOWN);
        LOG_ERROR("Telescope pointing unknown!");
        return false;
    }
}

void LX200Skywalker::notifyMountLock(bool locked)
{
    if (locked)
    {
        MountStateS[0].s = ISS_ON;
        MountStateS[1].s = ISS_OFF;
        MountStateSP.s = IPS_OK;
        CurrentMountState = MOUNT_LOCKED; // ALWAYS set status!
    }
    else
    {
        MountStateS[0].s = ISS_OFF;
        MountStateS[1].s = ISS_ON;
        MountStateSP.s = IPS_OK;
        CurrentMountState = MOUNT_UNLOCKED; // ALWAYS set status!
    }
    IDSetSwitch(&MountStateSP, nullptr);
}

void LX200Skywalker::notifyTrackState(INDI::Telescope::TelescopeStatus state)
{
    if (state == SCOPE_TRACKING)
    {
        TrackStateS[TRACK_ON].s = ISS_ON;
        TrackStateS[TRACK_OFF].s = ISS_OFF;
        TrackStateSP.s = IPS_OK;
        INDI::Telescope::TrackState = state;
    }
    else
    {
        TrackStateS[TRACK_ON].s = ISS_OFF;
        TrackStateS[TRACK_OFF].s = ISS_ON;
        TrackStateSP.s = IPS_OK;
        INDI::Telescope::TrackState = state;
    }
    IDSetSwitch(&TrackStateSP, nullptr);
}

/*********************************************************************************
 * config file
 *********************************************************************************/

bool LX200Skywalker::saveConfigItems(FILE *fp)
{
    LOG_DEBUG(__FUNCTION__);
    IUSaveConfigText(fp, &SiteNameTP);

    return LX200Telescope::saveConfigItems(fp);
}


/*********************************************************************************
 * Queries
 *********************************************************************************/

/**
 * @brief Send a LX200 query to the communication port and read the result.
 * @param cmd LX200 query
 * @param response answer
 * @return true if the command succeeded, false otherwise
 */
bool LX200Skywalker::sendQuery(const char* cmd, char* response, char end, int wait)
{
    LOGF_DEBUG("%s %s End:%c Wait:%ds", __FUNCTION__, cmd, end, wait);
    response[0] = '\0';
    char lresponse[TCS_RESPONSE_BUFFER_LENGTH];
    lresponse [0] = '\0';
    bool lresult = false;
    if(!transmit(cmd))
    {
        LOGF_ERROR("Command <%s> not transmitted.", cmd);
    }
    else if (wait > TCS_NOANSWER)
    {
        if (receive(lresponse, end, wait))
        {
            strcpy(response, lresponse);
            return true;
        }
    }
    else
        lresult = true;
    return lresult;
}


/*
 * Set the site longitude.
 */
bool LX200Skywalker::setSiteLongitude(double longitude)
{
    LOG_DEBUG(__FUNCTION__);
    int d, m, s;
    char command[32] = {0};

    getSexComponents(longitude, &d, &m, &s);
    snprintf(command, sizeof(command), ":Sg%03d*%02d:%02d#", d, m, s);
    LOGF_DEBUG("Sending set site longitude request '%s'", command);

    char response[TCS_RESPONSE_BUFFER_LENGTH] = {0};
    bool result = sendQuery(command, response);
    // default wait for TCS_TIMEOUT seconds is ok because longitude is only set at start

    return (result);
}


/*
 * Set the site latitude
 */
bool LX200Skywalker::setSiteLatitude(double Lat)
{
    LOG_DEBUG(__FUNCTION__);
    int d, m, s;
    char command[32];

    getSexComponents(Lat, &d, &m, &s);

    snprintf(command, sizeof(command), ":St%+03d*%02d:%02d#", d, m, s);

    LOGF_DEBUG("Sending set site latitude request '%s'", command);

    char response[TCS_RESPONSE_BUFFER_LENGTH] = {0};
    return (sendQuery(command, response));
    // default wait for TCS_TIMEOUT seconds is ok because latitude is only set at start

}

bool LX200Skywalker::getJSONData_gp(int jindex, char *jstr) // preliminary hardcoded  :gp-query
{
    char lresponse[128];
    lresponse [0] = '\0';
    char lcmd[4] = ":gp";
    char end = '}';
    if(!transmit(lcmd))
    {
        LOGF_ERROR("Command <%s> not transmitted.", lcmd);
    }
    if (receive(lresponse, end, 1))
    {
        flush();
    }
    else
    {
        LOG_ERROR("Failed to get JSONData");
        return false;
    }
    char data[3][40] = {"", "", ""};
    int returnCode = sscanf(lresponse, "%*[^[][%40[^\"]%40[^,]%*[,]%40[^]]", data[0], data[1], data[2]);
    if (returnCode < 1)
    {
        LOGF_ERROR("Failed to parse JSONData '%s'.", lresponse);
        return false;
    }
    strcpy(jstr, data[jindex]);
    return true;
}

bool LX200Skywalker::getJSONData_Y(int jindex, char *jstr) // preliminary hardcoded query :Y#-query
{
    char lresponse[128];
    lresponse [0] = '\0';
    char lcmd[4] = ":Y#";
    char end = '}';
    if(!transmit(lcmd))
    {
        LOGF_ERROR("Command <%s> not transmitted.", lcmd);
    }
    if (receive(lresponse, end, 1))
    {
        flush();
    }
    else
    {
        LOG_ERROR("Failed to get JSONData");
        return false;
    }
    char data[6][20] = {"", "", "", "", "", ""};
    int returnCode = sscanf(lresponse, "%20[^,]%*[,]%20[^,]%*[,]%20[^#]%*[#\",]%20[^,]%*[,]%20[^,]%*[,]%20[^,]", data[0], data[1], data[2], data[3], data[4], data[5]);
    if (returnCode < 1)
    {
        LOGF_ERROR("Failed to parse JSONData '%s'.", lresponse);
        return false;
    }
    strcpy(jstr, data[jindex]);
    return true;
}

bool LX200Skywalker::MountLocked()
{
    char lstat[20] = {0};
    if(!getJSONData_gp(2, lstat))
        return false;
    else
    {
        int li = std::stoi(lstat);
        if (li > 0)
            return true;
        else
            return false;
    }
}

bool LX200Skywalker::SetMountLock(bool enable)
{
    // Command set lock on/off  - :hE#
    char response[TCS_RESPONSE_BUFFER_LENGTH] = {0};
    bool retval = false;
    if (enable)
    {
        if (MountLocked())
            retval = true;
        else if (sendQuery(":hE#", response, 0))
            retval = true;
    }
    else // if disable
    {
        if (!MountLocked())
            retval = true;
        else if (sendQuery(":hE#", response, 0))
            retval = true;
    }

    if (retval == false)
        LOGF_ERROR("Failed to %s lock", enable ? "enable" : "disable");
    else
        LOGF_INFO("Lock is %s.", enable ? "enabled" : "disabled");
    return retval;
}

bool LX200Skywalker::SyncDefaultPark() // Saved mount position is loaded and synced
{
    double parkAZ  = GetAxis1Park();
    double parkAlt = GetAxis2Park();

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);
    LOGF_DEBUG("Unparking from Az (%s) Alt (%s)...", AzStr, AltStr);

    ln_hrz_posn horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270
    horizontalPos.az = parkAZ + 180;
    if (horizontalPos.az >= 360)
        horizontalPos.az -= 360;
    horizontalPos.alt = parkAlt;

    ln_lnlat_posn observer;

    observer.lat = LocationN[LOCATION_LATITUDE].value;
    observer.lng = LocationN[LOCATION_LONGITUDE].value;

    if (observer.lng > 180)
        observer.lng -= 360;

    ln_equ_posn equatorialPos;

    ln_get_equ_from_hrz(&horizontalPos, &observer, ln_get_julian_from_sys(), &equatorialPos);

    char RAStr[16], DEStr[16];
    fs_sexa(RAStr, equatorialPos.ra / 15.0, 2, 3600);
    fs_sexa(DEStr, equatorialPos.dec, 2, 3600);
    LOGF_DEBUG("Syncing to parked coordinates RA (%s) DEC (%s)...", RAStr, DEStr);

    return (Sync(equatorialPos.ra / 15.0, equatorialPos.dec));
}

bool LX200Skywalker::SetCurrentPark() // Current mount position is copied into park position fields
{
    ln_hrz_posn horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270

    ln_lnlat_posn observer;
    observer.lat = LocationN[LOCATION_LATITUDE].value;
    observer.lng = LocationN[LOCATION_LONGITUDE].value;
    if (observer.lng > 180)
        observer.lng -= 360;

    ln_equ_posn equatorialPos;
    equatorialPos.ra  = currentRA * 15;
    equatorialPos.dec = currentDEC;
    ln_get_hrz_from_equ(&equatorialPos, &observer, ln_get_julian_from_sys(), &horizontalPos);

    double parkAZ = horizontalPos.az - 180;
    if (parkAZ < 0)
        parkAZ += 360;
    double parkAlt = horizontalPos.alt;

    char AzStr[16], AltStr[16];
    fs_sexa(AzStr, parkAZ, 2, 3600);
    fs_sexa(AltStr, parkAlt, 2, 3600);

    LOGF_DEBUG("Setting current parking position to coordinates Az (%s) Alt (%s)...", AzStr, AltStr);

    SetAxis1Park(parkAZ);
    SetAxis2Park(parkAlt);

    return true;
}

bool LX200Skywalker::SetDefaultPark() // Saved mount position is copied into park position fields
{
    return INDI::Telescope::InitPark();
}

/**
 * @brief Determine the system slew speed mode
 * @param xx
 * @return true iff request succeeded
 */
bool LX200Skywalker::getSystemSlewSpeed (int *xx)
{
    LOG_DEBUG(__FUNCTION__);
    // query status  - :Gm#
    // response      - <number>#

    char response[TCS_RESPONSE_BUFFER_LENGTH] = {0};

    if (!sendQuery(":Gm#", response))
    {
        LOG_ERROR("Failed to send query system slew speed request.");
        return false;
    }
    if (! sscanf(response, "%03d#", xx))
    {
        LOGF_ERROR("Unexpected system slew speed response '%s'.", response);
        return false;
    }
    *xx /= 15; //TCS: displayed Speed (in °/s) = *xx / 15
    return true;
}

bool LX200Skywalker::setSystemSlewSpeed (int xx)
{
    // set speed  - :Sm<number>#
    // response   - none

    char cmd[TCS_COMMAND_BUFFER_LENGTH];
    char response[TCS_RESPONSE_BUFFER_LENGTH] = {0};
    sprintf(cmd, ":Sm%2d#", xx * 15); //TCS: xx = displayed Speed (in °/s) * 15
    if (xx < 0 || xx > 30)
    {
        LOGF_ERROR("Unexpected system slew speed '%02d'.", xx);
        return false;
    }
    else if (sendQuery(cmd, response, TCS_NOANSWER))
    {
        return true;
    }
    else
    {
        LOG_ERROR("Setting system slew speed FAILED");
        return false;
    }

}

/**
 * @brief Retrieve the firmware info from the mount
 * @param firmwareInfo - firmware description
 * @return
 */
bool LX200Skywalker::getFirmwareInfo(char* vstring)
{
    char lstat[20] = {0};
    if(!getJSONData_gp(1, lstat))
        return false;
    else
    {
        strcpy(vstring, lstat);
        return true;
    }
}

/*********************************************************************************
 * Helper functions
 *********************************************************************************/


/**
 * @brief Receive answer from the communication port.
 * @param buffer - buffer holding the answer
 * @param bytes - number of bytes contained in the answer
 * @author CanisUrsa
 * @return true if communication succeeded, false otherwise
 */
bool LX200Skywalker::receive(char* buffer, char end, int wait)
{
    //    LOGF_DEBUG("%s timeout=%ds",__FUNCTION__, wait);
    int bytes = 0;
    int timeout = wait;
    int returnCode = tty_read_section(PortFD, buffer, end, timeout, &bytes);
    if (returnCode != TTY_OK && (bytes < 1))
    {
        char errorString[MAXRBUF];
        tty_error_msg(returnCode, errorString, MAXRBUF);
        if(returnCode == TTY_TIME_OUT && wait <= 0) return false;
        LOGF_WARN("Failed to receive full response: %s. (Return code: %d)", errorString, returnCode);
        return false;
    }
    if(buffer[bytes - 1] == '#')
        buffer[bytes - 1] = '\0'; // remove #
    else
        buffer[bytes] = '\0';

    return true;
}

/**
 * @brief Flush the communication port.
 * @author CanisUrsa
 */
void LX200Skywalker::flush()
{
    //LOG_DEBUG(__FUNCTION__);
    //tcflush(PortFD, TCIOFLUSH);
}

bool LX200Skywalker::transmit(const char* buffer)
{
    //    LOG_DEBUG(__FUNCTION__);
    int bytesWritten = 0;
    flush();
    int returnCode = tty_write_string(PortFD, buffer, &bytesWritten);
    if (returnCode != TTY_OK)
    {
        char errorString[MAXRBUF];
        tty_error_msg(returnCode, errorString, MAXRBUF);
        LOGF_WARN("Failed to transmit %s. Wrote %d bytes and got error %s.", buffer, bytesWritten, errorString);
        return false;
    }
    return true;
}

bool LX200Skywalker::checkLX200EquatorialFormat()
{
    LOG_DEBUG(__FUNCTION__);
    char response[TCS_RESPONSE_BUFFER_LENGTH];

    controller_format = LX200_LONG_FORMAT;

    if (!sendQuery(":GR#", response))
    {
        LOG_ERROR("Failed to get RA for format check");
        return false;
    }
    /* If it's short format, try to toggle to high precision format */
    if (strlen(response) <= 5 || response[5] == '.')
    {
        LOG_INFO("Detected low precision format, "
                 "attempting to switch to high precision.");
        if (!sendQuery(":U#", response, 0))
        {
            LOG_ERROR("Failed to switch precision");
            return false;
        }
        if (!sendQuery(":GR#", response))
        {
            LOG_ERROR("Failed to get high precision RA");
            return false;
        }
    }
    if (strlen(response) <= 5 || response[5] == '.')
    {
        controller_format = LX200_SHORT_FORMAT;
        LOG_INFO("Coordinate format is low precision.");
        return 0;

    }
    else if (strlen(response) > 8 && response[8] == '.')
    {
        controller_format = LX200_LONGER_FORMAT;
        LOG_INFO("Coordinate format is ultra high precision.");
        return 0;
    }
    else
    {
        controller_format = LX200_LONG_FORMAT;
        LOG_INFO("Coordinate format is high precision.");
        return 0;
    }
}

bool LX200Skywalker::SetSlewRate(int index)
{
    LOG_DEBUG(__FUNCTION__);
    // Convert index to Meade format
    index = 3 - index;

    if (!isSimulation() && !setSlewMode(index))
    {
        SlewRateSP.s = IPS_ALERT;
        IDSetSwitch(&SlewRateSP, "Error setting slew mode.");
        return false;
    }

    SlewRateSP.s = IPS_OK;
    IDSetSwitch(&SlewRateSP, nullptr);
    return true;
}
bool LX200Skywalker::setSlewMode(int slewMode)
{
    LOG_DEBUG(__FUNCTION__);
    char cmd[TCS_COMMAND_BUFFER_LENGTH];
    char response[TCS_RESPONSE_BUFFER_LENGTH];

    switch (slewMode)
    {
        case LX200_SLEW_MAX:
            strcpy(cmd, ":RS#");
            break;
        case LX200_SLEW_FIND:
            strcpy(cmd, ":RM#");
            break;
        case LX200_SLEW_CENTER:
            strcpy(cmd, ":RC#");
            break;
        case LX200_SLEW_GUIDE:
            strcpy(cmd, ":RG#");
            break;
        default:
            return false;
    }
    return (sendQuery(cmd, response, 0)); // Don't wait for response - there isn't one
}

IPState LX200Skywalker::GuideNorth(uint32_t ms)
{
    LOGF_DEBUG("%s %dms %d", __FUNCTION__, ms, usePulseCommand);
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }

    // If already moving (no pulse command), then stop movement
    if (MovementNSSP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementNSSP);

        MoveNS(dir == 0 ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_STOP);
    }

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    if (usePulseCommand)
    {
        SendPulseCmd(LX200_NORTH, ms);
    }
    else
    {
        if (!setSlewMode(LX200_SLEW_GUIDE))
        {
            SlewRateSP.s = IPS_ALERT;
            IDSetSwitch(&SlewRateSP, "Error setting slew mode.");
            return IPS_ALERT;
        }

        MovementNSS[DIRECTION_NORTH].s = ISS_ON;
        MoveNS(DIRECTION_NORTH, MOTION_START);
    }

    // Set slew to guiding
    IUResetSwitch(&SlewRateSP);
    SlewRateS[SLEW_GUIDE].s = ISS_ON;
    IDSetSwitch(&SlewRateSP, nullptr);
    guide_direction_ns = LX200_NORTH;
    GuideNSTID      = IEAddTimer(ms, guideTimeoutHelperNS, this);
    return IPS_BUSY;
}

IPState LX200Skywalker::GuideSouth(uint32_t ms)
{
    LOGF_DEBUG("%s %dms %d", __FUNCTION__, ms, usePulseCommand);
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }

    // If already moving (no pulse command), then stop movement
    if (MovementNSSP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementNSSP);

        MoveNS(dir == 0 ? DIRECTION_NORTH : DIRECTION_SOUTH, MOTION_STOP);
    }

    if (GuideNSTID)
    {
        IERmTimer(GuideNSTID);
        GuideNSTID = 0;
    }

    if (usePulseCommand)
    {
        SendPulseCmd(LX200_SOUTH, ms);
    }
    else
    {
        if (!setSlewMode(LX200_SLEW_GUIDE))
        {
            SlewRateSP.s = IPS_ALERT;
            IDSetSwitch(&SlewRateSP, "Error setting slew mode.");
            return IPS_ALERT;
        }

        MovementNSS[DIRECTION_SOUTH].s = ISS_ON;
        MoveNS(DIRECTION_SOUTH, MOTION_START);
    }

    // Set slew to guiding
    IUResetSwitch(&SlewRateSP);
    SlewRateS[SLEW_GUIDE].s = ISS_ON;
    IDSetSwitch(&SlewRateSP, nullptr);
    guide_direction_ns = LX200_SOUTH;
    GuideNSTID      = IEAddTimer(ms, guideTimeoutHelperNS, this);
    return IPS_BUSY;
}

IPState LX200Skywalker::GuideEast(uint32_t ms)
{
    LOGF_DEBUG("%s %dms %d", __FUNCTION__, ms, usePulseCommand);
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }

    // If already moving (no pulse command), then stop movement
    if (MovementWESP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementWESP);

        MoveWE(dir == 0 ? DIRECTION_WEST : DIRECTION_EAST, MOTION_STOP);
    }

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    if (usePulseCommand)
    {
        SendPulseCmd(LX200_EAST, ms);
    }
    else
    {
        if (!setSlewMode(LX200_SLEW_GUIDE))
        {
            SlewRateSP.s = IPS_ALERT;
            IDSetSwitch(&SlewRateSP, "Error setting slew mode.");
            return IPS_ALERT;
        }

        MovementWES[DIRECTION_EAST].s = ISS_ON;
        MoveWE(DIRECTION_EAST, MOTION_START);
    }

    // Set slew to guiding
    IUResetSwitch(&SlewRateSP);
    SlewRateS[SLEW_GUIDE].s = ISS_ON;
    IDSetSwitch(&SlewRateSP, nullptr);
    guide_direction_we = LX200_EAST;
    GuideWETID      = IEAddTimer(ms, guideTimeoutHelperWE, this);
    return IPS_BUSY;
}

IPState LX200Skywalker::GuideWest(uint32_t ms)
{
    LOGF_DEBUG("%s %dms %d", __FUNCTION__, ms, usePulseCommand);
    if (usePulseCommand && (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY))
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }

    // If already moving (no pulse command), then stop movement
    if (MovementWESP.s == IPS_BUSY)
    {
        int dir = IUFindOnSwitchIndex(&MovementWESP);

        MoveWE(dir == 0 ? DIRECTION_WEST : DIRECTION_EAST, MOTION_STOP);
    }

    if (GuideWETID)
    {
        IERmTimer(GuideWETID);
        GuideWETID = 0;
    }

    if (usePulseCommand)
    {
        SendPulseCmd(LX200_WEST, ms);
    }
    else
    {
        if (!setSlewMode(LX200_SLEW_GUIDE))
        {
            SlewRateSP.s = IPS_ALERT;
            IDSetSwitch(&SlewRateSP, "Error setting slew mode.");
            return IPS_ALERT;
        }

        MovementWES[DIRECTION_WEST].s = ISS_ON;
        MoveWE(DIRECTION_WEST, MOTION_START);
    }

    // Set slew to guiding
    IUResetSwitch(&SlewRateSP);
    SlewRateS[SLEW_GUIDE].s = ISS_ON;
    IDSetSwitch(&SlewRateSP, nullptr);
    guide_direction_we = LX200_WEST;
    GuideWETID      = IEAddTimer(ms, guideTimeoutHelperWE, this);
    return IPS_BUSY;
}

int LX200Skywalker::SendPulseCmd(int8_t direction, uint32_t duration_msec)
{
    LOGF_DEBUG("%s dir=%d dur=%d ms", __FUNCTION__, direction, duration_msec );
    char cmd[TCS_COMMAND_BUFFER_LENGTH];
    char response[TCS_RESPONSE_BUFFER_LENGTH];
    switch (direction)
    {
        case LX200_NORTH:
            sprintf(cmd, ":Mgn%04u#", duration_msec);
            break;
        case LX200_SOUTH:
            sprintf(cmd, ":Mgs%04u#", duration_msec);
            break;
        case LX200_EAST:
            sprintf(cmd, ":Mge%04u#", duration_msec);
            break;
        case LX200_WEST:
            sprintf(cmd, ":Mgw%04u#", duration_msec);
            break;
        default:
            return 1;
    }
    if (!sendQuery(cmd, response, 0)) // Don't wait for response - there isn't one
    {
        return false;
    }
    return true;
}

bool LX200Skywalker::MountTracking()
{
    LOG_DEBUG(__FUNCTION__);
    // query status  - :GK#
    // response      - 1# / 0# (ON / OFF)

    char response[TCS_RESPONSE_BUFFER_LENGTH] = {0};

    if (!sendQuery(":GK#", response))
    {
        LOG_ERROR("Failed to send query tracking state request.");
        return false;
    }
    if (strcmp(response, "0") == 0)
        return false;
    else
        return true;
}

bool LX200Skywalker::SetTrackEnabled(bool enabled)
{
    // Command set tracking on  - :hT#
    //         set tracking off - :hN#

    char response[TCS_RESPONSE_BUFFER_LENGTH] = {0};
    if (!sendQuery(enabled ? ":hT#" : ":hN#", response, 0))
    {
        LOGF_ERROR("Failed to %s tracking", enabled ? "enable" : "disable");
        return false;
    }
    LOGF_INFO("Tracking %s.", enabled ? "enabled" : "disabled");
    return true;
}

bool LX200Skywalker::SetTrackRate(double raRate, double deRate)
{
    LOG_DEBUG(__FUNCTION__);
    INDI_UNUSED(raRate);
    INDI_UNUSED(deRate);
    char cmd[TCS_COMMAND_BUFFER_LENGTH];
    char response[TCS_RESPONSE_BUFFER_LENGTH];
    int rate = raRate;
    sprintf(cmd, ":X1E%04d", rate);
    if(!sendQuery(cmd, response, 0))
    {
        LOGF_ERROR("Failed to set tracking t %d", rate);
        return false;
    }
    return true;
}

void LX200Skywalker::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    LX200Telescope::ISGetProperties(dev);
    if (isConnected())
    {
        if (HasTrackMode() && TrackModeS != nullptr)
            defineSwitch(&TrackModeSP);
        if (CanControlTrack())
            defineSwitch(&TrackStateSP);
        if (HasTrackRate())
            defineNumber(&TrackRateNP);        
    }
    /*
        if (isConnected())
        {
            if (genericCapability & LX200_HAS_ALIGNMENT_TYPE)
                defineSwitch(&AlignmentSP);

            if (genericCapability & LX200_HAS_TRACKING_FREQ)
                defineNumber(&TrackingFreqNP);

            if (genericCapability & LX200_HAS_PULSE_GUIDING)
                defineSwitch(&UsePulseCmdSP);

            if (genericCapability & LX200_HAS_SITES)
            {
                defineSwitch(&SiteSP);
                defineText(&SiteNameTP);
            }

            defineNumber(&GuideNSNP);
            defineNumber(&GuideWENP);

            if (genericCapability & LX200_HAS_FOCUS)
            {
                defineSwitch(&FocusMotionSP);
                defineNumber(&FocusTimerNP);
                defineSwitch(&FocusModeSP);
            }
        }
        */
}

bool LX200Skywalker::Goto(double ra, double dec)
{
    LOG_DEBUG(__FUNCTION__);
    const struct timespec timeout = {0, 100000000L};

    targetRA  = ra;
    targetDEC = dec;
    char RAStr[64] = {0}, DecStr[64] = {0};
    int fracbase = 3600;

    fs_sexa(RAStr, targetRA, 2, fracbase);
    fs_sexa(DecStr, targetDEC, 2, fracbase);

    // If moving, let's stop it first.
    if (EqNP.s == IPS_BUSY)
    {
        if (!isSimulation() && !Abort())
        {
            AbortSP.s = IPS_ALERT;
            IDSetSwitch(&AbortSP, "Abort slew failed.");
            return false;
        }

        AbortSP.s = IPS_OK;
        EqNP.s    = IPS_IDLE;
        IDSetSwitch(&AbortSP, "Slew aborted.");
        IDSetNumber(&EqNP, nullptr);

        if (MovementNSSP.s == IPS_BUSY || MovementWESP.s == IPS_BUSY)
        {
            MovementNSSP.s = MovementWESP.s = IPS_IDLE;
            EqNP.s                          = IPS_IDLE;
            IUResetSwitch(&MovementNSSP);
            IUResetSwitch(&MovementWESP);
            IDSetSwitch(&MovementNSSP, nullptr);
            IDSetSwitch(&MovementWESP, nullptr);
        }

        // sleep for 100 mseconds
        nanosleep(&timeout, nullptr);
    }
    if(!isSimulation() && !setObjectCoords(ra, dec))
    {
        LOG_ERROR("Error setting coords for goto");
        return false;
    }

    if (!isSimulation())
    {
        char response[TCS_RESPONSE_BUFFER_LENGTH];
        if(!sendQuery(":MS#", response))
            /* Query reads '0', mount is not slewing */
        {
            LOG_ERROR("Error Slewing");
            slewError(0);
            return false;
        }
    }

    TrackState = SCOPE_SLEWING;
    EqNP.s     = IPS_BUSY;

    LOGF_INFO("Slewing to RA: %s / DEC: %s", RAStr, DecStr);

    return true;
}

bool LX200Skywalker::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    LOG_DEBUG(__FUNCTION__);
    char cmd[TCS_COMMAND_BUFFER_LENGTH];
    char response[TCS_RESPONSE_BUFFER_LENGTH];

    sprintf(cmd, ":%s%s#", command == MOTION_START ? "M" : "Q", dir == DIRECTION_NORTH ? "n" : "s");
    if (!isSimulation() && !sendQuery(cmd, response, 0))
    {
        LOG_ERROR("Error N/S motion direction.");
        return false;
    }

    return true;
}

bool LX200Skywalker::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    LOG_DEBUG(__FUNCTION__);
    char cmd[TCS_COMMAND_BUFFER_LENGTH];
    char response[TCS_RESPONSE_BUFFER_LENGTH];

    sprintf(cmd, ":%s%s#", command == MOTION_START ? "M" : "Q", dir == DIRECTION_WEST ? "w" : "e");

    if (!isSimulation() && !sendQuery(cmd, response, 0))
    {
        LOG_ERROR("Error W/E motion direction.");
        return false;
    }

    return true;
}

bool LX200Skywalker::Abort()
{
    LOG_DEBUG(__FUNCTION__);
    //   char cmd[TCS_COMMAND_BUFFER_LENGTH];
    char response[TCS_RESPONSE_BUFFER_LENGTH];
    if (!isSimulation() && !sendQuery(":Q#", response, 0))
    {
        LOG_ERROR("Failed to abort slew.");
        return false;
    }

    if (GuideNSNP.s == IPS_BUSY || GuideWENP.s == IPS_BUSY)
    {
        GuideNSNP.s = GuideWENP.s = IPS_IDLE;
        GuideNSN[0].value = GuideNSN[1].value = 0.0;
        GuideWEN[0].value = GuideWEN[1].value = 0.0;

        if (GuideNSTID)
        {
            IERmTimer(GuideNSTID);
            GuideNSTID = 0;
        }

        if (GuideWETID)
        {
            IERmTimer(GuideWETID);
            GuideNSTID = 0;
        }

        LOG_INFO("Guide aborted.");
        IDSetNumber(&GuideNSNP, nullptr);
        IDSetNumber(&GuideWENP, nullptr);

        return true;
    }

    return true;
}

bool LX200Skywalker::Sync(double ra, double dec)
{
    LOG_DEBUG(__FUNCTION__);
    char response[TCS_RESPONSE_BUFFER_LENGTH];

    if(!isSimulation() && !setObjectCoords(ra, dec))
    {
        LOG_ERROR("Error setting coords for sync");
        return false;
    }

    if (!isSimulation() && !sendQuery(":CM#", response))
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Synchronization failed.");
        return false;
    }

    currentRA  = ra;
    currentDEC = dec;

    LOG_INFO("Synchronization successful.");

    EqNP.s     = IPS_OK;

    NewRaDec(currentRA, currentDEC);

    if (!notifyPierSide())
        return false;
    // show lock
    if (MountLocked()) // Normally lock is set by TCS on syncing
    {
        MountStateS[0].s = ISS_ON;
        MountStateS[1].s = ISS_OFF;
        MountStateSP.s = IPS_OK;
        CurrentMountState = MOUNT_LOCKED; // ALWAYS set status!
        IDSetSwitch(&MountStateSP, nullptr);
    }
    else if (INDI::Telescope::TrackState == SCOPE_PARKED) // sync is called after parkpos reached
    {
        MountStateS[0].s = ISS_OFF;
        MountStateS[1].s = ISS_ON;
        MountStateSP.s = IPS_OK;
        CurrentMountState = MOUNT_UNLOCKED; // ALWAYS set status!
        IDSetSwitch(&MountStateSP, nullptr);
        if (SetTrackEnabled(false))
        {
            TrackStateS[TRACK_ON].s = ISS_ON;
            TrackStateS[TRACK_OFF].s = ISS_OFF;
            TrackStateSP.s = IPS_ALERT;
            // INDI::Telescope::TrackState = SCOPE_PARKED; // ALWAYS set status!
            IDSetSwitch(&TrackStateSP, nullptr);
            LOG_WARN("Telescope still parked!");
            return false;
        }
        else
        {
            LOG_ERROR("Mount not locked on sync!");
            return false;
        }
    }
    // set tracking
    if (MountTracking()) // Normally tracking is set by TCS on syncing
    {
        TrackStateS[TRACK_ON].s = ISS_ON;
        TrackStateS[TRACK_OFF].s = ISS_OFF;
        TrackStateSP.s = IPS_OK;
        INDI::Telescope::TrackState = SCOPE_TRACKING; // ALWAYS set status!
        IDSetSwitch(&TrackStateSP, nullptr);
    }
    else
    {
        LOG_ERROR("Tracking not set on sync!");
        return false;
    }
    return true;
}


bool LX200Skywalker::setObjectCoords(double ra, double dec)
{
    LOG_DEBUG(__FUNCTION__);

    char RAStr[64] = {0}, DecStr[64] = {0};
    int h, m, s, d;
    getSexComponents(ra, &h, &m, &s);
    snprintf(RAStr, sizeof(RAStr), ":Sr%02d:%02d:%02d#", h, m, s);
    getSexComponents(dec, &d, &m, &s);
    /* case with negative zero */
    if (!d && dec < 0)
        snprintf(DecStr, sizeof(DecStr), ":Sd-%02d*%02d:%02d#", d, m, s);
    else
        snprintf(DecStr, sizeof(DecStr), ":Sd%+03d*%02d:%02d#", d, m, s);
    char response[TCS_RESPONSE_BUFFER_LENGTH];
    if (isSimulation()) return true;
    // These commands receive a response without a terminating #
    if(!sendQuery(RAStr, response, '1', 2)  || !sendQuery(DecStr, response, '1', 2) )
    {
        EqNP.s = IPS_ALERT;
        IDSetNumber(&EqNP, "Error setting RA/DEC.");
        return false;
    }

    return true;
}

bool LX200Skywalker::setLocalDate(uint8_t days, uint8_t months, uint16_t years)
{
    LOG_DEBUG(__FUNCTION__);
    char cmd[RB_MAX_LEN] = {0};
    char response[RB_MAX_LEN] = {0};

    int yy = years % 100;

    snprintf(cmd, sizeof(cmd), ":SC%02d/%02d/%02d#", months, days, yy);
    // Correct command string without spaces and with slashes (wrong in lx200driver of INDIcore!)
    // ":SCMM/DD/YY#" (cf. Meade Telescope Serial Command Protocol; Revision 2010.10)
    return (sendQuery(cmd, response, 0));
}

bool LX200Skywalker::setLocalTime24(uint8_t hour, uint8_t minute, uint8_t second)
{
    LOG_DEBUG(__FUNCTION__);
    char cmd[RB_MAX_LEN] = {0};
    char response[RB_MAX_LEN] = {0};

    snprintf(cmd, sizeof(cmd), ":SL%02d:%02d:%02d#", hour, minute, second);
    // Correct command string without spaces (wrong in lx200driver of INDIcore!)
    // ":SLHH:MM:SS#" (cf. Meade Telescope Serial Command Protocol; Revision 2010.10)
    return (sendQuery(cmd, response, 0));
}

bool LX200Skywalker::setUTCOffset(double offset)
{
    LOG_DEBUG(__FUNCTION__);
    char cmd[RB_MAX_LEN] = {0};
    char response[RB_MAX_LEN] = {0};
    int hours = offset * -1.0;

    snprintf(cmd, sizeof(cmd), ":SG%+03d#", hours);
    // Correct command string without spaces (wrong in lx200driver of INDIcore!)
    // ":SGsHH.H#" (cf. Meade Telescope Serial Command Protocol; Revision 2010.10)
    return (sendQuery(cmd, response, 0));
}

bool LX200Skywalker::getTrackFrequency(double *value)
{
    LOG_DEBUG(__FUNCTION__);
    float Freq;
    char response[RB_MAX_LEN] = {0};

    if (!sendQuery(":GT#", response))
        return false;

    if (sscanf(response, "%f#", &Freq) < 1)
    {
        LOG_ERROR("Unable to parse response");
        return false;
    }

    *value = static_cast<double>(Freq);
    return true;
}


﻿/*
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

const char *INFO_TAB = "Info";

static class Loader
{
        std::unique_ptr<LX200Skywalker> telescope;
    public:
        Loader()
        {
            if (telescope.get() == nullptr)
            {
                LX200Skywalker* myScope = new LX200Skywalker();
                telescope.reset(myScope);
            }
        }
} loader;

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

//**************************************************************************************
const char *LX200Skywalker::getDefaultName()
{
    return "AOK Skywalker";
}

//**************************************************************************************
bool LX200Skywalker::Handshake()
{
    char fwinfo[TCS_JSON_BUFFER_LENGTH] = {0};
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

//**************************************************************************************
void LX200Skywalker::ISGetProperties(const char *dev)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) != 0)
        return;

    LX200Telescope::ISGetProperties(dev);
    if (isConnected())
    {
        if (HasTrackMode() && TrackModeSP.isValid())
            defineProperty(TrackModeSP);
        if (CanControlTrack())
            defineProperty(TrackStateSP);
        if (HasTrackRate())
            defineProperty(TrackRateNP);
    }
    /*
        if (isConnected())
        {
            if (genericCapability & LX200_HAS_ALIGNMENT_TYPE)
                defineProperty(&AlignmentSP);

            if (genericCapability & LX200_HAS_TRACKING_FREQ)
                defineProperty(&TrackingFreqNP);

            if (genericCapability & LX200_HAS_PULSE_GUIDING)
                defineProperty(&UsePulseCmdSP);

            if (genericCapability & LX200_HAS_SITES)
            {
                defineProperty(&SiteSP);
                defineProperty(&SiteNameTP);
            }

            defineProperty(&GuideNSNP);
            defineProperty(&GuideWENP);

            if (genericCapability & LX200_HAS_FOCUS)
            {
                defineProperty(&FocusMotionSP);
                defineProperty(&FocusTimerNP);
                defineProperty(&FocusModeSP);
            }
        }
        */
}

//**************************************************************************************
bool LX200Skywalker::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if (dev != nullptr && strcmp(dev, getDeviceName()) == 0)
    {
        // tracking state
        if (TrackStateSP.isNameMatch(name))
        {
            if (TrackStateSP.update( states, names, n) == false)
                return false;
            int trackState = TrackStateSP.findOnSwitchIndex();
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

            TrackStateSP.setState(result ? IPS_OK : IPS_ALERT);
            TrackStateSP.apply();
            return result;
        }
        // tracking mode
        else if (TrackModeSP.isNameMatch(name))
        {
            if (TrackModeSP.update(states, names, n) == false)
                return false;
            int trackMode = TrackModeSP.findOnSwitchIndex();
            bool result = false;

            switch (trackMode) // ToDo: Custom tracking
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
                    LOG_INFO("Lunar tracking mode selected.");
                    result = SetTrackMode(trackMode);
                    break;
                case TRACK_CUSTOM:
                    LOG_INFO("Custom tracking not yet implemented.");
                    break;
            }

            TrackModeSP.setState(result ? IPS_OK : IPS_ALERT);
            TrackModeSP.apply();
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
        if (ParkSP.isNameMatch(name))
        {
            if (LX200Telescope::ISNewSwitch(dev, name, states, names, n))
            {
                ParkSP.setState(IPS_OK); //INDI::Telescope::SetParked(false) sets IPS_IDLE (!?)
                ParkSP.apply();
                return true;
            }
            else
                return false;
        }
        if (ParkOptionSP.isNameMatch(name))
        {
            ParkOptionSP.update(states, names, n);
            int index = ParkOptionSP.findOnSwitchIndex();
            if (index == -1)
                return false;
            ParkOptionSP.reset();
            if ((TrackState != SCOPE_IDLE && TrackState != SCOPE_TRACKING) || MovementNSSP.getState() == IPS_BUSY ||
                MovementWESP.getState() == IPS_BUSY)
            {
                LOG_WARN("Mount slewing or already parked...");
                ParkOptionSP.setState(IPS_ALERT);
                ParkOptionSP.apply();
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

//**************************************************************************************
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

//**************************************************************************************
bool LX200Skywalker::initProperties()
{
    /* Make sure to init parent properties first */
    if (!LX200Telescope::initProperties()) return false;

    // INDI::Telescope has to know which parktype to enable parking options UI
    SetParkDataType(PARK_AZ_ALT); //

    // System Slewspeed
    IUFillNumber(&SystemSlewSpeedP[0], "SLEW_SPEED", "Slewspeed", "%.2f", 0.0, 30.0, 1, 0);
    IUFillNumberVector(&SystemSlewSpeedNP, SystemSlewSpeedP, 1, getDeviceName(), "SLEW_SPEED", "Slewspeed", MAIN_CONTROL_TAB,
                       IP_RW, 60, IPS_IDLE);

    // Motors Status
    IUFillSwitch(&MountStateS[0], "On", "", ISS_OFF);
    IUFillSwitch(&MountStateS[1], "Off", "", ISS_OFF);
    IUFillSwitchVector(&MountStateSP, MountStateS, 2, getDeviceName(), "Mountlock", "Mount lock", MAIN_CONTROL_TAB, IP_RW,
                       ISR_1OFMANY, 0, IPS_IDLE);

    // Infotab
    IUFillText(&FirmwareVersionT[0], "Firmware", "Version", "123456");
    IUFillTextVector(&FirmwareVersionTP, FirmwareVersionT, 1, getDeviceName(), "Firmware", "Firmware", INFO_TAB, IP_RO, 60,
                     IPS_IDLE);

    // Setting the park position in the controller (with webinterface) evokes a restart of the very same!
    // 4th option "purge" of INDI::Telescope doesn't make any sense here, so it is not displayed
    ParkOptionSP[PARK_CURRENT].fill("PARK_CURRENT", "Copy", ISS_OFF);
    ParkOptionSP[PARK_DEFAULT].fill("PARK_DEFAULT", "Read", ISS_OFF);
    ParkOptionSP[PARK_WRITE_DATA].fill("PARK_WRITE_DATA", "Write", ISS_OFF);
    ParkOptionSP.fill(getDeviceName(), "TELESCOPE_PARK_OPTION", "Park Options", SITE_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    return true;
}

//**************************************************************************************
bool LX200Skywalker::updateProperties()
{
    if (! LX200Telescope::updateProperties()) return false;
    if (isConnected())
    {
        // Switch is obsolete: NOT using pulse commands makes no sense with TCS
        deleteProperty(UsePulseCmdSP.name);
        // FIRST delete property, THEN define new ones!
        // Otherwise we break the list of buttons defined beforehand and lose their
        // responsiveness in the INDI Control Panel when called from EKOS
        defineProperty(&MountStateSP);
        defineProperty(&SystemSlewSpeedNP);
        defineProperty(&FirmwareVersionTP);
    }
    else
    {
        deleteProperty(MountStateSP.name);
        deleteProperty(SystemSlewSpeedNP.name);
        deleteProperty(FirmwareVersionTP.name);
    }

    return true;
}

//**************************************************************************************
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

/***************************************************************************************
bool LX200Skywalker::ReadScopeStatus()
{
    return (LX200Telescope::ReadScopeStatus());  // TCS does not :D#! -> ovverride isSlewComplete
}*/

//**************************************************************************************
void LX200Skywalker::getBasicData()
{
    LOG_DEBUG(__FUNCTION__);
    if (!isSimulation())
    {
        checkLX200EquatorialFormat();

        if (INDI::Telescope::capability & TELESCOPE_CAN_PARK)
        {
            ParkSP.setState(IPS_OK);
            ParkSP.apply();
        }

        if (genericCapability & LX200_HAS_ALIGNMENT_TYPE)
            getAlignment();

        if (genericCapability & LX200_HAS_TRACKING_FREQ)
        {
            if (! getTrackFrequency(&TrackFreqN[0].value))
                LOG_ERROR("Failed to get tracking frequency from device.");
            else
                IDSetNumber(&TrackFreqNP, nullptr);
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
            int trackMode = TrackModeSP.findOnSwitchIndex();
            //int modes = sizeof(TelescopeTrackMode); (enum Sidereal, Solar, Lunar, Custom)
            int modes = TRACK_LUNAR; // ToDo: Custom tracking
            TrackModeSP.setState((trackMode <= modes) ? IPS_OK : IPS_ALERT);
            TrackModeSP.apply();
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
                    ParkSP.setState(IPS_OK);
                    ParkSP.apply();
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

    /* NOT using pulse commands makes no sense with skywalker controller
    if (genericCapability & LX200_HAS_PULSE_GUIDING)
    {
        UsePulseCmdS[0].s = ISS_ON;
        UsePulseCmdS[1].s = ISS_OFF;
        UsePulseCmdSP.s = IPS_OK;
        usePulseCommand = false; // ALWAYS set status! (cf. ISNewSwitch())
        IDSetSwitch(&UsePulseCmdSP, nullptr);
    }*/

}

//**************************************************************************************
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

/*********************************************************************************
 * config file
 *********************************************************************************/
bool LX200Skywalker::saveConfigItems(FILE *fp)
{
    LOG_DEBUG(__FUNCTION__);
    IUSaveConfigText(fp, &SiteNameTP);

    return LX200Telescope::saveConfigItems(fp);
}

/********************************************************************************
 * Notifier-Section
 ********************************************************************************/
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
                notifyPierSide(PierSideWest());
                if (MountLocked()) // Normally lock is set by TCS if slew ends
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

// Following items were changed from original "set_" to "notify_" because of the logic behind: From
// the controller view we change the viewer (and a copy of the model), not the the model itself!
void LX200Skywalker::notifyPierSide(bool west)
{
    if (west)
    {
        Telescope::setPierSide(INDI::Telescope::PIER_WEST);
        LOG_INFO("Telescope pointing east");
    }
    else
    {
        Telescope::setPierSide(INDI::Telescope::PIER_EAST);
        LOG_INFO("Telescope pointing west");
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
        TrackStateSP[TRACK_ON].setState(ISS_ON);
        TrackStateSP[TRACK_OFF].setState(ISS_OFF);
        TrackStateSP.setState(IPS_OK);
        INDI::Telescope::TrackState = state;
    }
    else
    {
        TrackStateSP[TRACK_ON].setState(ISS_OFF);
        TrackStateSP[TRACK_OFF].setState(ISS_ON);
        TrackStateSP.setState(IPS_OK);
        INDI::Telescope::TrackState = state;
    }
    TrackStateSP.apply();
}

/*********************************************************************************
 * Get/Set
 *********************************************************************************/
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

bool LX200Skywalker::SetCurrentPark() // Current mount position is copied into park position fields
{
    ln_hrz_posn horizontalPos;
    // Libnova south = 0, west = 90, north = 180, east = 270

    ln_lnlat_posn observer;
    observer.lat = LocationNP[LOCATION_LATITUDE].getValue();
    observer.lng = LocationNP[LOCATION_LONGITUDE].getValue();
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
    *xx /= 11; //TCS: displayed value (speed in °/s) = *xx / 11
    return true;
}

bool LX200Skywalker::setSystemSlewSpeed (int xx)
{
    // set speed  - :Sm<number>#
    // response   - none

    char cmd[TCS_COMMAND_BUFFER_LENGTH];
    char response[TCS_RESPONSE_BUFFER_LENGTH] = {0};
    sprintf(cmd, ":Sm%2d#", xx * 11); //TCS: saved value = (speed in °/s) * 11
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

bool LX200Skywalker::SetSlewRate(int index)
{
    LOG_DEBUG(__FUNCTION__);
    // Convert index to Meade format
    index = 3 - index;

    if (!isSimulation() && !setSlewMode(index))
    {
        SlewRateSP.setState(IPS_ALERT);
        LOG_ERROR("Error setting slew mode.");
        SlewRateSP.apply();
        return false;
    }

    SlewRateSP.setState(IPS_OK);
    SlewRateSP.apply();
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
        EqNP.setState(IPS_ALERT);
        LOG_ERROR("Error setting RA/DEC.");
        EqNP.apply();
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

/*********************************************************************************
 * Control
 *********************************************************************************/
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
    if (EqNP.getState() == IPS_BUSY)
    {
        if (!isSimulation() && !Abort())
        {
            AbortSP.setState(IPS_ALERT);
            LOG_ERROR("Abort slew failed.");
            AbortSP.apply();
            return false;
        }

        AbortSP.setState(IPS_OK);
        EqNP.setState(IPS_IDLE);
        LOG_ERROR("Slew aborted.");
        AbortSP.apply();
        EqNP.apply();

        if (MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY)
        {
            MovementNSSP.setState(IPS_IDLE);
            MovementWESP.setState(IPS_IDLE);
            EqNP.setState(IPS_IDLE);
            MovementNSSP.reset();
            MovementWESP.reset();
            MovementNSSP.apply();
            MovementWESP.apply();
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
    EqNP.setState(IPS_BUSY);

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
        EqNP.setState(IPS_ALERT);
        LOG_ERROR("Synchronization failed");
        EqNP.apply();
        return false;
    }

    currentRA  = ra;
    currentDEC = dec;

    LOG_INFO("Synchronization successful.");

    EqNP.setState(IPS_OK);

    NewRaDec(currentRA, currentDEC);

    notifyPierSide(PierSideWest());
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
            TrackStateSP[TRACK_ON].setState(ISS_ON);
            TrackStateSP[TRACK_OFF].setState(ISS_OFF);
            TrackStateSP.setState(IPS_ALERT);
            // INDI::Telescope::TrackState = SCOPE_PARKED; // ALWAYS set status!
            TrackStateSP.apply();
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
        TrackStateSP[TRACK_ON].setState(ISS_ON);
        TrackStateSP[TRACK_OFF].setState(ISS_OFF);
        TrackStateSP.setState(IPS_OK);
        INDI::Telescope::TrackState = SCOPE_TRACKING; // ALWAYS set status!
        TrackStateSP.apply();
    }
    else
    {
        LOG_ERROR("Tracking not set on sync!");
        return false;
    }
    return true;
}

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
        if (MountLocked()) // TCS should set Mountlock
        {
            notifyMountLock(true);
            // INDI::Telescope::SetParked(false) sets TrackState = SCOPE_IDLE but TCS is tracking
            notifyTrackState(SCOPE_TRACKING);
            // INDI::Telescope::SetParked(false) sets ParkSP.S = IPS_IDLE but mount IS unparked!
            ParkSP.setState(IPS_OK);
            ParkSP.apply();
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

    observer.lat = LocationNP[LOCATION_LATITUDE].getValue();
    observer.lng = LocationNP[LOCATION_LONGITUDE].getValue();

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

    if (GuideNSNP.getState() == IPS_BUSY || GuideWENP.getState() == IPS_BUSY)
    {
        GuideNSNP.setState(IPS_IDLE);
        GuideWENP.setState(IPS_IDLE);
        GuideNSNP[0].setValue(0);
        GuideNSNP[1].setValue(0);
        GuideWENP[0].setValue(0);
        GuideWENP[1].setValue(0);

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
        GuideNSNP.apply();
        GuideWENP.apply();
        return true;
    }

    return true;
}

/*********************************************************************************
 * Guiding
 *********************************************************************************/
IPState LX200Skywalker::GuideNorth(uint32_t ms)
{
    LOGF_DEBUG("%s %dms", __FUNCTION__, ms);
    if (MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY)
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }
    return SendPulseCmd(LX200_NORTH, ms) ? IPS_OK : IPS_ALERT;
}

IPState LX200Skywalker::GuideSouth(uint32_t ms)
{
    LOGF_DEBUG("%s %dms", __FUNCTION__, ms);
    if (MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY)
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }
    return SendPulseCmd(LX200_SOUTH, ms) ? IPS_OK : IPS_ALERT;
}

IPState LX200Skywalker::GuideEast(uint32_t ms)
{
    LOGF_DEBUG("%s %dms", __FUNCTION__, ms);
    if (MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY)
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }
    return SendPulseCmd(LX200_EAST, ms) ? IPS_OK : IPS_ALERT;
}

IPState LX200Skywalker::GuideWest(uint32_t ms)
{
    LOGF_DEBUG("%s %dms", __FUNCTION__, ms);
    if (MovementNSSP.getState() == IPS_BUSY || MovementWESP.getState() == IPS_BUSY)
    {
        LOG_ERROR("Cannot guide while moving.");
        return IPS_ALERT;
    }
    return SendPulseCmd(LX200_WEST, ms) ? IPS_OK : IPS_ALERT;
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

/*********************************************************************************
 * Helper functions
 *********************************************************************************/
// static_cast<type>(enum::element)
template <typename T>
constexpr typename std::underlying_type<T>::type index_of(T element) noexcept
{
    return static_cast<typename std::underlying_type<T>::type>(element);
}

bool LX200Skywalker::getFirmwareInfo(char* vstring)
{
    char lstat[TCS_RESPONSE_BUFFER_LENGTH] = {0};
    if(!getJSONData(":gp", index_of(val::version), lstat))
        return false;
    else
    {
        strcpy(vstring, lstat);
        return true;
    }
}

bool LX200Skywalker::MountLocked()
{
    char lstat[TCS_RESPONSE_BUFFER_LENGTH] = {0};
    if(!getJSONData(":gp", index_of(val::lock), lstat))
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

bool LX200Skywalker::PierSideWest()
{
    char lstat[TCS_RESPONSE_BUFFER_LENGTH] = {0};
    if (!getJSONData(":Y#", index_of(V1::gstate), lstat))
        return false;
    else
    {
        int li = std::stoi(lstat);
        li = li & (1 << 7);
        if (li > 0)
            return true;
        else
            return false;
    }
}

bool LX200Skywalker::getJSONData(const char* cmd, const uint8_t tok_index, char* data)
{
    char lresponse[TCS_JSON_BUFFER_LENGTH];
    lresponse [0] = '\0';
    char end = '}';
    if(!transmit(cmd))
        return false;
    if (!receive(lresponse, end, 1))
        return false;
    const char delimiter[] = ",";
    char* token;
    uint8_t i = 0;
    token = strtok(lresponse, delimiter);
    while (++i <= tok_index)
    {
        token = strtok(nullptr, delimiter);
    }
    if (!token)
    {
        LOGF_ERROR("Failed to parse token [%d] of JSON response '%s'", tok_index, lresponse);
        return false;
    }
    //strncpy(data, token, strlen(token));
    memcpy(data, token, strlen(token));
    return true;
}

bool LX200Skywalker::sendQuery(const char* cmd, char* response, char end, int wait)
{
    LOGF_DEBUG("%s %s End:%c Wait:%ds", __FUNCTION__, cmd, end, wait);
    response[0] = '\0';
    char lresponse[TCS_RESPONSE_BUFFER_LENGTH];
    lresponse [0] = '\0';
    if(!transmit(cmd))
        return false;
    else if ((wait > TCS_NOANSWER) && (receive(lresponse, end, wait)))
    {
        strcpy(response, lresponse);
        return true;
    }
    else
        return true;
}

bool LX200Skywalker::transmit(const char* buffer)
{
    //    LOG_DEBUG(__FUNCTION__);
    int bytesWritten = 0;
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
    tcflush(PortFD, TCIOFLUSH);
    return true;
}


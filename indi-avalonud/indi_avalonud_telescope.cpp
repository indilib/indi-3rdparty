/*
    Avalon Unified Driver Telescope

    Copyright (C) 2020,2023

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

#include "config.h"

#include "indicom.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <memory>
#ifdef _USE_SYSTEM_JSONLIB
#include <nlohmann/json.hpp>
#else
#include <indijson.hpp>
#endif
#include <chrono>
#include <pthread.h>
#include <zmq.h>

#include "indi_avalonud_telescope.h"


using json = nlohmann::json;

const int IPport = 5451;

static char device_str[MAXINDIDEVICE] = "AvalonUD Telescope";


static std::unique_ptr<AUDTELESCOPE> telescope(new AUDTELESCOPE());

void ISInit()
{
    static int isInit = 0;

    if (isInit == 1)
        return;

    isInit = 1;
    if(telescope.get() == 0) telescope.reset(new AUDTELESCOPE());
}

void ISGetProperties(const char *dev)
{
    telescope->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    telescope->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    telescope->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    telescope->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[],
                char *names[], int num)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(num);
}

void ISSnoopDevice (XMLEle *root)
{
    telescope->ISSnoopDevice(root);
}

/****************************************************************
**
**
*****************************************************************/
AUDTELESCOPE::AUDTELESCOPE() : GI(this)
{
    setVersion(AVALONUD_VERSION_MAJOR, AVALONUD_VERSION_MINOR);

    context = zmq_ctx_new();
    setTelescopeConnection( CONNECTION_NONE );
    SetTelescopeCapability( TELESCOPE_CAN_GOTO |
                            TELESCOPE_CAN_SYNC |
                            TELESCOPE_CAN_PARK |
                            TELESCOPE_CAN_ABORT |
                            TELESCOPE_HAS_TIME |
                            TELESCOPE_HAS_LOCATION |
                            TELESCOPE_HAS_TRACK_MODE |
                            TELESCOPE_CAN_CONTROL_TRACK |
                            TELESCOPE_HAS_TRACK_RATE |
                            TELESCOPE_HAS_PIER_SIDE,
                            4 );
    SetParkDataType(PARK_HA_DEC);
}

/****************************************************************
**
**
*****************************************************************/
AUDTELESCOPE::~AUDTELESCOPE()
{
    //    zmq_ctx_term(context);
}

/****************************************************************
**
**
*****************************************************************/
bool AUDTELESCOPE::initProperties()
{
    INDI::Telescope::initProperties();

    ConfigTP[0].fill("ADDRESS", "Address", "127.0.0.1");
    ConfigTP.fill(getDeviceName(), "DEVICE_ADDRESS", "Server", CONNECTION_TAB, IP_RW, 60, IPS_IDLE);

    MountModeSP[MM_EQUATORIAL].fill("MOUNT_EQUATORIAL", "Equatorial", ISS_OFF);
    MountModeSP[MM_ALTAZ].fill("MOUNT_ALTAZ", "AltAz", ISS_OFF);
    MountModeSP.fill(getDeviceName(), "MOUNT_TYPE", "Mount Type", MAIN_CONTROL_TAB, IP_RO, ISR_1OFMANY, 0, IPS_IDLE);

    LocalEqNP[LEQ_HA].fill("HA", "HA (hh:mm:ss)", "%010.6m", -12, 12, 0, 0);
    LocalEqNP[LEQ_DEC].fill("DEC", "DEC (dd:mm:ss)", "%010.6m", -90, 90, 0, 0);
    LocalEqNP.fill(getDeviceName(), "LOCAL_EQUATORIAL_EOD_COORD", "Local Eq. Coordinates", MAIN_CONTROL_TAB, IP_RO, 60,
                   IPS_IDLE);

    AltAzNP[ALTAZ_AZ].fill("Az", "Az (deg)", "%.2f", -180, 180, 0, 0);
    AltAzNP[ALTAZ_ALT].fill("Alt", "Alt (deg)", "%.2f", -90, 90, 0, 0);
    AltAzNP.fill(getDeviceName(), "AZALT_EOD_COORD", "Azimuthal Coordinates", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    HomeSP[HOME_SYNC].fill("SYNCHOME", "Sync Home position", ISS_OFF);
    HomeSP[HOME_SLEW].fill("SLEWHOME", "Slew to Home position", ISS_OFF);
    HomeSP.fill(getDeviceName(), "TELESCOPE_HOME", "Home", SITE_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    TTimeNP[TTIME_JD].fill("JD", "JD (days)", "%.6f", 0, 0, 0, 0);
    TTimeNP[TTIME_UTC].fill("UTC", "UTC (hh:mm:ss)", "%09.6m", 0, 24, 0, 0);
    TTimeNP[TTIME_LST].fill("LST", "LST (hh:mm:ss)", "%09.6m", 0, 24, 0, 0);
    TTimeNP.fill(getDeviceName(), "TELESCOPE_TIME", "Time", SITE_TAB, IP_RO, 60, IPS_IDLE);

    ParkOptionSP[PARK_CURRENT].fill("PARK_CURRENT", "Set Park (Current)", ISS_OFF);
    ParkOptionSP[PARK_DEFAULT].fill("PARK_DEFAULT", "Restore Park (Default)", ISS_OFF);
    ParkOptionSP.fill(getDeviceName(), "TELESCOPE_PARK_OPTION", "Park Options", SITE_TAB, IP_RW, ISR_ATMOST1, 60, IPS_IDLE);

    // Since we have 4 slew rates, let's fill them out
    SlewRateSP[SLEW_GUIDE].fill("SLEW_GUIDE", "Guide", ISS_OFF);
    SlewRateSP[SLEW_CENTERING].fill("SLEW_CENTER", "Center", ISS_OFF);
    SlewRateSP[SLEW_FIND].fill("SLEW_FIND", "Find", ISS_OFF);
    SlewRateSP[SLEW_MAX].fill( "SLEW_MAX", "Max", ISS_ON);
    SlewRateSP.fill(getDeviceName(), "TELESCOPE_SLEW_RATE", "Slew Rate", MOTION_TAB, IP_RW,
                    ISR_1OFMANY, 60, IPS_IDLE);

    // Add Tracking Modes. If you have SOLAR, LUNAR..etc, add them here as well.
    AddTrackMode("TRACK_SIDEREAL", "Sidereal", true);
    AddTrackMode("TRACK_SOLAR", "Solar");
    AddTrackMode("TRACK_LUNAR", "Lunar");
    AddTrackMode("TRACK_CUSTOM", "Custom");

    // Mount Meridian Flip
    MeridianFlipSP[MFLIP_ON].fill("FLIP_ON", "On", ISS_OFF);
    MeridianFlipSP[MFLIP_OFF].fill("FLIP_OFF", "Off", ISS_ON);
    MeridianFlipSP.fill(getDeviceName(), "MOUNT_MERIDIAN_FLIP", "Mount Meridian Flip", SITE_TAB, IP_RW, ISR_1OFMANY, 60,
                        IPS_IDLE);

    // Mount Meridian Flip HA
    MeridianFlipHANP[0].fill("FLIP_HA", "Flip HA (deg)", "%.2f", -30.0, 30.0, 0.1, 0.0);
    MeridianFlipHANP.fill(getDeviceName(), "MOUNT_MERIDIAN_FLIP_HA", "Mount Meridian Flip HA", SITE_TAB, IP_RW, 60, IPS_IDLE);

    // HW type
    HWTypeTP[0].fill("HW_TYPE", "Controller Type", "");
    HWTypeTP.fill(getDeviceName(), "HW_TYPE_INFO", "Type", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // HW info
    HWModelTP[0].fill("HW_MODEL", "Mount Model", "");
    HWModelTP.fill(getDeviceName(), "HW_MODEL_INFO", "Model", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // HW identifier
    HWIdentifierTP[0].fill("HW_IDENTIFIER", "HW Identifier", "");
    HWIdentifierTP.fill(getDeviceName(), "HW_IDENTIFIER_INFO", "Identifier", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // high level info
    HighLevelSWTP[HLSW_NAME].fill("HLSW_NAME", "Name", "");
    HighLevelSWTP[HLSW_VERSION].fill("HLSW_VERSION", "Version", "--");
    HighLevelSWTP.fill(getDeviceName(), "HLSW_INFO", "HighLevel SW", INFO_TAB, IP_RO, 60, IPS_IDLE);

    // low level SW info
    LowLevelSWTP[LLSW_NAME].fill("LLSW_NAME", "Name", "");
    LowLevelSWTP[LLSW_VERSION].fill("LLSW_VERSION", "Version", "--");
    LowLevelSWTP.fill(getDeviceName(), "LLSW_INFO", "LowLevel SW", INFO_TAB, IP_RO, 60, IPS_IDLE);

    TrackState = SCOPE_IDLE;
    previousTrackState = SCOPE_IDLE;
    fTracking = false;
    fFirstTime = true;
    lastErrorMsg = NULL;

    //    SetTrackMode(TRACK_SIDEREAL);
    trackspeedra = TRACKRATE_SIDEREAL;
    trackspeeddec = 0;

    GI::initProperties(GUIDE_TAB);
    setDriverInterface(getDriverInterface() | GUIDER_INTERFACE);

    addDebugControl();
    addConfigurationControl();

    pthread_mutex_init( &connectionmutex, NULL );

    return true;
}

/****************************************************************
**
**
*****************************************************************/
void AUDTELESCOPE::ISGetProperties(const char *dev)
{
    INDI::Telescope::ISGetProperties(dev);

    defineProperty(ConfigTP);
    loadConfig(true, ConfigTP.getName());
}

/****************************************************************
**
**
*****************************************************************/
bool AUDTELESCOPE::updateProperties()
{
    if (isConnected())
    {
        defineProperty(HomeSP);
    }
    else
    {
        deleteProperty(HomeSP.getName());
    }

    INDI::Telescope::updateProperties();
    if (isConnected())
    {
        defineProperty(MountModeSP);

        defineProperty(LocalEqNP);
        defineProperty(AltAzNP);
        defineProperty(TTimeNP);
        defineProperty(MeridianFlipSP);
        defineProperty(MeridianFlipHANP);
        defineProperty(HWTypeTP);
        defineProperty(HWModelTP);
        defineProperty(HWIdentifierTP);
        defineProperty(HighLevelSWTP);
        defineProperty(LowLevelSWTP);
    }
    else
    {
        deleteProperty(MountModeSP);

        deleteProperty(LocalEqNP);
        deleteProperty(AltAzNP);
        deleteProperty(TTimeNP);
        deleteProperty(MeridianFlipSP);
        deleteProperty(MeridianFlipHANP);
        deleteProperty(HWTypeTP);
        deleteProperty(HWModelTP);
        deleteProperty(HWIdentifierTP);
        deleteProperty(HighLevelSWTP);
        deleteProperty(LowLevelSWTP);
    }

    GI::updateProperties();

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool AUDTELESCOPE::Connect()
{
    char *answer;
    char addr[1024];
    int timeout = 500;


    if (isConnected())
        return true;

    IPaddress = strdup(ConfigTP[0].text);

    DEBUGF(INDI::Logger::DBG_SESSION, "Attempting to connect %s telescope...", IPaddress);

    requester = zmq_socket(context, ZMQ_REQ);
    zmq_setsockopt(requester, ZMQ_RCVTIMEO, &timeout, sizeof(timeout) );
    snprintf( addr, sizeof(addr), "tcp://%s:%d", IPaddress, IPport );
    zmq_connect(requester, addr);

    answer = sendRequest("ASTRO_INFO");
    if ( answer )
    {
        json j;
        std::string sHWt, sHWm, sHWi, sLLSW, sLLSWv, sHLSW, sHLSWv;

        j = json::parse(answer, nullptr, false);
        free(answer);
        if ( j.is_discarded() ||
                !j.contains("HWType") ||
                !j.contains("HWModel") ||
                !j.contains("HWIdentifier") ||
                !j.contains("lowLevelSW") ||
                !j.contains("lowLevelSWVersion") ||
                !j.contains("highLevelSW") ||
                !j.contains("highLevelSWVersion") )
        {
            zmq_close(requester);
            DEBUGF(INDI::Logger::DBG_ERROR, "Communication with %s telescope failed", IPaddress);
            free(IPaddress);
            return false;
        }

        j["HWType"].get_to(sHWt);
        HWTypeTP[0].setText(sHWt);
        HWTypeTP.apply();
        j["HWModel"].get_to(sHWm);
        HWModelTP[0].setText(sHWm);
        HWModelTP.apply();
        j["HWIdentifier"].get_to(sHWi);
        HWIdentifierTP[0].setText(sHWi);
        HWIdentifierTP.apply();
        j["lowLevelSW"].get_to(sLLSW);
        LowLevelSWTP[LLSW_NAME].setText(sLLSW);
        j["lowLevelSWVersion"].get_to(sLLSWv);
        LowLevelSWTP[LLSW_VERSION].setText(sLLSWv);
        LowLevelSWTP.apply();
        j["highLevelSW"].get_to(sHLSW);
        HighLevelSWTP[HLSW_NAME].setText(sHLSW);
        j["highLevelSWVersion"].get_to(sHLSWv);
        HighLevelSWTP[HLSW_VERSION].setText(sHLSWv);
        HighLevelSWTP.apply();
    }
    else
    {
        zmq_close(requester);
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect %s telescope", IPaddress);
        free(IPaddress);
        return false;
    }

    answer = sendRequest("ASTRO_GETMERIDIANFLIPHA");
    if ( answer && !strncmp(answer, "OK:", 3) )
    {
        MeridianFlipHANP[0].value = atof(answer + 3);
        free( answer );
        MeridianFlipHANP.apply();
    }
    else
    {
        zmq_close(requester);
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect %s telescope", IPaddress);
        free(IPaddress);
        return false;
    }

    answer = sendRequest("ASTRO_GETMOUNTMODE");
    if ( answer && !strncmp(answer, "OK:", 3) )
    {
        if ( !strcmp(answer, "OK:ALTAZ") )
            mounttype = MM_ALTAZ;
        else
            mounttype = MM_EQUATORIAL;
        free( answer );
        MountModeSP[mounttype].setState(ISS_ON);
        MountModeSP[(mounttype ? 0 : 1)].setState(ISS_OFF);
        MountModeSP.setState(IPS_OK);
        MountModeSP.setPermission(IP_RO);
        MountModeSP.apply();
    }
    else
    {
        zmq_close(requester);
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect %s telescope", IPaddress);
        free(IPaddress);
        return false;
    }

    answer = sendRequest("ASTRO_GETLOCATION");
    if ( answer )
    {
        json j;

        j = json::parse(answer, nullptr, false);
        free(answer);
        if ( j.is_discarded() ||
                !j.contains("longitude") ||
                !j.contains("latitude") ||
                !j.contains("elevation") )
        {
            zmq_close(requester);
            DEBUGF(INDI::Logger::DBG_ERROR, "Communication with %s telescope failed", IPaddress);
            free(IPaddress);
            return false;
        }
        j["longitude"].get_to(LocationNP[LOCATION_LONGITUDE].value);
        j["latitude"].get_to(LocationNP[LOCATION_LATITUDE].value);
        j["elevation"].get_to(LocationNP[LOCATION_ELEVATION].value);
        LocationNP.apply();
    }
    else
    {
        zmq_close(requester);
        DEBUGF(INDI::Logger::DBG_ERROR, "Failed to connect %s telescope", IPaddress);
        free(IPaddress);
        return false;
    }

    northernHemisphere = 1;

    slewState = IPS_IDLE;

    tid = SetTimer(getCurrentPollingPeriod());

    DEBUGF(INDI::Logger::DBG_SESSION, "Successfully connected %s telescope", IPaddress);
    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool AUDTELESCOPE::Disconnect()
{
    if (!isConnected())
        return true;

    DEBUG(INDI::Logger::DBG_SESSION, "Attempting to disconnect telescope...");

    zmq_close(requester);

    RemoveTimer( tid );

    free(IPaddress);

    DEBUG(INDI::Logger::DBG_SESSION, "Successfully disconnected telescope");

    return true;
}

/****************************************************************
**
**
*****************************************************************/
bool AUDTELESCOPE::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if (!strcmp(dev, getDeviceName()))
    {
        // TCP Server settings
        if (ConfigTP.isNameMatch(name))
        {
            if ( isConnected() && strcmp(IPaddress, texts[0]) )
            {
                DEBUG(INDI::Logger::DBG_WARNING, "Please Disconnect before changing IP address");
                return false;
            }
            ConfigTP.update(texts, names, n);
            ConfigTP.setState(IPS_OK);
            ConfigTP.apply();
            return true;
        }
    }

    return INDI::Telescope::ISNewText(dev, name, texts, names, n);
}

bool AUDTELESCOPE::ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    // Check guider interface
    if (GI::processNumber(dev, name, values, names, n))
        return true;

    //  first check if it's for our device
    if(!strcmp(dev, getDeviceName()))
    {

        // Meridian Flip HourAngle
        if(MeridianFlipHANP.isNameMatch(name))
        {
            MeridianFlipHANP.setState(IPS_BUSY);
            MeridianFlipHANP.update(values, names, n);

            if ( isConnected() )
            {
                if ( setMeridianFlipHA(values[0]) )
                    MeridianFlipHANP.setState(IPS_OK);
                else
                    MeridianFlipHANP.setState(IPS_ALERT);
            }
            MeridianFlipHANP.apply();
            return true;
        }
    }

    return INDI::Telescope::ISNewNumber(dev, name, values, names, n);
}

bool AUDTELESCOPE::ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(!strcmp(dev, getDeviceName()))
    {

        // Home
        if (HomeSP.isNameMatch(name))
        {
            HomeSP.update(states, names, n);
            int index = HomeSP.findOnSwitchIndex();

            HomeSP.setState(IPS_BUSY);
            HomeSP.reset();
            HomeSP.apply();

            if ( isConnected() )
            {
                switch (index)
                {
                    case HOME_SYNC :
                        if ( SyncHome() )
                            HomeSP.setState(IPS_OK);
                        else
                            HomeSP.setState(IPS_ALERT);
                        break;
                    case HOME_SLEW :
                        if ( SlewToHome() )
                            HomeSP.setState(IPS_OK);
                        else
                            HomeSP.setState(IPS_ALERT);
                        break;
                }
            }
            HomeSP.apply();
            return true;
        }

        // Meridian flip
        if (MeridianFlipSP.isNameMatch(name))
        {
            MeridianFlipSP.setState(IPS_BUSY);
            MeridianFlipSP.update(states, names, n);

            if ( isConnected() )
            {
                int targetState = MeridianFlipSP.findOnSwitchIndex();
                if ( meridianFlipEnable((targetState == 0) ? true : false) )
                    MeridianFlipSP.setState(IPS_OK);
                else
                    MeridianFlipSP.setState(IPS_ALERT);
            }
            MeridianFlipSP.apply();
            return true;
        }
    }

    return INDI::Telescope::ISNewSwitch(dev, name, states, names, n);
}

bool AUDTELESCOPE::updateLocation(double latitude, double longitude, double elevation)
{
    char *answer;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Location update called before driver connection");
        return false;
    }

    DEBUG(INDI::Logger::DBG_SESSION, "Location update ..." );
    answer = sendCommand("ASTRO_SETLOCATION %.8f %.8f %.1f", longitude, latitude, elevation);
    if ( !answer )
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Location update completed" );
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING, "Location update failed due to %s", answer );
    free(answer);
    return false;
}

bool AUDTELESCOPE::updateTime(ln_date *utc, double utc_offset)
{
    char *answer, buffer[256];

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Time update called before driver connection");
        return false;
    }

    INDI_UNUSED(utc_offset);

    snprintf( buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%08.6fZ", utc->years, utc->months, utc->days, utc->hours,
              utc->minutes, utc->seconds );
    DEBUGF(INDI::Logger::DBG_SESSION, "Time update to %s ...", buffer );
    answer = sendCommandOnce("ASTRO_SETUTCDATE %s", buffer);
    if ( !answer )
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Time update completed" );
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING, "Time update to %s failed due to %s", buffer, answer );
    free(answer);
    return false;
}

bool AUDTELESCOPE::ReadScopeStatus()
{
    char *answer;
    int sts, pierside, exposureready, meridianflip;
    double utc, lst, jd, ha, ra, dec, az, alt, meridianflipha;


    answer = sendRequest("ASTRO_STATUS");
    if ( answer )
    {
        json j;

        j = json::parse(answer, nullptr, false);
        free(answer);
        if ( j.is_discarded() ||
                !j.contains("UTC") ||
                !j.contains("JD") ||
                !j.contains("LST") ||
                !j.contains("HA") ||
                !j.contains("RA") ||
                !j.contains("Dec") ||
                !j.contains("Az") ||
                !j.contains("Alt") ||
                !j.contains("globalStatus") ||
                !j.contains("meridianFlip") ||
                !j.contains("pierSide") ||
                !j.contains("meridianFlipHA") ||
                !j.contains("exposureReady") )
        {
            DEBUG(INDI::Logger::DBG_WARNING, "Status communication error");
            return false;
        }
        j["UTC"].get_to(utc);
        j["JD"].get_to(jd);
        j["LST"].get_to(lst);
        j["HA"].get_to(ha);
        j["RA"].get_to(ra);
        j["Dec"].get_to(dec);
        j["Az"].get_to(az);
        j["Alt"].get_to(alt);
        j["globalStatus"].get_to(sts);
        j["meridianFlip"].get_to(meridianflip);
        j["pierSide"].get_to(pierside);
        j["meridianFlipHA"].get_to(meridianflipha);
        j["exposureReady"].get_to(exposureready);
        if ( j.contains("errorMsg") )
        {
            std::string msg;
            j["errorMsg"].get_to(msg);
            if ( msg.length() > 0 )
            {
                if ( !lastErrorMsg || ( lastErrorMsg && strcmp(msg.c_str(), lastErrorMsg) ) )
                {
                    // the error message is written only once until it changes
                    DEBUGF(INDI::Logger::DBG_WARNING, "Failed due to %s", msg.c_str());
                    if ( lastErrorMsg )
                        free( lastErrorMsg );
                    lastErrorMsg = strdup(msg.c_str());
                }
            }
            else
            {
                if ( lastErrorMsg )
                    free( lastErrorMsg );
                lastErrorMsg = NULL;
            }
        }
        else
        {
            if ( lastErrorMsg )
                free( lastErrorMsg );
            lastErrorMsg = NULL;
        }

        previousTrackState = TrackState;

        NewRaDec( ra, dec );
        switch ( sts )
        {
            case 0 :
                TrackState = SCOPE_IDLE;
                break;
            case 1 :
                TrackState = SCOPE_SLEWING;
                break;
            case 2 :
                TrackState = SCOPE_TRACKING;
                slewState = IPS_IDLE;
                break;
            case 3 :
                TrackState = SCOPE_PARKING;
                break;
            case 4 :
                TrackState = SCOPE_PARKED;
                break;
        }

        if ( fFirstTime )
        {
            SetParked( (TrackState == SCOPE_PARKED) );
            fFirstTime = false;
        }
        else if ( previousTrackState != TrackState )
        {
            if ( TrackState == SCOPE_PARKED )
                SetParked(true);
            else if ( previousTrackState == SCOPE_PARKED )
                SetParked(false);
        }

        if ( pierside >= 0 )
        {
            if ( meridianflip && ( MeridianFlipSP[MFLIP_ON].getState() == ISS_OFF ) )
            {
                MeridianFlipSP[MFLIP_ON].setState(ISS_ON);
                MeridianFlipSP[MFLIP_OFF].setState(ISS_OFF);
            }
            if ( !meridianflip && ( MeridianFlipSP[MFLIP_ON].getState() == ISS_ON ) )
            {
                MeridianFlipSP[MFLIP_ON].setState(ISS_OFF);
                MeridianFlipSP[MFLIP_OFF].setState(ISS_ON);
            }
        }
        else
        {
            MeridianFlipSP[MFLIP_ON].setState(ISS_OFF);
            MeridianFlipSP[MFLIP_OFF].setState(ISS_ON);
        }
        MeridianFlipSP.apply();

        MeridianFlipHANP[0].value = meridianflipha;
        MeridianFlipHANP.apply();
        setPierSide((TelescopePierSide)pierside);

        if (LocalEqNP[LEQ_HA].value != ha || LocalEqNP[LEQ_DEC].value != dec || LocalEqNP.getState() != EqNP.getState())
        {
            LocalEqNP[LEQ_HA].value = ha;
            LocalEqNP[LEQ_DEC].value = dec;
            LocalEqNP.setState(slewState);
            LocalEqNP.apply();
        }

        if (AltAzNP[ALTAZ_AZ].value != az || AltAzNP[ALTAZ_ALT].value != alt || AltAzNP.getState() != EqNP.getState())
        {
            AltAzNP[ALTAZ_AZ].value = az;
            AltAzNP[ALTAZ_ALT].value = alt;
            AltAzNP.setState(slewState);
            AltAzNP.apply();
        }

        if (TTimeNP[TTIME_JD].value != utc || TTimeNP[TTIME_UTC].value != jd || TTimeNP[TTIME_LST].value != lst)
        {
            TTimeNP[TTIME_JD].value = jd;
            TTimeNP[TTIME_UTC].value = utc;
            TTimeNP[TTIME_LST].value = lst;
            TTimeNP.setState(IPS_OK);
            TTimeNP.apply();
        }

        return true;
    }

    return false;
}

bool AUDTELESCOPE::meridianFlipEnable(int enable)
{
    char *answer;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Set meridian flip called before driver connection");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_SESSION, "Set meridian flip to %s ...", (enable ? "ENABLED" : "DISABLED"));
    answer = sendCommand("ASTRO_SETMERIDIANFLIP %d", enable);
    if ( !answer )
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Set meridian flip to %s completed", (enable ? "ENABLED" : "DISABLED"));
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING, "Set meridian flip to %s failed due to %s", (enable ? "ENABLED" : "DISABLED"), answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::setMeridianFlipHA(double angle)
{
    char *answer;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Set meridian flip HA called before driver connection");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_SESSION, "Set meridian flip HA to %.3fdeg ...", angle);
    answer = sendCommand("ASTRO_SETMERIDIANFLIPHA %.4f", angle);
    if ( !answer )
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Set meridian flip HA to %.3fdeg completed", angle);
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING, "Set meridian flip HA to %.3fdeg failed due to %s", angle, answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::Sync(double ra, double dec)
{
    char *answer;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Sync called before driver connection");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_SESSION, "Sync to RA:%.3fhours Dec:%.3fdeg ...", ra, dec);
    answer = sendCommand("ASTRO_SYNC %.8f %.8f", ra, dec);
    if ( !answer )
    {
        DEBUGF(INDI::Logger::DBG_SESSION, "Sync to RA:%.3fhours Dec:%.3fdeg completed", ra, dec);
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING, "Sync to RA:%.3fhours Dec:%.3fdeg failed due to %s", ra, dec, answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::Park()
{
    char *answer;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Start telescope park called before driver connection");
        return false;
    }

    TrackState = SCOPE_PARKING;
    LOG_INFO("Start telescope park...");
    answer = sendCommand("ASTRO_PARK");
    if ( !answer )
    {
        ParkSP.setState(IPS_BUSY);
        ParkSP.apply();
        TrackState = SCOPE_PARKING;
        DEBUG(INDI::Logger::DBG_SESSION, "Start telescope park completed");
        return true;
    }
    ParkSP.setState(IPS_ALERT);
    ParkSP.apply();
    TrackState = SCOPE_IDLE;
    DEBUGF(INDI::Logger::DBG_WARNING, "Start telescope park failed due to %s", answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::UnPark()
{
    char *answer;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Unparking telescope called before driver connection");
        return false;
    }

    LOG_INFO("Unparking telescope...");
    answer = sendCommand("ASTRO_UNPARK");
    if ( !answer )
    {
        SetParked( false );
        DEBUG(INDI::Logger::DBG_SESSION, "Unparking telescope completed");
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING, "Unparking telescope failed due to %s", answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::SetCurrentPark()
{
    char *answer;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Set park position called before driver connection");
        return false;
    }

    LOG_INFO("Set park position...");
    answer = sendCommand("ASTRO_SETPARK");
    if ( !answer )
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Set park position completed");
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING, "Set park position failed due to %s", answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::SetDefaultPark()
{
    char *answer;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Restore park position called before driver connection");
        return false;
    }

    LOG_INFO("Restore park position...");
    answer = sendCommand("ASTRO_RESTOREPARK");
    if ( !answer )
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Restore park position completed");
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING, "Restore park position failed due to %s", answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::SyncHome()
{
    char *answer;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Sync home position called before driver connection");
        return false;
    }

    LOG_INFO("Sync home position...");
    answer = sendCommand("ASTRO_SYNCHOME");
    if ( !answer )
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Sync home position completed");
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING, "Sync home position failed due to %s", answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::SlewToHome()
{
    char *answer;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Slew to home position called before driver connection");
        return false;
    }

    LOG_INFO("Start slew to home position...");
    answer = sendCommand("ASTRO_POINTHOME");
    if ( !answer )
    {
        DEBUG(INDI::Logger::DBG_SESSION, "Start slew to home position completed");
        return true;
    }
    DEBUGF(INDI::Logger::DBG_WARNING, "Start slew to home position failed due to %s", answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::Goto(double ra, double dec)
{
    if (CoordSP.isSwitchOn("TRACK"))
    {
        return Slew(ra, dec, true);
    }
    else
    {
        return Slew(ra, dec, false);
    }
}

bool AUDTELESCOPE::Slew(double ra, double dec, int track)
{
    char *answer = NULL;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Start telescope slew called before driver connection");
        return false;
    }

    slewState = IPS_BUSY;
    DEBUGF(INDI::Logger::DBG_SESSION, "Start telescope slew to RA:%.4fhours Dec:%.3fdeg and %stracking ...", ra, dec,
           (track ? "" : "NO "));
    if ( track )
    {
        // point and track
        answer = sendCommand("ASTRO_POINT %.8f %.8f %.8f %.8f", ra, dec, trackspeedra / 3600.0, trackspeeddec / 3600.0);
    }
    else
    {
        // point only
        answer = sendCommand("ASTRO_POINT %.8f %.8f 0 0", ra, dec);
    }
    if ( !answer )
    {
        targetRA = ra;
        targetDEC = dec;
        TrackState = SCOPE_SLEWING;
        slewState = IPS_OK;
        DEBUGF(INDI::Logger::DBG_SESSION, "Start telescope slew to RA:%.4fhours Dec:%.3fdeg and %stracking completed", ra, dec,
               (track ? "" : "NO "));
        return true;
    }
    TrackState = SCOPE_IDLE;
    slewState = IPS_ALERT;
    DEBUGF(INDI::Logger::DBG_WARNING, "Start telescope slew to RA:%.4fhours Dec:%.3fdeg and %stracking failed due to %s", ra,
           dec, (track ? "" : "NO "), answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::SetTrackMode(uint8_t mode)
{
    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Set tracking mode called before driver connection");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_SESSION, "Set tracking mode to %d...", mode);
    switch (mode)
    {
        case TRACK_SIDEREAL:
            trackspeedra = TRACKRATE_SIDEREAL;
            trackspeeddec = 0;
            break;
        case TRACK_SOLAR:
            trackspeedra = TRACKRATE_SOLAR;
            trackspeeddec = 0;
            break;
        case TRACK_LUNAR:
            trackspeedra = TRACKRATE_LUNAR;
            trackspeeddec = 0;
            break;
        case TRACK_CUSTOM:
            trackspeedra = TrackRateNP[AXIS_RA].getValue();
            trackspeeddec = TrackRateNP[AXIS_DE].getValue();
            break;
    }
    if ( TrackState == SCOPE_TRACKING )
        SetTrackRate( trackspeedra, trackspeeddec );
    DEBUGF(INDI::Logger::DBG_SESSION, "Set tracking mode to %d completed", mode);

    return true;
}

bool AUDTELESCOPE::SetTrackRate(double raRate, double deRate)
{
    char *answer;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Tracking change called before driver connection");
        return false;
    }

    DEBUGF(INDI::Logger::DBG_SESSION, "Tracking change to RA:%f\"/s Dec:%f\"/s ...", raRate, deRate);
    TrackStateSP.setState(IPS_BUSY);
    answer = sendCommand("ASTRO_TRACK %.8f %.8f", raRate / 3600.0, deRate / 3600.0);
    if ( !answer )
    {
        if ( ( raRate == 0 ) && ( deRate == 0 ) )
            TrackStateSP.setState(IPS_IDLE);
        else
            TrackStateSP.setState(IPS_OK);
        DEBUGF(INDI::Logger::DBG_SESSION, "Tracking change to RA:%f\"/s Dec:%f\"/s completed", raRate, deRate);
        return true;
    }
    TrackStateSP.setState(IPS_ALERT);
    DEBUGF(INDI::Logger::DBG_WARNING, "Tracking change to RA:%f\"/s Dec:%f\"/s failed due to %s", raRate, deRate, answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::SetTrackEnabled(bool enabled)
{
    int rc;

    DEBUGF(INDI::Logger::DBG_SESSION, "Change tracking to %s...", ((enabled) ? "ENABLED" : "DISABLED"));
    if ( enabled )
    {
        int mode = TrackModeSP.findOnSwitchIndex();
        SetTrackMode(mode);
        if ( TrackState != SCOPE_TRACKING )
            rc = SetTrackRate( trackspeedra, trackspeeddec );
        else
            rc = true;
    }
    else
    {
        rc = SetTrackRate( 0, 0 );
    }
    DEBUGF(INDI::Logger::DBG_SESSION, "Change tracking to %s completed", ((enabled) ? "ENABLED" : "DISABLED"));
    return rc;
}

bool AUDTELESCOPE::MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand command)
{
    char *answer = NULL;
    string speed[] = {"SLEWGUIDE", "SLEWCENTER", "SLEWFIND", "SLEWMAX"};
    int speedIndex;


    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "MoveNS called before driver connection");
        return false;
    }

    speedIndex = SlewRateSP.findOnSwitchIndex();
    MovementNSSP.setState(IPS_BUSY);
    if ( command == MOTION_START )
    {
        // force tracking after motion
        fTracking = true;
        if ( dir == DIRECTION_NORTH )
            answer = sendCommand("ASTRO_SLEW * (%.8f+%s)", trackspeeddec / 3600.0, speed[speedIndex].c_str());
        else
            answer = sendCommand("ASTRO_SLEW * (%.8f-%s)", trackspeeddec / 3600.0, speed[speedIndex].c_str());
    }
    else
    {
        if ( fTracking )
            answer = sendCommand("ASTRO_TRACK * %.8f", trackspeeddec / 3600.0);
        else
            answer = sendCommand("ASTRO_SLEW * 0");
    }
    if ( !answer )
    {
        if ( command == MOTION_START )
            MovementNSSP.setState(IPS_OK);
        else
            MovementNSSP.setState(IPS_IDLE);
        return true;
    }
    MovementNSSP.setState(IPS_ALERT);
    DEBUGF(INDI::Logger::DBG_WARNING, "MoveNS command failed due to %s", answer);
    free(answer);
    return false;
}

bool AUDTELESCOPE::MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand command)
{
    char *answer = NULL;
    string speed[] = {"SLEWGUIDE", "SLEWCENTER", "SLEWFIND", "SLEWMAX"};
    int speedIndex;


    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "MoveWE called before driver connection");
        return false;
    }

    speedIndex = SlewRateSP.findOnSwitchIndex();
    MovementWESP.setState(IPS_BUSY);
    if ( command == MOTION_START )
    {
        // force tracking after motion
        fTracking = true;
        if ( dir == DIRECTION_WEST )
            answer = sendCommand("ASTRO_SLEW (%.8f+%s) *", trackspeedra / 3600.0, speed[speedIndex].c_str());
        else
            answer = sendCommand("ASTRO_SLEW (%.8f-%s) *", trackspeedra / 3600.0, speed[speedIndex].c_str());
    }
    else
    {
        if ( fTracking )
            answer = sendCommand("ASTRO_TRACK %.8f *", trackspeedra / 3600.0);
        else
            answer = sendCommand("ASTRO_SLEW 0 *");
    }
    if ( !answer )
    {
        if ( command == MOTION_START )
            MovementWESP.setState(IPS_OK);
        else
            MovementWESP.setState(IPS_IDLE);
        return true;
    }
    MovementWESP.setState(IPS_ALERT);
    DEBUGF(INDI::Logger::DBG_WARNING, "MoveWE command failed due to %s", answer);
    free(answer);
    return false;
}

IPState AUDTELESCOPE::GuideNorth(uint32_t ms)
{
    char *answer = NULL;
    IPState rc;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "GuideNorth called before driver connection");
        return IPS_ALERT;
    }

    if ( ms == 0 )
        return IPS_OK;

    answer = sendCommand("ASTRO_GUIDE * %u", ms);
    if ( !answer )
    {
        rc = IPS_OK;
    }
    else
    {
        rc = IPS_ALERT;
        DEBUGF(INDI::Logger::DBG_WARNING, "GuideNorth command failed due to %s", answer);
        free(answer);
    }
    GuideComplete(INDI_EQ_AXIS::AXIS_DE);
    return rc;
}

IPState AUDTELESCOPE::GuideSouth(uint32_t ms)
{
    char *answer = NULL;
    IPState rc;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "GuideSouth called before driver connection");
        return IPS_ALERT;
    }

    if ( ms == 0 )
        return IPS_OK;

    answer = sendCommand("ASTRO_GUIDE * -%u", ms);
    if ( !answer )
    {
        rc = IPS_OK;
    }
    else
    {
        rc = IPS_ALERT;
        DEBUGF(INDI::Logger::DBG_WARNING, "GuideSouth command failed due to %s", answer);
        free(answer);
    }
    GuideComplete(INDI_EQ_AXIS::AXIS_DE);
    return rc;
}

IPState AUDTELESCOPE::GuideEast(uint32_t ms)
{
    char *answer = NULL;
    IPState rc;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "GuideEast called before driver connection");
        return IPS_ALERT;
    }

    if ( ms == 0 )
        return IPS_OK;

    answer = sendCommand("ASTRO_GUIDE %u *", ms);
    if ( !answer )
    {
        rc = IPS_OK;
    }
    else
    {
        rc = IPS_ALERT;
        DEBUGF(INDI::Logger::DBG_WARNING, "GuideEast command failed due to %s", answer);
        free(answer);
    }
    GuideComplete(INDI_EQ_AXIS::AXIS_RA);
    return rc;
}

IPState AUDTELESCOPE::GuideWest(uint32_t ms)
{
    char *answer = NULL;
    IPState rc;

    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "GuideWest called before driver connection");
        return IPS_ALERT;
    }

    if ( ms == 0 )
        return IPS_OK;

    answer = sendCommand("ASTRO_GUIDE -%u *", ms);
    if ( !answer )
    {
        rc = IPS_OK;
    }
    else
    {
        rc = IPS_ALERT;
        DEBUGF(INDI::Logger::DBG_WARNING, "GuideWest command failed due to %s", answer);
        free(answer);
    }
    GuideComplete(INDI_EQ_AXIS::AXIS_RA);
    return rc;
}

bool AUDTELESCOPE::Abort()
{
    if (!isConnected())
    {
        DEBUG(INDI::Logger::DBG_WARNING, "Abort called before driver connection");
        return false;
    }

    AbortSP.setState(IPS_OK);

    DEBUG(INDI::Logger::DBG_SESSION, "Telescope abort ...");

    if (isConnected())
    {
        char *answer;
        answer = sendCommand("ASTRO_STOP");
        free(answer);
    }

    AbortSP.setState(IPS_IDLE);
    AbortSP.reset();
    AbortSP.apply();

    slewState = IPS_IDLE;

    DEBUG(INDI::Logger::DBG_SESSION, "Telescope abort completed");

    return true;
}

void AUDTELESCOPE::TimerHit()
{
    if (isConnected() == false)
        return;

    ReadScopeStatus();
    EqNP.apply();

    SetTimer(getCurrentPollingPeriod());
}

bool AUDTELESCOPE::saveConfigItems(FILE *fp)
{
    ConfigTP.save(fp);
    MeridianFlipSP.save(fp);
    MeridianFlipHANP.save(fp);

    return INDI::Telescope::saveConfigItems(fp);
}

const char *AUDTELESCOPE::getDefaultName()
{
    return device_str;
}

char* AUDTELESCOPE::sendCommand(const char *fmt, ...)
{
    va_list ap;
    char buffer[4096], answer[4096], addr[1024];
    int rc, retries;
    zmq_pollitem_t item;

    va_start( ap, fmt );
    vsnprintf( buffer, sizeof(buffer), fmt, ap );
    va_end( ap );

    pthread_mutex_lock( &connectionmutex );
    retries = 3;
    do
    {
        zmq_send(requester, buffer, strlen(buffer), 0);
        item = { requester, 0, ZMQ_POLLIN, 0 };
        rc = zmq_poll( &item, 1, 500 ); // ms
        if ( ( rc >= 0 ) && ( item.revents & ZMQ_POLLIN ) )
        {
            // communication succeeded
            rc = zmq_recv(requester, answer, sizeof(answer), 0);
            if ( rc >= 0 )
            {
                pthread_mutex_unlock( &connectionmutex );
                answer[MIN(rc, (int)sizeof(answer) - 1)] = '\0';
                if ( !strncmp(answer, "OK", 2) )
                    return NULL;
                if ( !strncmp(answer, "ERROR:", 6) )
                    return strdup(answer + 6);
                return strdup("SYNTAXERROR");
            }
        }
        zmq_close(requester);
        requester = zmq_socket(context, ZMQ_REQ);
        snprintf( addr, sizeof(addr), "tcp://%s:%d", IPaddress, IPport );
        zmq_connect(requester, addr);
    }
    while ( --retries );
    pthread_mutex_unlock( &connectionmutex );
    DEBUG(INDI::Logger::DBG_WARNING, "No answer from driver");
    return strdup("COMMUNICATIONERROR");
}

char* AUDTELESCOPE::sendCommandOnce(const char *fmt, ...)
{
    va_list ap;
    char buffer[4096], answer[4096], addr[1024];
    int rc;
    zmq_pollitem_t item;

    va_start( ap, fmt );
    vsnprintf( buffer, sizeof(buffer), fmt, ap );
    va_end( ap );

    pthread_mutex_lock( &connectionmutex );
    zmq_send(requester, buffer, strlen(buffer), 0);
    item = { requester, 0, ZMQ_POLLIN, 0 };
    rc = zmq_poll( &item, 1, 500 ); // ms
    if ( ( rc >= 0 ) && ( item.revents & ZMQ_POLLIN ) )
    {
        // communication succeeded
        rc = zmq_recv(requester, answer, sizeof(answer), 0);
        if ( rc >= 0 )
        {
            pthread_mutex_unlock( &connectionmutex );
            answer[MIN(rc, (int)sizeof(answer) - 1)] = '\0';
            if ( !strncmp(answer, "OK", 2) )
                return NULL;
            if ( !strncmp(answer, "ERROR:", 6) )
                return strdup(answer + 6);
            return strdup("SYNTAXERROR");
        }
    }
    zmq_close(requester);
    requester = zmq_socket(context, ZMQ_REQ);
    snprintf( addr, sizeof(addr), "tcp://%s:%d", IPaddress, IPport );
    zmq_connect(requester, addr);
    pthread_mutex_unlock( &connectionmutex );
    DEBUG(INDI::Logger::DBG_WARNING, "No answer from driver");
    return strdup("COMMUNICATIONERROR");
}

char* AUDTELESCOPE::sendRequest(const char *fmt, ...)
{
    va_list ap;
    char buffer[4096], answer[4096], addr[1024];
    int rc, retries;
    zmq_pollitem_t item;

    va_start( ap, fmt );
    vsnprintf( buffer, sizeof(buffer), fmt, ap );
    va_end( ap );

    pthread_mutex_lock( &connectionmutex );
    retries = 3;
    do
    {
        zmq_send(requester, buffer, strlen(buffer), 0);
        item = { requester, 0, ZMQ_POLLIN, 0 };
        rc = zmq_poll( &item, 1, 500 ); // ms
        if ( ( rc >= 0 ) && ( item.revents & ZMQ_POLLIN ) )
        {
            // communication succeeded
            rc = zmq_recv(requester, answer, sizeof(answer), 0);
            if ( rc >= 0 )
            {
                pthread_mutex_unlock( &connectionmutex );
                answer[MIN(rc, (int)sizeof(answer) - 1)] = '\0';
                return strdup(answer);
            }
        }
        zmq_close(requester);
        requester = zmq_socket(context, ZMQ_REQ);
        snprintf( addr, sizeof(addr), "tcp://%s:%d", IPaddress, IPport );
        zmq_connect(requester, addr);
    }
    while ( --retries );
    pthread_mutex_unlock( &connectionmutex );
    DEBUG(INDI::Logger::DBG_WARNING, "No answer from driver");
    return strdup("COMMUNICATIONERROR");
}
